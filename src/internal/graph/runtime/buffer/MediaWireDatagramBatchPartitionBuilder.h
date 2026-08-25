#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuilder.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

using MediaWireDatagramBatchCollection =
    std::vector<std::shared_ptr<MediaWireDatagramBatchBuffer>>;

class MediaWireDatagramBatchPartitionBuilder final {
public:
    static ::media::Result<MediaWireDatagramBatchPartitionBuilder> create(
        const std::string& sessionKey,
        const std::string& serviceScopeId,
        std::uint64_t generation,
        const MediaDatagramBatchPlan& batchPlan);

    ::media::Status append(
        std::span<const std::uint8_t> bytes,
        std::uint64_t endpointId,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline,
        std::uint64_t globalSequence,
        MediaDatagramSubmitCommitLease commitLease);

    ::media::Result<MediaWireDatagramBatchCollection> finish();

private:
    MediaWireDatagramBatchPartitionBuilder(
        std::string sessionKey,
        std::string serviceScopeId,
        std::uint64_t generation,
        MediaDatagramBatchPlan batchPlan) noexcept;
    ::media::Status beginPartition();
    ::media::Status finishPartition();

    std::string m_sessionKey;
    std::string m_serviceScopeId;
    std::uint64_t m_generation;
    MediaDatagramBatchPlan m_batchPlan;
    std::optional<MediaWireDatagramBatchBuilder> m_current;
    std::size_t m_currentDatagrams = 0;
    std::uint64_t m_currentBytes = 0;
    std::optional<MediaRunningTime> m_currentDeadline;
    MediaWireDatagramBatchCollection m_partitions;
    bool m_finished = false;
};

} // namespace media::ffmpeg::graph
