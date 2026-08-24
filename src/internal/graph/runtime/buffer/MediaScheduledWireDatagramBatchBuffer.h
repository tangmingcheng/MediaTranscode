#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaScheduledWireDatagramDescriptor final {
    MediaWireDatagramDescriptor wire;
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
    std::uint64_t generation() const noexcept
    {
        return m_descriptor.wire.generation;
    }
    std::uint64_t endpointId() const noexcept
    {
        return m_descriptor.wire.endpointId;
    }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_descriptor.wire.canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_descriptor.wire.canonicalDeadline;
    }
    std::uint64_t globalSequence() const noexcept
    {
        return m_descriptor.wire.globalSequence;
    }
    MediaRunningTime enqueueNotBefore() const noexcept
    {
        return m_descriptor.enqueueNotBefore;
    }
    MediaRunningTime enqueueNotAfter() const noexcept
    {
        return m_descriptor.enqueueNotAfter;
    }
    MediaRunningTime wireServiceDuration() const noexcept
    {
        return m_descriptor.wireServiceDuration;
    }
    bool hasCommitLease() const noexcept { return m_commitLease.valid(); }

private:
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaScheduledWireDatagramBatchBuffer;

    MediaScheduledWireDatagram(
        std::span<const std::uint8_t> bytes,
        const MediaScheduledWireDatagramDescriptor& descriptor,
        MediaDatagramSubmitCommitLease commitLease) noexcept;
    ::media::Status commitSubmit() noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaScheduledWireDatagramDescriptor m_descriptor;
    MediaDatagramSubmitCommitLease m_commitLease;
};

class MediaScheduledWireDatagramBatchBuffer final : public MediaBuffer {
public:
    // This factory closes all per-batch limits. The future stateful shaper owns
    // service-scope debt and pending usage that span multiple batches.
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
