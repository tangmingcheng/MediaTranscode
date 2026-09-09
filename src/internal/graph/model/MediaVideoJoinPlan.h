#pragma once
#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
namespace media::ffmpeg::graph {
// A join admits only an independently decodable IDR access unit proven by
// its NAL syntax. CRA requires a separate leading-picture policy and is not
// admitted by this product. AVPacket KEY is never its evidence authority.
struct MediaVideoJoinPlan final {
    MediaAnnexBCodec codec;
    MediaEncodedPacketLayout packetLayout;
    friend bool operator==(const MediaVideoJoinPlan&, const MediaVideoJoinPlan&) = default;
};
} // namespace media::ffmpeg::graph
