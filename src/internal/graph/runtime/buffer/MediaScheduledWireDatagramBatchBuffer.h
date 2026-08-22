#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaScheduledWireDatagramDescriptor final {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::uint64_t payloadOffset;
    std::uint64_t payloadSize;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    std::uint64_t globalSequence;
    MediaRunningTime enqueueNotBefore;
    MediaRunningTime enqueueNotAfter;
    MediaRunningTime wireServiceDuration;
};

struct MediaScheduledWireDatagramBatchEntry final {
    MediaScheduledWireDatagramDescriptor descriptor;
    MediaDatagramSubmitCommitLease commitLease;
};

class MediaScheduledWireDatagram final {
public:
    MediaScheduledWireDatagram(
        MediaScheduledWireDatagram&&) noexcept = default;
    MediaScheduledWireDatagram& operator=(
        MediaScheduledWireDatagram&&) noexcept = default;
    MediaScheduledWireDatagram(
        const MediaScheduledWireDatagram&) = delete;
    MediaScheduledWireDatagram& operator=(
        const MediaScheduledWireDatagram&) = delete;

    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::uint64_t endpointId() const noexcept { return m_endpointId; }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_canonicalDeadline;
    }
    std::uint64_t globalSequence() const noexcept { return m_globalSequence; }
    MediaRunningTime enqueueNotBefore() const noexcept
    {
        return m_enqueueNotBefore;
    }
    MediaRunningTime enqueueNotAfter() const noexcept
    {
        return m_enqueueNotAfter;
    }
    MediaRunningTime wireServiceDuration() const noexcept
    {
        return m_wireServiceDuration;
    }
    bool hasCommitLease() const noexcept { return m_commitLease.valid(); }

private:
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaScheduledWireDatagramBatchBuffer;

    MediaScheduledWireDatagram(
        std::span<const std::uint8_t> bytes,
        const MediaScheduledWireDatagramDescriptor& descriptor,
        MediaDatagramSubmitCommitLease commitLease) noexcept;
    ::media::Status commitSubmit();

    std::span<const std::uint8_t> m_bytes;
    std::uint64_t m_generation;
    std::uint64_t m_endpointId;
    MediaRunningTime m_canonicalRelease;
    MediaRunningTime m_canonicalDeadline;
    std::uint64_t m_globalSequence;
    MediaRunningTime m_enqueueNotBefore;
    MediaRunningTime m_enqueueNotAfter;
    MediaRunningTime m_wireServiceDuration;
    MediaDatagramSubmitCommitLease m_commitLease;
};

class MediaScheduledWireDatagramBatchBuffer final : public MediaBuffer {
public:
    static ::media::Result<
        std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
    create(const MediaDatagramShapingPlan& plan,
           std::vector<std::uint8_t> payload,
           std::vector<MediaScheduledWireDatagramBatchEntry> entries);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::ScheduledWireDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaScheduledWireDatagram> datagrams() const noexcept
    {
        return m_datagrams;
    }

private:
    MediaScheduledWireDatagramBatchBuffer(
        std::uint64_t generation,
        std::vector<std::uint8_t> payload,
        std::vector<MediaScheduledWireDatagram> datagrams) noexcept;

    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaScheduledWireDatagram> m_datagrams;
};

} // namespace media::ffmpeg::graph
