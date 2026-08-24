#pragma once

#include "media_transcode/Result.h"

#include <concepts>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

class MediaProtocolDatagramCommitLease final {
public:
    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation) {
                     { reservation.commit() } noexcept ->
                         std::same_as<::media::Status>;
                 }
    static ::media::Result<MediaProtocolDatagramCommitLease> create(
        Reservation reservation)
    {
        try {
            return ::media::Result<MediaProtocolDatagramCommitLease>::success(
                MediaProtocolDatagramCommitLease(
                    std::make_unique<Model<Reservation>>(
                        std::move(reservation))));
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaProtocolDatagramCommitLease>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "protocol datagram commit lease"));
        }
    }

    MediaProtocolDatagramCommitLease(
        MediaProtocolDatagramCommitLease&&) noexcept = default;
    MediaProtocolDatagramCommitLease& operator=(
        MediaProtocolDatagramCommitLease&&) noexcept = default;
    MediaProtocolDatagramCommitLease(
        const MediaProtocolDatagramCommitLease&) = delete;
    MediaProtocolDatagramCommitLease& operator=(
        const MediaProtocolDatagramCommitLease&) = delete;
    ~MediaProtocolDatagramCommitLease() = default;

    bool valid() const noexcept { return m_reservation != nullptr; }
    ::media::Status commit() noexcept;

private:
    class Concept {
    public:
        virtual ~Concept() = default;
        virtual ::media::Status commit() noexcept = 0;
    };

    template <typename Reservation>
    class Model final : public Concept {
    public:
        explicit Model(Reservation reservation) noexcept
            : m_reservation(std::move(reservation))
        {
        }

        ::media::Status commit() noexcept override
        {
            return m_reservation.commit();
        }

    private:
        Reservation m_reservation;
    };

    explicit MediaProtocolDatagramCommitLease(
        std::unique_ptr<Concept> reservation) noexcept
        : m_reservation(std::move(reservation))
    {
    }

    std::unique_ptr<Concept> m_reservation;
};

} // namespace media::ffmpeg::graph
