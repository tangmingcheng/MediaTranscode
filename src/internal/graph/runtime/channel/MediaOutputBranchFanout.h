#pragma once

#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
#include <mutex>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaBranchStartGate { Immediate, RandomAccessUnit };
enum class MediaBranchPublicationBoundary { Ordinary, RandomAccessUnit, Control };

// Common subscription ownership and publication boundary. Media nodes supply
// their header/reference replica operation and authoritative start boundaries.
class MediaOutputBranchFanout final {
public:
    using ReplicaFactory = ::media::Result<MediaBufferRef> (*)(
        const MediaBufferRef&, MediaBranchPublicationBoundary);
    ::media::Status subscribe(std::shared_ptr<MediaRuntimeBranch> branch,
                             MediaEdgeId edge, MediaBranchStartGate gate);
    void unsubscribe(std::uint64_t outputId);
    ::media::Status publish(const MediaBufferRef& source,
                           MediaBranchPublicationBoundary boundary,
                           ReplicaFactory replica);
private:
    struct Subscription final {
        std::shared_ptr<MediaRuntimeBranch> branch;
        MediaEdgeId edge;
        MediaBranchStartGate gate;
        bool awaitingRandomAccess;
        ::media::ErrorInfo replicaAllocationFailure;
    };
    std::mutex m_mutex;
    std::vector<Subscription> m_subscriptions;
};

} // namespace media::ffmpeg::graph
