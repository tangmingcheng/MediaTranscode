#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <concepts>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

class MediaDatagramShaperNode;
class MediaScheduledDatagramSenderNode;
class MediaScheduledWireDatagram;
class MediaScheduledWireDatagramBatchBuffer;
class MediaWireDatagramBatchBuffer;

class MediaDatagramSubmitCommitLease final {
public:
    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation) {
                     { reservation.commit() } ->
                         std::same_as<::media::Status>;
                 }
    static ::media::Result<MediaDatagramSubmitCommitLease> create(
        std::uint64_t generation,
        std::uint64_t globalSequence,
        Reservation reservation)
    {
        using Result = ::media::Result<MediaDatagramSubmitCommitLease>;
        if (generation == 0) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram commit lease requires a generation"));
        }
        try {
            return Result::success(MediaDatagramSubmitCommitLease(
                generation, globalSequence,
                std::make_unique<Model<Reservation>>(
                    std::move(reservation))));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "datagram submit commit lease"));
        }
    }

    MediaDatagramSubmitCommitLease(
        MediaDatagramSubmitCommitLease&&) noexcept = default;
    MediaDatagramSubmitCommitLease& operator=(
        MediaDatagramSubmitCommitLease&&) noexcept = default;
    MediaDatagramSubmitCommitLease(
        const MediaDatagramSubmitCommitLease&) = delete;
    MediaDatagramSubmitCommitLease& operator=(
        const MediaDatagramSubmitCommitLease&) = delete;
    ~MediaDatagramSubmitCommitLease() = default;

    bool valid() const noexcept { return m_reservation != nullptr; }

private:
    class Concept {
    public:
        virtual ~Concept() = default;
        virtual ::media::Status commit() = 0;
    };

    template <typename Reservation>
    class Model final : public Concept {
    public:
        explicit Model(Reservation reservation) noexcept
            : m_reservation(std::move(reservation))
        {
        }

        ::media::Status commit() override
        {
            return m_reservation.commit();
        }

    private:
        Reservation m_reservation;
    };

    MediaDatagramSubmitCommitLease(
        std::uint64_t generation,
        std::uint64_t globalSequence,
        std::unique_ptr<Concept> reservation) noexcept
        : m_generation(generation),
          m_globalSequence(globalSequence),
          m_reservation(std::move(reservation))
    {
    }

    bool matches(std::uint64_t generation,
                 std::uint64_t globalSequence) const noexcept
    {
        return valid() && m_generation == generation &&
               m_globalSequence == globalSequence;
    }
    ::media::Status commit();

    friend class MediaScheduledDatagramSenderNode;
    friend class MediaScheduledWireDatagram;
    friend class MediaScheduledWireDatagramBatchBuffer;
    friend class MediaWireDatagramBatchBuffer;

    std::uint64_t m_generation;
    std::uint64_t m_globalSequence;
    std::unique_ptr<Concept> m_reservation;
};

struct MediaWireDatagramDescriptor final {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::uint64_t payloadOffset;
    std::uint64_t payloadSize;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    std::uint64_t globalSequence;
};

struct MediaWireDatagramBatchEntry final {
    MediaWireDatagramDescriptor descriptor;
    MediaDatagramSubmitCommitLease commitLease;
};

class MediaWireDatagram final {
public:
    MediaWireDatagram(MediaWireDatagram&&) noexcept = default;
    MediaWireDatagram& operator=(MediaWireDatagram&&) noexcept = default;
    MediaWireDatagram(const MediaWireDatagram&) = delete;
    MediaWireDatagram& operator=(const MediaWireDatagram&) = delete;

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
    bool hasCommitLease() const noexcept { return m_commitLease.valid(); }

private:
    friend class MediaDatagramShaperNode;
    friend class MediaWireDatagramBatchBuffer;

    MediaWireDatagram(
        std::span<const std::uint8_t> bytes,
        const MediaWireDatagramDescriptor& descriptor,
        MediaDatagramSubmitCommitLease commitLease) noexcept;
    MediaDatagramSubmitCommitLease takeCommitLease() noexcept;

    std::span<const std::uint8_t> m_bytes;
    std::uint64_t m_generation;
    std::uint64_t m_endpointId;
    MediaRunningTime m_canonicalRelease;
    MediaRunningTime m_canonicalDeadline;
    std::uint64_t m_globalSequence;
    MediaDatagramSubmitCommitLease m_commitLease;
};

class MediaWireDatagramBatchBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
    create(std::vector<std::uint8_t> payload,
           std::vector<MediaWireDatagramBatchEntry> entries);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::WireDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaWireDatagram> datagrams() const noexcept
    {
        return m_datagrams;
    }

private:
    friend class MediaDatagramShaperNode;

    MediaWireDatagramBatchBuffer(
        std::uint64_t generation,
        std::vector<std::uint8_t> payload,
        std::vector<MediaWireDatagram> datagrams) noexcept;

    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaWireDatagram> m_datagrams;
};

} // namespace media::ffmpeg::graph
