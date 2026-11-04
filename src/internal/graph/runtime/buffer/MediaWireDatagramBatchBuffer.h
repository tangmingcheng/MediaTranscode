#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"
#include "internal/graph/runtime/network/MediaDatagramSubmittedService.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaDatagramCommitStoragePlan.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometryProbe.h"
#include "internal/graph/runtime/storage/MediaOwnedByteStorage.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/storage/MediaByteSerializationView.h"

#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

class MediaWireGlobalSequenceState;

class MediaScheduledDatagramSenderNode;
class MediaWireDatagramBatchBuffer;
class MediaWireDatagramBatchPartitionBuilder;

class MediaDatagramCommitSlice;

class MediaDatagramCommitTransaction final {
    template<class Reservation> class Model;
public:
    template<class Reservation>
    static ::media::Result<MediaSharedAllocationGeometry> allocationGeometry()
    {
        return probeMediaSharedAllocationGeometry<Model<Reservation>>();
    }

    // Preparation owns real control storage but cannot publish or commit it.
    template<class Reservation>
    class Prepared final {
    public:
        Prepared(Prepared&&) noexcept = default;
        Prepared& operator=(Prepared&&) noexcept = default;
        Prepared(const Prepared&) = delete;
        Prepared& operator=(const Prepared&) = delete;

        ::media::Result<MediaDatagramCommitTransaction> bind(Reservation reservation) &&;

    private:
        friend class MediaDatagramCommitTransaction;
        Prepared(std::uint64_t generation, std::shared_ptr<Model<Reservation>> control) noexcept
            : m_generation(generation), m_control(std::move(control)) {}
        std::uint64_t m_generation;
        std::shared_ptr<Model<Reservation>> m_control;
    };

    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation, std::size_t begin,
                          std::size_t count, const MediaDatagramSubmittedService& submitted) {
                     { reservation.size() } noexcept ->
                         std::same_as<std::size_t>;
                     { reservation.sequence(begin) } noexcept ->
                         std::same_as<::media::Result<std::uint64_t>>;
                     { reservation.markScheduledPrefix(
                         begin, count,
                         MediaRunningTime::fromNanoseconds(0)) } noexcept ->
                         std::same_as<::media::Status>;
                     { reservation.commitSubmittedPrefix(
                         begin, count, submitted) } noexcept ->
                         std::same_as<::media::Status>;
                 }
    static ::media::Result<Prepared<Reservation>> prepare(std::uint64_t generation,
        const MediaDatagramCommitStoragePlan& storage,
        std::shared_ptr<MediaNetworkMemoryResource> resource,
        MediaOwnedMemoryAllocation allocation)
    {
        using Result = ::media::Result<Prepared<Reservation>>;
        auto geometry = probeMediaSharedAllocationGeometry<Model<Reservation>>(
            std::type_identity<MediaSharedAllocationAllocator<Model<Reservation>>>{});
        if (!geometry) return Result::failure(geometry.error());
        if (storage.geometry != geometry.value())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Commit storage differs from its actual allocator geometry"));
        if (!resource || storage.kind != Reservation::storageKind())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Commit preparation requires matching planned storage and resource"));
        if (generation == 0) return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram commit preparation requires a positive generation"));
        try {
            return Result::success(Prepared<Reservation>(
                generation, std::allocate_shared<Model<Reservation>>(
                    MediaSharedAllocationAllocator<Model<Reservation>>{
                        MediaSharedAllocationAdmitted{std::move(resource), storage.geometry, &allocation}})));

        } catch (const MediaSharedAllocationMismatch&) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Commit allocation differs from its prepared geometry"));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "datagram commit preparation"));
        }
    }

    MediaDatagramCommitTransaction(
        MediaDatagramCommitTransaction&& other) noexcept;
    MediaDatagramCommitTransaction& operator=(
        MediaDatagramCommitTransaction&& other) noexcept;
    MediaDatagramCommitTransaction(
        const MediaDatagramCommitTransaction&) = delete;
    MediaDatagramCommitTransaction& operator=(
        const MediaDatagramCommitTransaction&) = delete;
    ~MediaDatagramCommitTransaction() noexcept;

    bool valid() const noexcept { return m_control != nullptr; }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::uint64_t firstGlobalSequence() const noexcept
    {
        return m_firstGlobalSequence;
    }
    std::size_t size() const noexcept { return m_size; }
    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept;

private:
    class Concept {
    public:
        virtual ~Concept() = default;
        virtual ::media::Status markScheduledPrefix(
            std::size_t begin,
            std::size_t count,
            MediaRunningTime now) noexcept = 0;
        virtual ::media::Status commitSubmittedPrefix(
            std::size_t begin,
            std::size_t count,
            const MediaDatagramSubmittedService& submitted) noexcept = 0;
        virtual void abandon() noexcept = 0;
    };

    template <typename Reservation>
    class Model final : public Concept {
    public:
        Model() noexcept = default;

        ::media::Status markScheduledPrefix(
            std::size_t begin,
            std::size_t count,
            MediaRunningTime now) noexcept override
        {
            std::lock_guard lock(m_mutex);
            if (!m_reservation || begin != m_nextScheduled || count == 0 ||
                count > m_size - begin) {
                abandonLocked();
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "datagram schedule prefix is stale, empty, or outside its transaction"));
            }
            auto marked = m_reservation->markScheduledPrefix(begin, count, now);
            if (!marked) {
                abandonLocked();
                return marked;
            }
            m_nextScheduled += count;
            return ::media::Status::success();
        }

        ::media::Status commitSubmittedPrefix(
            std::size_t begin,
            std::size_t count,
            const MediaDatagramSubmittedService& submitted) noexcept override
        {
            std::lock_guard lock(m_mutex);
            if (!m_reservation || begin != m_nextCommitted || count == 0 ||
                count > m_size - begin || begin + count > m_nextScheduled) {
                abandonLocked();
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "datagram submitted prefix is stale, empty, unscheduled, or outside its transaction"));
            }
            auto committed = m_reservation->commitSubmittedPrefix(
                begin, count, submitted);
            if (!committed) {
                abandonLocked();
                return committed;
            }
            m_nextCommitted += count;
            if (m_nextCommitted == m_size) m_reservation.reset();
            return ::media::Status::success();
        }

        void abandon() noexcept override
        {
            std::lock_guard lock(m_mutex);
            abandonLocked();
        }

    private:
        friend class Prepared<Reservation>;
        // Binding authority belongs only to the exclusive prepared handle.
        void bind(Reservation reservation) noexcept
        {
            m_size = reservation.size();
            m_reservation.emplace(std::move(reservation));
        }
        void abandonLocked() noexcept { m_reservation.reset(); }

        std::mutex m_mutex;
        std::optional<Reservation> m_reservation;
        std::size_t m_size = 0;
        std::size_t m_nextScheduled = 0;
        std::size_t m_nextCommitted = 0;
    };

    MediaDatagramCommitTransaction(
        std::uint64_t generation,
        std::uint64_t firstGlobalSequence,
        std::size_t size,
        std::shared_ptr<Concept> control) noexcept
        : m_generation(generation),
          m_firstGlobalSequence(firstGlobalSequence),
          m_size(size),
          m_control(std::move(control))
    {
    }

    ::media::Result<MediaDatagramCommitSlice> takeNextSlice(
        std::size_t count) noexcept;
    void abandonUnsliced() noexcept;

    friend class MediaWireDatagramBatchPartitionBuilder;
    friend class MediaDatagramCommitSlice;

    std::uint64_t m_generation = 0;
    std::uint64_t m_firstGlobalSequence = 0;
    std::size_t m_size = 0;
    std::size_t m_nextSlice = 0;
    std::shared_ptr<Concept> m_control;
};

template<class Reservation>
::media::Result<MediaDatagramCommitTransaction>
MediaDatagramCommitTransaction::Prepared<Reservation>::bind(Reservation reservation) &&
{
    using Result = ::media::Result<MediaDatagramCommitTransaction>;
    const auto size = reservation.size();
    auto firstSequence = reservation.sequence(0);
    if (!m_control || size == 0 || !firstSequence ||
        static_cast<std::uint64_t>(size - 1) >
            (std::numeric_limits<std::uint64_t>::max)() - firstSequence.value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram commit binding requires unused preparation and a representable nonempty sequence range"));
    }
    m_control->bind(std::move(reservation));
    return Result::success(MediaDatagramCommitTransaction(
        m_generation, firstSequence.value(), size, std::move(m_control)));
}

class MediaDatagramCommitSlice final {
public:
    MediaDatagramCommitSlice(MediaDatagramCommitSlice&& other) noexcept;
    MediaDatagramCommitSlice& operator=(
        MediaDatagramCommitSlice&& other) noexcept;
    MediaDatagramCommitSlice(const MediaDatagramCommitSlice&) = delete;
    MediaDatagramCommitSlice& operator=(
        const MediaDatagramCommitSlice&) = delete;
    ~MediaDatagramCommitSlice() noexcept;

    bool valid() const noexcept { return m_control != nullptr; }
    std::size_t size() const noexcept { return m_count; }

private:
    MediaDatagramCommitSlice(
        std::shared_ptr<MediaDatagramCommitTransaction::Concept> control,
        std::uint64_t generation,
        std::uint64_t firstGlobalSequence,
        std::size_t begin,
        std::size_t count) noexcept;
    bool matches(std::uint64_t generation,
                 std::uint64_t firstGlobalSequence,
                 std::size_t count) const noexcept;
    ::media::Status scheduleAll(MediaRunningTime now) noexcept;
    ::media::Status commitSubmittedPrefix(
        std::size_t count, const MediaDatagramSubmittedService& submitted) noexcept;
    void abandon() noexcept;

    friend class MediaDatagramCommitTransaction;
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaWireDatagramBatchBuffer;

    std::shared_ptr<MediaDatagramCommitTransaction::Concept> m_control;
    std::uint64_t m_generation = 0;
    std::uint64_t m_firstGlobalSequence = 0;
    std::size_t m_begin = 0;
    std::size_t m_count = 0;
    std::size_t m_committed = 0;
    bool m_scheduled = false;
};

struct MediaWireDatagramBatchInput final {
    MediaByteSerializationView bytes;
    std::uint64_t endpointId;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    MediaWireMediaBoundary mediaBoundary;
    std::optional<MediaWireProtocolDeadlineObservation> protocolDeadlineObservation;
};

class MediaWireDatagram final {
public:
    MediaWireDatagram(MediaWireDatagram&&) noexcept = default;
    MediaWireDatagram& operator=(MediaWireDatagram&&) noexcept = default;
    MediaWireDatagram(const MediaWireDatagram&) = delete;
    MediaWireDatagram& operator=(const MediaWireDatagram&) = delete;

    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    std::uint64_t generation() const noexcept
    {
        return m_layout.generation;
    }
    std::uint64_t endpointId() const noexcept
    {
        return m_layout.endpointId;
    }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_layout.canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_layout.canonicalDeadline;
    }
    const std::optional<MediaWireProtocolDeadlineObservation>&
    protocolDeadlineObservation() const noexcept { return m_layout.protocolDeadlineObservation; }
    std::uint64_t globalSequence() const noexcept
    {
        return *m_globalSequence;
    }
private:
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaWireDatagramBatchBuffer;

    MediaWireDatagram(
        std::span<const std::uint8_t> bytes,
        const MediaWireDatagramLayout& layout) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaWireDatagramLayout m_layout;
    std::optional<std::uint64_t> m_globalSequence;
};

class MediaWireDatagramBatchBuffer final : public MediaBuffer {
public:
    class ConstructionKey final {
        friend class MediaWireDatagramBatchBuffer;
        explicit ConstructionKey() = default;
    };
    static ::media::Result<MediaSharedAllocationGeometry> allocationGeometry();
    MediaWireDatagramBatchBuffer(ConstructionKey,
        std::shared_ptr<const MediaWireGlobalSequenceState> identity,
        std::uint64_t generation, MediaOwnedByteStorage payload,
        MediaFixedCapacityStorage<MediaWireDatagram> datagrams);
    class Prepared final {
    public:
        Prepared(Prepared&&) noexcept = default;
        Prepared& operator=(Prepared&&) noexcept = default;
        Prepared(const Prepared&) = delete;
        Prepared& operator=(const Prepared&) = delete;
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>> bind(
            MediaDatagramCommitSlice slice) &&;
    private:
        friend class MediaWireDatagramBatchBuffer;
        explicit Prepared(std::shared_ptr<MediaWireDatagramBatchBuffer> buffer) noexcept
            : m_buffer(std::move(buffer)) {}
        std::shared_ptr<MediaWireDatagramBatchBuffer> m_buffer;
    };

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::WireDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::string& sessionKey() const noexcept;
    const std::string& serviceScopeId() const noexcept;
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaWireDatagram> datagrams() const noexcept
    {
        return m_datagrams.view();
    }

private:
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaWireDatagramBatchPartitionBuilder;

    static ::media::Result<Prepared>
    prepare(std::shared_ptr<const MediaWireGlobalSequenceState> identity,
        std::span<const MediaWireDatagramBatchInput> inputs,
        MediaOwnedByteStorage payload,
        MediaFixedCapacityStorage<MediaWireDatagram> datagrams,
        const MediaSharedAllocationGeometry& geometry,
        std::shared_ptr<MediaNetworkMemoryResource> resource,
        MediaOwnedMemoryAllocation control);

    ::media::Status bindCommit(MediaDatagramCommitSlice slice);

    const std::shared_ptr<const MediaWireGlobalSequenceState> m_identity;
    std::uint64_t m_generation;
    MediaOwnedByteStorage m_payload;
    MediaFixedCapacityStorage<MediaWireDatagram> m_datagrams;
    std::optional<MediaDatagramCommitSlice> m_commitSlice;
};

} // namespace media::ffmpeg::graph
