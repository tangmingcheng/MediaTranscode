#pragma once

#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/model/MediaRuntimeReclamationPlan.h"

#include <functional>

namespace media::ffmpeg::graph {

class CodecResolverNode;
class EncodedVideoOutputFanoutNode;

struct MediaRealtimeCreatedSegment final {
    std::shared_ptr<MediaRuntimeBranch> branch;
    CodecResolverNode* resolver;
    EncodedVideoOutputFanoutNode* encodedFanout;
};

class MediaRealtimeSegmentFactory final {
public:
    using ExportSource = std::function<::media::Result<MediaRuntimeSegmentOutputBinding>(MediaNodeId, MediaPortId)>;
    static ::media::Result<MediaRealtimeCreatedSegment> create(
        std::uint64_t segmentId, std::shared_ptr<const MediaGraph> graph,
        const std::vector<MediaNodeId>& nodes, MediaThreadingPolicy threading,
        std::shared_ptr<MediaRuntimeBranchResourceReservation> resources,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
        std::shared_ptr<MediaDatagramServiceScopeArbiter> serviceScope,
        MediaBufferRef preparedEncoder, MediaGraphExecutionContext& session,
        const ExportSource& exportSource, const MediaRuntimeReclamationPlan& reclamationPlan);
};

} // namespace media::ffmpeg::graph
