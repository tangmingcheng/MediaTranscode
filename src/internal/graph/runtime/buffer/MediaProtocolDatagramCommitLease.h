#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
#include "internal/graph/runtime/storage/MediaOwnedMemoryAllocation.h"

#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

class MediaProtocolDatagramCommitTransaction final {
    class Concept;
    template <typename Reservation> class Model;
    struct ReservationDeleter final {
        void (*destroy)(Concept*, std::pmr::memory_resource*) noexcept;
        std::pmr::memory_resource* resource;
        void operator()(Concept* value) const noexcept { destroy(value, resource); }
    };
    using ReservationOwner = std::unique_ptr<Concept, ReservationDeleter>;
public:
    template <typename Reservation>
    static MediaSharedAllocationGeometry allocationGeometry() noexcept
    {
        return {1, sizeof(Model<Reservation>), alignof(Model<Reservation>)};
    }

    // Bind only pre-admitted real storage. Construction and ownership transfer
    // perform no allocation and do not acquire another budget lease.
    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation, std::size_t count) {
                     { reservation.size() } noexcept -> std::same_as<std::size_t>;
                     { reservation.commitNextPrefix(count) } noexcept -> std::same_as<::media::Status>;
                 }
    static ::media::Result<MediaProtocolDatagramCommitTransaction> bindPrepared(
        Reservation reservation, MediaOwnedMemoryAllocation allocation)
    {
        using Result = ::media::Result<MediaProtocolDatagramCommitTransaction>;
        const auto geometry = allocationGeometry<Reservation>();
        const auto size = reservation.size();
        if (!size || !allocation || allocation.bytes() != geometry.bytes ||
            allocation.alignment() != geometry.alignment)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Protocol commit binding requires exact prepared model storage"));
        auto* resource = allocation.resource();
        auto* model = std::construct_at(static_cast<Model<Reservation>*>(allocation.data()),
            std::move(reservation));
        ReservationOwner owner(model, ReservationDeleter{
            [](Concept* value, std::pmr::memory_resource* memory) noexcept {
                auto* concrete = static_cast<Model<Reservation>*>(value);
                std::destroy_at(concrete);
                memory->deallocate(concrete, sizeof(Model<Reservation>), alignof(Model<Reservation>));
            }, resource});
        (void)allocation.release();
        return Result::success(MediaProtocolDatagramCommitTransaction(size, std::move(owner)));
    }

    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation, std::size_t count) {
                     { reservation.size() } noexcept ->
                         std::same_as<std::size_t>;
                     { reservation.commitNextPrefix(count) } noexcept ->
                         std::same_as<::media::Status>;
                 }
    static ::media::Result<MediaProtocolDatagramCommitTransaction> create(
        Reservation reservation)
    {
        const auto size = reservation.size();
        if (size == 0) {
            return ::media::Result<MediaProtocolDatagramCommitTransaction>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "protocol commit transaction requires a nonempty batch"));
        }
        try {
            return ::media::Result<MediaProtocolDatagramCommitTransaction>::success(
                MediaProtocolDatagramCommitTransaction(
                    size,
                    ReservationOwner(new Model<Reservation>(std::move(reservation)),
                        ReservationDeleter{[](Concept* value, std::pmr::memory_resource*) noexcept { delete static_cast<Model<Reservation>*>(value); }, nullptr})));
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaProtocolDatagramCommitTransaction>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "protocol datagram commit transaction"));
        }
    }

    MediaProtocolDatagramCommitTransaction(
        MediaProtocolDatagramCommitTransaction&&) noexcept = default;
    MediaProtocolDatagramCommitTransaction& operator=(
        MediaProtocolDatagramCommitTransaction&&) noexcept = default;
    MediaProtocolDatagramCommitTransaction(
        const MediaProtocolDatagramCommitTransaction&) = delete;
    MediaProtocolDatagramCommitTransaction& operator=(
        const MediaProtocolDatagramCommitTransaction&) = delete;
    ~MediaProtocolDatagramCommitTransaction() = default;

    bool valid() const noexcept { return m_reservation != nullptr; }
    std::size_t size() const noexcept { return m_size; }
    std::size_t committed() const noexcept { return m_committed; }
    ::media::Status commitNextPrefix(std::size_t count) noexcept;
    void abandon() noexcept { m_reservation.reset(); }

private:
    class Concept {
    public:
        virtual ~Concept() = default;
        virtual ::media::Status commitNextPrefix(
            std::size_t count) noexcept = 0;
    };

    template <typename Reservation>
    class Model final : public Concept {
    public:
        explicit Model(Reservation reservation) noexcept
            : m_reservation(std::move(reservation))
        {
        }

        ::media::Status commitNextPrefix(std::size_t count) noexcept override
        {
            return m_reservation.commitNextPrefix(count);
        }

    private:
        Reservation m_reservation;
    };

    MediaProtocolDatagramCommitTransaction(
        std::size_t size,
        ReservationOwner reservation) noexcept
        : m_size(size), m_reservation(std::move(reservation))
    {
    }

    std::size_t m_size = 0;
    std::size_t m_committed = 0;
    ReservationOwner m_reservation;
};

} // namespace media::ffmpeg::graph
