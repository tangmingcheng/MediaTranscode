#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketCommitter.h"

#include <array>
#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaTsPacketBatchWriter final {
public:
    static ::media::Result<MediaTsPacketBatchWriter> create(
        std::uint8_t maximumPacketsPerDatagram,
        std::unique_ptr<MediaTsDatagramSink> sink,
        std::unique_ptr<MediaTsPacketCommitter> committer);

    MediaTsPacketBatchWriter(const MediaTsPacketBatchWriter&) = delete;
    MediaTsPacketBatchWriter& operator=(const MediaTsPacketBatchWriter&) = delete;
    MediaTsPacketBatchWriter(MediaTsPacketBatchWriter&&) noexcept = default;
    MediaTsPacketBatchWriter& operator=(MediaTsPacketBatchWriter&&) = delete;
    ~MediaTsPacketBatchWriter();

    ::media::Result<std::size_t> writeCursor(
        MediaTsPacketCursor& cursor,
        MediaRunningTime emitOnMaster);
    ::media::Status finish();
    void abort() noexcept;

private:
    MediaTsPacketBatchWriter(std::uint8_t maximumPacketsPerDatagram,
                             std::unique_ptr<MediaTsDatagramSink> sink,
                             std::unique_ptr<MediaTsPacketCommitter> committer);
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status firstFailure() const;

    std::uint8_t m_maximumPacketsPerDatagram;
    std::unique_ptr<MediaTsDatagramSink> m_sink;
    std::unique_ptr<MediaTsPacketCommitter> m_committer;
    std::array<std::uint8_t, 7 * 188> m_datagram{};
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
