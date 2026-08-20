#pragma once

#include "internal/graph/protocol/mpegts/MediaTsDatagramSink.h"
#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuilder.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaTsScheduledDatagramSink final : public MediaTsDatagramSink {
public:
    static ::media::Result<std::unique_ptr<MediaTsScheduledDatagramSink>> create(
        std::shared_ptr<MediaScheduledDatagramBatchBuilder> builder,
        std::uint16_t packetSizeBytes);

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        const MediaTsDatagramEnqueueWindow& enqueueWindow) override;
    ::media::Status flush() override;
    ::media::Status close() override;
    void abort() noexcept override;

private:
    explicit MediaTsScheduledDatagramSink(
        std::shared_ptr<MediaScheduledDatagramBatchBuilder> builder,
        std::uint16_t packetSizeBytes) noexcept;
    ::media::Status fail(::media::ErrorInfo error);

    std::shared_ptr<MediaScheduledDatagramBatchBuilder> m_builder;
    std::uint16_t m_packetSizeBytes;
    std::optional<::media::ErrorInfo> m_failure;
    bool m_closed = false;
};

} // namespace media::ffmpeg::graph
