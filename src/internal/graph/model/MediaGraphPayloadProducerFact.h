#pragma once

#include "internal/graph/model/MediaGraphPayloadCreditPlan.h"

namespace media::ffmpeg::graph {

// One allocation-producing output key. Bounds are logical payload bytes;
// physical device allocations belong to their separate allocation-owner ledger.
struct MediaGraphPayloadProducerFact final {
    MediaNodeId nodeId;
    MediaStreamKind streamKind;
    MediaPayloadKind payloadKind;
    std::uint64_t maximumLogicalBytes;
    std::optional<MediaFrameCreditContract> frameCredit;
    std::string authority;
};

} // namespace media::ffmpeg::graph
