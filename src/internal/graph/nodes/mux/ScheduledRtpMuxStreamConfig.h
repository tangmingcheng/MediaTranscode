#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"
#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h"
#include "internal/graph/protocol/rtp/MediaRtpAccessUnitEmissionContract.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDatagramWriteAvio.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class ScheduledRtpMuxStreamConfig final {
public:
    static ::media::Result<ScheduledRtpMuxStreamConfig> create(
        MediaStreamKind streamKind,
        const AVCodecParameters& codecParameters,
        AVRational streamTimeBase,
        MediaScheduledRtpPacketizationMode packetizationMode,
        int payloadType,
        std::uint32_t ssrc,
        int maximumDatagramBytes,
        MediaRtpAccessUnitEmissionContract emissionContract);

    ScheduledRtpMuxStreamConfig(ScheduledRtpMuxStreamConfig&&) noexcept = default;
    ScheduledRtpMuxStreamConfig& operator=(ScheduledRtpMuxStreamConfig&&) noexcept = default;
    ScheduledRtpMuxStreamConfig(const ScheduledRtpMuxStreamConfig&) = delete;
    ScheduledRtpMuxStreamConfig& operator=(const ScheduledRtpMuxStreamConfig&) = delete;

    MediaStreamKind streamKind() const noexcept { return m_streamKind; }
    const AVCodecParameters& codecParameters() const noexcept { return *m_codecParameters; }
    AVRational streamTimeBase() const noexcept { return m_streamTimeBase; }
    MediaScheduledRtpPacketizationMode packetizationMode() const noexcept
    {
        return m_packetizationMode;
    }
    const MediaRtpDatagramRewriteIdentity& identity() const noexcept
    {
        return m_identity;
    }
    const FFmpegDatagramWriteAvioConfig& avioConfig() const noexcept
    {
        return m_avioConfig;
    }
    const MediaRtpAccessUnitEmissionContract& emissionContract() const noexcept
    {
        return m_emissionContract;
    }

private:
    ScheduledRtpMuxStreamConfig(
        MediaStreamKind streamKind,
        ::media::ffmpeg::CodecParametersPtr codecParameters,
        AVRational streamTimeBase,
        MediaScheduledRtpPacketizationMode packetizationMode,
        MediaRtpDatagramRewriteIdentity identity,
        FFmpegDatagramWriteAvioConfig avioConfig,
        MediaRtpAccessUnitEmissionContract emissionContract) noexcept;

    MediaStreamKind m_streamKind;
    ::media::ffmpeg::CodecParametersPtr m_codecParameters;
    AVRational m_streamTimeBase;
    MediaScheduledRtpPacketizationMode m_packetizationMode;
    MediaRtpDatagramRewriteIdentity m_identity;
    FFmpegDatagramWriteAvioConfig m_avioConfig;
    MediaRtpAccessUnitEmissionContract m_emissionContract;
};

} // namespace media::ffmpeg::graph
