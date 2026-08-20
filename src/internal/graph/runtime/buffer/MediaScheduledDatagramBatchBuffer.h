#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaScheduledDatagram final {
public:
    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    MediaRunningTime enqueueNotBefore() const noexcept { return m_enqueueNotBefore; }
    MediaRunningTime enqueueDeadline() const noexcept { return m_enqueueDeadline; }
    MediaRunningTime serviceDuration() const noexcept { return m_serviceDuration; }

private:
    friend class MediaScheduledDatagramBatchBuffer;

    MediaScheduledDatagram(
        std::span<const std::uint8_t> bytes,
        MediaRunningTime enqueueNotBefore,
        MediaRunningTime enqueueDeadline,
        MediaRunningTime serviceDuration) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaRunningTime m_enqueueNotBefore;
    MediaRunningTime m_enqueueDeadline;
    MediaRunningTime m_serviceDuration;
};

struct MediaScheduledDatagramDescriptor final {
    std::uint64_t payloadOffset;
    std::uint64_t payloadSize;
    MediaRunningTime enqueueNotBefore;
    MediaRunningTime enqueueDeadline;
    MediaRunningTime serviceDuration;
};

class MediaScheduledDatagramBatchBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>>
    create(std::uint64_t generation,
           std::vector<std::uint8_t> payload,
           std::vector<MediaScheduledDatagramDescriptor> descriptors);

    MediaBufferType type() const noexcept override { return MediaBufferType::ScheduledDatagramBatch; }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaScheduledDatagram> datagrams() const noexcept { return m_datagrams; }

private:
    MediaScheduledDatagramBatchBuffer(
        std::uint64_t generation,
        std::vector<std::uint8_t> payload,
        std::vector<MediaScheduledDatagram> datagrams) noexcept;

    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaScheduledDatagram> m_datagrams;
};

} // namespace media::ffmpeg::graph
