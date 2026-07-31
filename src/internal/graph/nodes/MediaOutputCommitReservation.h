#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace media::ffmpeg::graph {

class MediaOutputCommitReservation final {
public:
    MediaOutputCommitReservation() noexcept = default;
    MediaOutputCommitReservation(
        MediaOutputCommitReservation&& other) noexcept
    {
        moveFrom(other);
    }
    MediaOutputCommitReservation& operator=(
        MediaOutputCommitReservation&& other) noexcept
    {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }
    MediaOutputCommitReservation(
        const MediaOutputCommitReservation&) = delete;
    MediaOutputCommitReservation& operator=(
        const MediaOutputCommitReservation&) = delete;
    ~MediaOutputCommitReservation()
    {
        reset();
    }

    template <typename Reservation>
    static MediaOutputCommitReservation hold(Reservation reservation)
    {
        static_assert(
            std::is_nothrow_move_constructible_v<Reservation>);
        static_assert(
            std::is_nothrow_destructible_v<Reservation>);
        static_assert(sizeof(Reservation) <= StorageSize);
        static_assert(alignof(Reservation) <= StorageAlignment);
        MediaOutputCommitReservation output;
        std::construct_at(
            reinterpret_cast<Reservation*>(output.m_storage),
            std::move(reservation));
        output.m_destroy = [](void* storage) noexcept {
            std::destroy_at(
                reinterpret_cast<Reservation*>(storage));
        };
        output.m_move = [](void* target, void* source) noexcept {
            auto* value = reinterpret_cast<Reservation*>(source);
            std::construct_at(
                reinterpret_cast<Reservation*>(target),
                std::move(*value));
            std::destroy_at(value);
        };
        return output;
    }

private:
    static constexpr std::size_t StorageSize = 128;
    static constexpr std::size_t StorageAlignment =
        alignof(std::max_align_t);
    using Destroy = void (*)(void*) noexcept;
    using Move = void (*)(void*, void*) noexcept;

    void reset() noexcept
    {
        if (m_destroy) {
            m_destroy(m_storage);
            m_destroy = nullptr;
            m_move = nullptr;
        }
    }

    void moveFrom(MediaOutputCommitReservation& other) noexcept
    {
        if (!other.m_move) return;
        other.m_move(m_storage, other.m_storage);
        m_destroy = other.m_destroy;
        m_move = other.m_move;
        other.m_destroy = nullptr;
        other.m_move = nullptr;
    }

    alignas(StorageAlignment) std::byte m_storage[StorageSize]{};
    Destroy m_destroy = nullptr;
    Move m_move = nullptr;
};

} // namespace media::ffmpeg::graph
