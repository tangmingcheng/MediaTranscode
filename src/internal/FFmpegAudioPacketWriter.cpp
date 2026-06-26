#include "internal/FFmpegAudioPacketWriter.h"

#include "internal/FFmpegError.h"

#include <algorithm>
#include <sstream>
#include <utility>

extern "C" {
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

void FFmpegAudioPacketWriter::reset()
{
    m_outputNode = nullptr;
    m_outputStream = nullptr;

    m_timestampErrorPrefix = "audio packet";
    m_writeErrorMessage = "audio packet write failed";

    m_packetCount = 0;
    m_lastWrittenDts = AV_NOPTS_VALUE;
    m_lastWrittenOutTimeMs = 0;
}

Status FFmpegAudioPacketWriter::initialize(Config config)
{
    reset();

    if (!config.outputNode) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPacketWriter initialize failed: outputNode is null"));
    }

    if (!config.outputStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPacketWriter initialize failed: outputStream is null"));
    }

    m_outputNode = config.outputNode;
    m_outputStream = config.outputStream;
    m_timestampErrorPrefix = std::move(config.timestampErrorPrefix);
    m_writeErrorMessage = std::move(config.writeErrorMessage);

    return Status::success();
}

Status FFmpegAudioPacketWriter::write(
    AVPacket* packet,
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPacketWriter write failed: packet is null"));
    }

    if (!m_outputNode || !m_outputStream) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPacketWriter write failed: writer is not initialized"));
    }

    packet->stream_index = m_outputStream->index;

    Status timestampStatus = validateTimestamp(packet);
    if (!timestampStatus) {
        return timestampStatus;
    }

    updateProgressFromPacket(packet);

    const Status writeStatus = m_outputNode->pushPacket(packet);
    if (!writeStatus) {
        const ErrorInfo& error = writeStatus.error();
        return Status::failure(ErrorInfo{
            error.code,
            m_writeErrorMessage + ": " + error.message
        });
    }

    ++m_packetCount;

    if (onPacketWritten) {
        onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
    }

    return Status::success();
}

bool FFmpegAudioPacketWriter::isInitialized() const
{
    return m_outputNode && m_outputStream;
}

int64_t FFmpegAudioPacketWriter::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegAudioPacketWriter::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

Status FFmpegAudioPacketWriter::validateTimestamp(const AVPacket* packet)
{
    if (!packet) {
        return Status::success();
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        if (m_lastWrittenDts != AV_NOPTS_VALUE && packet->dts <= m_lastWrittenDts) {
            std::ostringstream oss;
            oss << m_timestampErrorPrefix
                << " dts is not strictly increasing: current="
                << packet->dts << ", last=" << m_lastWrittenDts;
            return makeTimestampError(oss.str());
        }

        m_lastWrittenDts = packet->dts;
    }

    if (packet->pts != AV_NOPTS_VALUE &&
        packet->dts != AV_NOPTS_VALUE &&
        packet->pts < packet->dts) {
        std::ostringstream oss;
        oss << m_timestampErrorPrefix
            << " pts is smaller than dts: pts="
            << packet->pts << ", dts=" << packet->dts;
        return makeTimestampError(oss.str());
    }

    return Status::success();
}

void FFmpegAudioPacketWriter::updateProgressFromPacket(const AVPacket* packet)
{
    if (!packet || !m_outputStream) {
        return;
    }

    const int64_t timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (timestamp == AV_NOPTS_VALUE) {
        return;
    }

    const int64_t outTimeMs = av_rescale_q(
        timestamp,
        m_outputStream->time_base,
        AVRational{ 1, 1000 }
    );

    m_lastWrittenOutTimeMs = std::max<int64_t>(m_lastWrittenOutTimeMs, outTimeMs);
}

} // namespace media::ffmpeg
