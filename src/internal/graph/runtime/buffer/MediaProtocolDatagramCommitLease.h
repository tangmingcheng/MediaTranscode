#pragma once

#include "media_transcode/Result.h"

#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

class MediaProtocolDatagramCommitTransaction final {
public:
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
                    std::make_unique<Model<Reservation>>(
                        std::move(reservation))));
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
        std::unique_ptr<Concept> reservation) noexcept
        : m_size(size), m_reservation(std::move(reservation))
    {
    }

    std::size_t m_size = 0;
    std::size_t m_committed = 0;
    std::unique_ptr<Concept> m_reservation;
};

} // namespace media::ffmpeg::graph
