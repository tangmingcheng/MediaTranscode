#include "internal/FFmpegAudioCopyPipeline.h"

#include "internal/FFmpegError.h"
#include "internal/FFmpegRAII.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/output/capabilities/audio/AudioOutputStreamProvider.h"

#include <sstream>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg {
namespace {

Status makeTimestampError(const std::string& message)
{
    return Status::failure(ErrorInfo::internalError(message));
}

} // namespace

void FFmpegAudioCopyPipeline::reset()
{
    m_inputAudioStream = nullptr;
    m_outputStreamProvider = nullptr;
    m_outputNode = nullptr;
    m_outputAudioStream = nullptr;
    m_timeline = nullptr;
    m_packetWriter.reset();
}

Status FFmpegAudioCopyPipeline::initialize(const FFmpegAudioPipelineConfig& config)
{
    reset();

    if (!config.inputAudioStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline initialize failed: inputAudioStream is null"));
    }

    if (!config.outputStreamProvider) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline initialize failed: outputStreamProvider is null"));
    }

    if (!config.outputNode) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline initialize failed: outputNode is null"));
    }

    if (!config.timeline) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline initialize failed: timeline is null"));
    }

    m_inputAudioStream = config.inputAudioStream;
    m_outputStreamProvider = config.outputStreamProvider;
    m_outputNode = config.outputNode;
    m_timeline = config.timeline;

    auto streamResult = m_outputStreamProvider->createAudioCopyStream(m_inputAudioStream);
    if (!streamResult) {
        return Status::failure(streamResult.error());
    }

    m_outputAudioStream = streamResult.value();

    FFmpegAudioPacketWriter::Config writerConfig;
    writerConfig.outputNode = m_outputNode;
    writerConfig.outputStream = m_outputAudioStream;
    writerConfig.timestampErrorPrefix = "audio packet";
    writerConfig.writeErrorMessage = "audio packet write failed";

    return m_packetWriter.initialize(std::move(writerConfig));
}

Status FFmpegAudioCopyPipeline::processPacket(
    AVPacket* packet,
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!isInitialized()) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioCopyPipeline processPacket failed: pipeline is not initialized"));
    }

    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline processPacket failed: packet is null"));
    }

    PacketPtr clonedPacket = makePacket();
    if (!clonedPacket) {
        return Status::failure(makeAllocationError(
            "FFmpegAudioCopyPipeline failed to allocate packet"));
    }

    const int refRet = av_packet_ref(clonedPacket.get(), packet);
    if (refRet < 0) {
        return Status::failure(makeFFmpegError(
            "FFmpegAudioCopyPipeline av_packet_ref failed", refRet));
    }

    const Status normalizeStatus = normalizePacketTimestamp(clonedPacket.get());
    if (!normalizeStatus) {
        return normalizeStatus;
    }

    return m_packetWriter.write(clonedPacket.get(), onPacketWritten);
}

Status FFmpegAudioCopyPipeline::flush(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    (void)onPacketWritten;
    return Status::success();
}

bool FFmpegAudioCopyPipeline::isInitialized() const
{
    return m_inputAudioStream &&
        m_outputStreamProvider &&
        m_outputNode &&
        m_outputAudioStream &&
        m_timeline &&
        m_packetWriter.isInitialized();
}

FFmpegAudioPipelineMode FFmpegAudioCopyPipeline::mode() const
{
    return FFmpegAudioPipelineMode::Copy;
}

AVStream* FFmpegAudioCopyPipeline::outputStream() const
{
    return m_outputAudioStream;
}

FFmpegAudioPacketProgress FFmpegAudioCopyPipeline::progress() const
{
    return FFmpegAudioPacketProgress{
        m_packetWriter.packetCount(),
        m_packetWriter.lastWrittenOutTimeMs()
    };
}

Status FFmpegAudioCopyPipeline::normalizePacketTimestamp(AVPacket* packet) const
{
    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline normalizePacketTimestamp failed: packet is null"));
    }

    if (!m_timeline || !m_inputAudioStream || !m_outputAudioStream) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioCopyPipeline normalizePacketTimestamp failed: pipeline is not initialized"));
    }

    m_timeline->normalizePacket(packet, m_inputAudioStream, m_outputAudioStream);
    return Status::success();
}

} // namespace media::ffmpeg
