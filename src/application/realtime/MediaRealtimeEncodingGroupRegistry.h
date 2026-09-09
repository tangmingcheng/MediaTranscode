#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeVideoEncodingWitness.h"
#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"

#include <map>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class EncodedVideoOutputFanoutNode;

struct MediaRealtimeEncodingGroup final {
    std::uint64_t groupId;
    std::uint64_t generation;
    std::shared_ptr<MediaRuntimeBranch> branch;
    std::vector<MediaNodeId> nodes;
    MediaEdgeId frameInput;
    MediaEncodedBranchEndpoints encoded;
    EncodedVideoOutputFanoutNode* fanout;
    std::shared_ptr<const MediaRealtimeVideoEncodingWitness> witness;
    MediaBufferRef codecParameters;
    std::size_t outputCount = 0;
    bool draining = false;
    bool graphPublished = false;
};

// Owns groups and their identity space; matching decisions belong to planner.
class MediaRealtimeEncodingGroupRegistry final {
public:
    ::media::Result<std::uint64_t> allocateGroupId();
    ::media::Result<std::uint64_t> allocateSegmentId();
    std::vector<MediaRealtimeExistingVideoEncodingGroup> available() const;
    std::map<std::uint64_t, MediaRealtimeEncodingGroup>& groups() noexcept { return m_groups; }
    const std::map<std::uint64_t, MediaRealtimeEncodingGroup>& groups() const noexcept { return m_groups; }

private:
    static ::media::Result<std::uint64_t> allocate(std::uint64_t& next);
    std::uint64_t m_nextGroupId = 1;
    std::uint64_t m_nextSegmentId = 1;
    std::map<std::uint64_t, MediaRealtimeEncodingGroup> m_groups;
};

} // namespace media::ffmpeg::graph
