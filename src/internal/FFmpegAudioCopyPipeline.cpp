#include "internal/FFmpegAudioCopyPipeline.h"

#include "internal/FFmpegError.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/output/AudioOutputStreamProvider.h"

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
    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioCopyPipeline processPacket failed: packet is null"));
    }

    if (!isInitialized()) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioCopyPipeline processPacket failed: pipeline is not initialized"));
    }

    Status timestampStatus = normalizePacketTimestamp(packet);
    if (!timestampStatus) {
        return timestampStatus;
    }

    return m_packetWriter.write(packet, onPacketWritten);
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
    if (!packet || !m_inputAudioStream || !m_outputAudioStream || !m_timeline) {
        return Status::success();
    }

    const AVRational inputTimeBase = m_inputAudioStream->time_base;
    const AVRational outputTimeBase = m_outputAudioStream->time_base;

    if (packet->pts != AV_NOPTS_VALUE) {
        const int64_t ptsUs = TimelineNormalizer::toUs(packet->pts, inputTimeBase);
        const int64_t normalizedPtsUs = m_timeline->normalizeUs(ptsUs);

        if (normalizedPtsUs == AV_NOPTS_VALUE) {
            return makeTimestampError("failed to normalize audio packet pts");
        }

        packet->pts = TimelineNormalizer::fromUs(normalizedPtsUs, outputTimeBase);
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        const int64_t dtsUs = TimelineNormalizer::toUs(packet->dts, inputTimeBase);
        const int64_t normalizedDtsUs = m_timeline->normalizeUs(dtsUs);

        if (normalizedDtsUs == AV_NOPTS_VALUE) {
            return makeTimestampError("failed to normalize audio packet dts");
        }

        packet->dts = TimelineNormalizer::fromUs(normalizedDtsUs, outputTimeBase);
    }

    if (packet->duration > 0) {
        packet->duration = av_rescale_q(packet->duration, inputTimeBase, outputTimeBase);
    }

    return Status::success();
}

} // namespace media::ffmpeg
