#pragma once

#include "internal/graph/protocol/mpegts/MediaTsTransportPacketizer.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaMpegTsProtocolCommitState;

class MediaMpegTsProtocolDatagram final {
public:
    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    MediaRunningTime presentationOnMaster() const noexcept
    {
        return m_presentationOnMaster;
    }
    MediaRunningTime canonicalRelease() const noexcept
    {
        return m_canonicalRelease;
    }
    MediaRunningTime canonicalDeadline() const noexcept
    {
        return m_canonicalDeadline;
    }

private:
    friend class MediaMpegTsProtocolDatagramBatchBuffer;
    MediaMpegTsProtocolDatagram(
        std::span<const std::uint8_t> bytes,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline) noexcept;

    std::span<const std::uint8_t> m_bytes;
    MediaRunningTime m_presentationOnMaster;
    MediaRunningTime m_canonicalRelease;
    MediaRunningTime m_canonicalDeadline;
};

class MediaMpegTsProtocolDatagramBatchBuffer final : public MediaBuffer {
public:
    static ::media::Result<
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer>>
    create(std::uint64_t generation,
           MediaTsPacketCursor cursor,
           std::uint16_t maximumPacketsPerDatagram,
           MediaRunningTime presentationOnMaster,
           MediaRunningTime canonicalRelease,
           MediaRunningTime canonicalDeadline);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::MpegTsProtocolDatagramBatch;
    }
    std::optional<std::uint64_t> payloadFootprintBytes() const noexcept override;
    std::uint64_t generation() const noexcept { return m_generation; }
    std::span<const MediaMpegTsProtocolDatagram> datagrams() const noexcept
    {
        return std::span(m_datagrams).subspan(m_materializedDatagrams);
    }
    ::media::Result<MediaProtocolDatagramCommitTransaction>
    takeCommitTransaction(std::size_t datagrams);

private:
    explicit MediaMpegTsProtocolDatagramBatchBuffer(
        std::uint64_t generation) noexcept;

    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaMpegTsProtocolDatagram> m_datagrams;
    std::shared_ptr<MediaMpegTsProtocolCommitState> m_commitTransaction;
    std::size_t m_materializedDatagrams = 0;
};

} // namespace media::ffmpeg::graph
