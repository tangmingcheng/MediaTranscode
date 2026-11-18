#pragma once
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketRejection.h"
#include <variant>

namespace media::ffmpeg::graph {
enum class MediaAacRtpPacketKind { Complete, FragmentStart, FragmentContinuation, FragmentCompletion };
struct MediaAacRtpPacketPlan final {
    MediaAacRtpAuHeaderPlan headers;
    MediaAacRtpPacketKind kind;
};
using MediaAacRtpPacketPreparation = std::variant<MediaAacRtpPacketPlan, MediaRtpPacketRejection>;
} // namespace media::ffmpeg::graph
