#pragma once

#include "internal/graph/core/MediaGraph.h"
#include <optional>

namespace media::ffmpeg::graph {

struct MediaGraphPayloadProducerOutput final {
    MediaPayloadKind payload;
    std::optional<MediaStreamKind> singleStream;
};

// Controlled list of production implementations that reserve and attach credits.
// No caller-provided integration flag and no independent duplicate whitelist.
inline std::optional<MediaGraphPayloadProducerOutput> mediaGraphPayloadProducerOutput(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RawRtpInput:
    case MediaNodeKind::Demux:
    case MediaNodeKind::MpegTsDemux:
    case MediaNodeKind::PacketNormalize:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Packet, std::nullopt};
    case MediaNodeKind::VideoDecode:
    case MediaNodeKind::HardwareTransfer:
    case MediaNodeKind::VideoFilter:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Frame, MediaStreamKind::Video};
    case MediaNodeKind::VideoEncode:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Packet, MediaStreamKind::Video};
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Frame, MediaStreamKind::Audio};
    case MediaNodeKind::AudioEncode:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Packet, MediaStreamKind::Audio};
    case MediaNodeKind::AvContinuousAggregate:
        return MediaGraphPayloadProducerOutput{MediaPayloadKind::Frame, std::nullopt};
    default:
        return std::nullopt;
    }
}

inline bool acceptsMediaGraphPayloadProducerOutput(MediaNodeKind node, MediaStreamKind stream, MediaPayloadKind payload) noexcept
{
    const auto contract = mediaGraphPayloadProducerOutput(node);
    return contract && (stream == MediaStreamKind::Video || stream == MediaStreamKind::Audio) &&
        contract->payload == payload && (!contract->singleStream || *contract->singleStream == stream);
}

} // namespace media::ffmpeg::graph
