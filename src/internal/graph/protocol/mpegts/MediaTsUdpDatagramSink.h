#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaOutputByteSink;

class MediaTsUdpDatagramSink final : public MediaTsDatagramSink {
public:
    static ::media::Result<std::unique_ptr<MediaTsUdpDatagramSink>> create(
        std::unique_ptr<MediaOutputByteSink> sink);
    ~MediaTsUdpDatagramSink() override;

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        const MediaTsDatagramEnqueueWindow& enqueueWindow) override;
    ::media::Status flush() override;
    ::media::Status close() override;
    void abort() noexcept override;

private:
    explicit MediaTsUdpDatagramSink(
        std::unique_ptr<MediaOutputByteSink> sink) noexcept;
    ::media::Status fail(::media::ErrorInfo error);
    ::media::Status terminalStatus() const;

    std::unique_ptr<MediaOutputByteSink> m_sink;
    std::optional<MediaRunningTime> m_lastEmitOnMaster;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
