#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsPacketizerControlState;
struct MediaTsPacketCursorState;
struct MediaTsPacketCursorFactory;

class MediaTsPacketCommitToken final {
public:
    MediaTsPacketCommitToken(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken& operator=(const MediaTsPacketCommitToken&) = delete;
    MediaTsPacketCommitToken(MediaTsPacketCommitToken&& other) noexcept;
    MediaTsPacketCommitToken& operator=(MediaTsPacketCommitToken&& other) noexcept;

private:
    friend class MediaTsPacketCursor;
    MediaTsPacketCommitToken(std::weak_ptr<MediaTsPacketizerControlState> owner,
                             std::uint64_t cursorIdentity,
                             std::uint64_t revision) noexcept;

    std::weak_ptr<MediaTsPacketizerControlState> m_owner;
    std::uint64_t m_cursorIdentity = 0;
    std::uint64_t m_revision = 0;
    bool m_valid = false;
};

class MediaTsPreparedPacketBatch final {
public:
    MediaTsPreparedPacketBatch(const MediaTsPreparedPacketBatch&) = delete;
    MediaTsPreparedPacketBatch& operator=(const MediaTsPreparedPacketBatch&) = delete;
    MediaTsPreparedPacketBatch(MediaTsPreparedPacketBatch&&) noexcept = default;
    MediaTsPreparedPacketBatch& operator=(MediaTsPreparedPacketBatch&&) noexcept = default;

    std::span<const std::array<std::uint8_t, 188>> packets() const noexcept;
    MediaTsPacketCommitToken takeCommitToken() noexcept;

private:
    friend class MediaTsPacketCursor;
    using PacketStorage = std::vector<std::array<std::uint8_t, 188>>;
    MediaTsPreparedPacketBatch(std::shared_ptr<const PacketStorage> storage,
                               std::size_t begin,
                               std::size_t count,
                               MediaTsPacketCommitToken commitToken) noexcept;

    std::shared_ptr<const PacketStorage> m_storage;
    std::size_t m_begin;
    std::size_t m_count;
    MediaTsPacketCommitToken m_commitToken;
};

class MediaTsPreparedPacketSeries final {
public:
    MediaTsPreparedPacketSeries(const MediaTsPreparedPacketSeries&) = delete;
    MediaTsPreparedPacketSeries& operator=(const MediaTsPreparedPacketSeries&) = delete;
    MediaTsPreparedPacketSeries(MediaTsPreparedPacketSeries&&) noexcept = default;
    MediaTsPreparedPacketSeries& operator=(MediaTsPreparedPacketSeries&&) noexcept = default;

    std::span<const std::array<std::uint8_t, 188>> packets() const noexcept;
    MediaTsPacketCommitToken takeCommitToken() noexcept;

private:
    friend class MediaTsPacketCursor;
    using PacketStorage = std::vector<std::array<std::uint8_t, 188>>;
    MediaTsPreparedPacketSeries(
        std::shared_ptr<const PacketStorage> storage,
        std::size_t begin,
        std::size_t count,
        MediaTsPacketCommitToken commitToken) noexcept;

    std::shared_ptr<const PacketStorage> m_storage;
    std::size_t m_begin;
    std::size_t m_count;
    MediaTsPacketCommitToken m_commitToken;
};

class MediaTsPacketCursor final {
public:
    ~MediaTsPacketCursor();
    MediaTsPacketCursor(const MediaTsPacketCursor&) = delete;
    MediaTsPacketCursor& operator=(const MediaTsPacketCursor&) = delete;
    MediaTsPacketCursor(MediaTsPacketCursor&& other) noexcept;
    MediaTsPacketCursor& operator=(MediaTsPacketCursor&& other) noexcept;

    ::media::Result<MediaTsPreparedPacketBatch> prepare(std::size_t maximumPackets);
    ::media::Result<MediaTsPreparedPacketSeries> prepareRemaining();
    ::media::Status commit(MediaTsPacketCommitToken commitToken);
    void poison() noexcept;
    bool finished() const noexcept;
    std::size_t remainingPacketCount() const noexcept;

private:
    friend class MediaTsTransportPacketizer;
    friend struct MediaTsPacketCursorFactory;
    explicit MediaTsPacketCursor(std::unique_ptr<MediaTsPacketCursorState> state) noexcept;
    void cancel() noexcept;

    std::unique_ptr<MediaTsPacketCursorState> m_state;
};

class MediaTsTransportPacketizer final {
public:
    static ::media::Result<MediaTsTransportPacketizer> create(
        const MediaTsMuxPlan& plan,
        bool startsWithDiscontinuity);

    MediaTsTransportPacketizer(const MediaTsTransportPacketizer&) = delete;
    MediaTsTransportPacketizer& operator=(const MediaTsTransportPacketizer&) = delete;
    MediaTsTransportPacketizer(MediaTsTransportPacketizer&&) noexcept = default;
    MediaTsTransportPacketizer& operator=(MediaTsTransportPacketizer&&) noexcept = default;

    ::media::Result<MediaTsPacketCursor> beginPat(const MediaTsPatSection& section);
    ::media::Result<MediaTsPacketCursor> beginPmt(const MediaTsPmtSection& section);
    ::media::Result<MediaTsPacketCursor> beginPcrOnly(const MediaTsPcrClock& pcr);
    ::media::Result<std::size_t> patPacketCount(
        const MediaTsPatSection& section) const;
    ::media::Result<std::size_t> pmtPacketCount(
        const MediaTsPmtSection& section) const;
    ::media::Result<std::size_t> pcrOnlyPacketCount(
        const MediaTsPcrClock& pcr) const;
    ::media::Result<MediaTsPacketCursor> beginPes(
        MediaScheduledStream stream,
        const MediaTsPesHeader& header,
        std::span<const std::uint8_t> payload,
        bool randomAccess);

private:
    explicit MediaTsTransportPacketizer(
        std::shared_ptr<MediaTsPacketizerControlState> state) noexcept;

    std::shared_ptr<MediaTsPacketizerControlState> m_state;
};

} // namespace media::ffmpeg::graph
