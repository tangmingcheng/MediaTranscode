#pragma once

#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaWireDatagramBatchBuilder final {
public:
    explicit MediaWireDatagramBatchBuilder(std::uint64_t generation) noexcept;

    ::media::Status append(
        std::span<const std::uint8_t> bytes,
        std::uint64_t endpointId,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline,
        std::uint64_t globalSequence,
        MediaDatagramSubmitCommitLease commitLease);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>> finish();

private:
    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaWireDatagramBatchEntry> m_entries;
    bool m_finished = false;
};

} // namespace media::ffmpeg::graph
