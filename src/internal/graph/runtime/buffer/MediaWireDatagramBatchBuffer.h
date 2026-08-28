#pragma once

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"
#include "media_transcode/Result.h"

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

class MediaDatagramShaperNode;
class MediaDatagramServiceShaper;
class MediaScheduledDatagramSenderNode;
class MediaScheduledWireDatagram;
class MediaScheduledWireDatagramBatchBuffer;
class MediaWireDatagramBatchBuffer;
class MediaWireDatagramBatchBuilder;
class MediaWireDatagramBatchPartitionBuilder;

class MediaDatagramCommitSlice;

class MediaDatagramCommitTransaction final {
public:
    template <typename Reservation>
        requires std::is_nothrow_move_constructible_v<Reservation> &&
                 std::is_nothrow_destructible_v<Reservation> &&
                 requires(Reservation& reservation, std::size_t begin,
                          std::size_t count) {
                     { reservation.size() } noexcept ->
                         std::same_as<std::size_t>;
                     { reservation.sequence(begin) } noexcept ->
                         std::same_as<::media::Result<std::uint64_t>>;
                     { reservation.markScheduledPrefix(
                         begin, count,
                         MediaRunningTime::fromNanoseconds(0)) } noexcept ->
                         std::same_as<::media::Status>;
                     { reservation.commitSubmittedPrefix(
                         begin, count,
                         MediaRunningTime::fromNanoseconds(0)) } noexcept ->
                         std::same_as<::media::Status>;
                 }
    static ::media::Result<MediaDatagramCommitTransaction> create(
        std::uint64_t generation,
        Reservation reservation)
    {
        using Result = ::media::Result<MediaDatagramCommitTransaction>;
        const auto size = reservation.size();
        auto firstSequence = reservation.sequence(0);
        if (generation == 0 || size == 0 || !firstSequence ||
            static_cast<std::uint64_t>(size - 1) >
                (std::numeric_limits<std::uint64_t>::max)() -
                    firstSequence.value()) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram commit transaction requires generation and a representable nonempty sequence range"));
        }
        try {
            return Result::success(MediaDatagramCommitTransaction(
                generation, firstSequence.value(), size,
                std::make_shared<Model<Reservation>>(
                    std::move(reservation))));
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "datagram commit transaction"));
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
            MediaRunningTime now) noexcept = 0;
        virtual void abandon() noexcept = 0;
    };

    template <typename Reservation>
    class Model final : public Concept {
    public:
        explicit Model(Reservation reservation) noexcept
            : m_reservation(std::move(reservation)),
              m_size(m_reservation->size())
        {
        }

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
            MediaRunningTime now) noexcept override
        {
            std::lock_guard lock(m_mutex);
            if (!m_reservation || begin != m_nextCommitted || count == 0 ||
                count > m_size - begin || begin + count > m_nextScheduled) {
                abandonLocked();
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "datagram submitted prefix is stale, empty, unscheduled, or outside its transaction"));
            }
            auto committed = m_reservation->commitSubmittedPrefix(
                begin, count, now);
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
        std::size_t count, MediaRunningTime now) noexcept;
    void abandon() noexcept;

    friend class MediaDatagramCommitTransaction;
    friend class MediaScheduledDatagramSenderNode;
    friend class MediaScheduledWireDatagramBatchBuffer;
    friend class MediaWireDatagramBatchBuffer;

    std::shared_ptr<MediaDatagramCommitTransaction::Concept> m_control;
    std::uint64_t m_generation = 0;
    std::uint64_t m_firstGlobalSequence = 0;
    std::size_t m_begin = 0;
    std::size_t m_count = 0;
    std::size_t m_committed = 0;
    bool m_scheduled = false;
};

struct MediaWireDatagramBatchEntry final {
    MediaWireDatagramDescriptor descriptor;
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
        return m_descriptor.generation;
    }
    std::uint64_t endpointId() const noexcept
    {
        return m_descriptor.endpointId;
    }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_descriptor.canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_descriptor.canonicalDeadline;
    }
    std::uint64_t globalSequence() const noexcept
    {
        return m_descriptor.globalSequence;
    }
private:
    friend class MediaDatagramShaperNode;
    friend class MediaDatagramServiceShaper;
    friend class MediaScheduledWireDatagramBatchBuffer;
    friend class MediaWireDatagramBatchBuffer;

    MediaWireDatagram(
        std::span<const std::uint8_t> bytes,
        const MediaWireDatagramDescriptor& descriptor) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaWireDatagramDescriptor m_descriptor;
};

class MediaWireDatagramBatchBuffer final : public MediaBuffer {
public:
    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::WireDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    const std::string& sessionKey() const noexcept { return m_sessionKey; }
    const std::string& serviceScopeId() const noexcept
    {
        return m_serviceScopeId;
    }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaWireDatagram> datagrams() const noexcept
    {
        return m_datagrams;
    }

private:
    friend class MediaDatagramShaperNode;
    friend class MediaDatagramServiceShaper;
    friend class MediaScheduledWireDatagramBatchBuffer;
    friend class MediaWireDatagramBatchBuilder;
    friend class MediaWireDatagramBatchPartitionBuilder;

    static ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
    create(std::string sessionKey,
           std::string serviceScopeId,
           std::vector<std::uint8_t> payload,
           std::vector<MediaWireDatagramBatchEntry> entries,
           MediaDatagramCommitSlice commitSlice);

    MediaWireDatagramBatchBuffer(
        std::string sessionKey,
        std::string serviceScopeId,
         std::uint64_t generation,
         std::vector<std::uint8_t> payload,
         std::vector<MediaWireDatagram> datagrams,
         MediaDatagramCommitSlice commitSlice) noexcept;

    const std::string m_sessionKey;
    const std::string m_serviceScopeId;
    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaWireDatagram> m_datagrams;
    MediaDatagramCommitSlice m_commitSlice;
};

} // namespace media::ffmpeg::graph
