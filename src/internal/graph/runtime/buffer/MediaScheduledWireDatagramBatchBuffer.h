#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaScheduledWireDatagramDescriptor final {
    MediaWireDatagramDescriptor wire;
    MediaRunningTime enqueueNotBefore;
    MediaRunningTime enqueueNotAfter;
    MediaRunningTime wireServiceDuration;
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
    ::media::Status markSubmitted(MediaRunningTime now) noexcept;
    ::media::Status commitSubmit(MediaRunningTime now) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaScheduledWireDatagramDescriptor m_descriptor;
    MediaDatagramSubmitCommitLease m_commitLease;
};

class MediaScheduledWireDatagramBatchBuffer final : public MediaBuffer {
public:
    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::ScheduledWireDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::string& sessionKey() const noexcept { return m_sessionKey; }
    const std::string& serviceScopeId() const noexcept
    {
        return m_serviceScopeId;
    }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaScheduledWireDatagram> datagrams() const noexcept
    {
        return m_datagrams;
    }

private:
    friend class MediaDatagramServiceShaper;
    friend class MediaScheduledDatagramSenderNode;

    static ::media::Result<
        std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
    create(const MediaDatagramShapingPlan& plan,
           MediaWireDatagramBatchBuffer& source,
           std::vector<MediaScheduledWireDatagramDescriptor> descriptors,
           MediaRunningTime scheduledAt);

    MediaScheduledWireDatagramBatchBuffer(
        std::string sessionKey,
        std::string serviceScopeId,
        std::uint64_t generation) noexcept;

    const std::string m_sessionKey;
    const std::string m_serviceScopeId;
    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaScheduledWireDatagram> m_datagrams;
};

} // namespace media::ffmpeg::graph
