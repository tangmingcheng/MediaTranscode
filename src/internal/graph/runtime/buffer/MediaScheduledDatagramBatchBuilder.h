#pragma once

#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuffer.h"

#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaScheduledDatagramBatchBuilder final {
public:
    static ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuilder>>
    create(std::uint64_t generation, std::uint64_t maximumPayloadBytes);

    ::media::Status append(
        std::span<const std::uint8_t> bytes,
        MediaRunningTime enqueueNotBefore,
        MediaRunningTime enqueueDeadline,
        MediaRunningTime serviceDuration);
    ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>> release();
    ::media::Status beginNextBatch();
    bool empty() const noexcept { return m_descriptors.empty(); }
    bool released() const noexcept { return !m_active; }

private:
    MediaScheduledDatagramBatchBuilder(
        std::uint64_t generation,
        std::uint64_t maximumPayloadBytes) noexcept;

    std::uint64_t m_generation;
    std::uint64_t m_maximumPayloadBytes;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaScheduledDatagramDescriptor> m_descriptors;
    bool m_active = true;
};

} // namespace media::ffmpeg::graph
