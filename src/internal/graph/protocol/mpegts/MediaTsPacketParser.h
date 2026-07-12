#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaTsPacketView final {
    uint64_t byteOffset = 0;
    uint16_t pid = 0;
    bool payloadUnitStart = false;
    uint8_t continuityCounter = 0;
    bool discontinuity = false;
    std::optional<uint64_t> pcr27Mhz;
    std::span<const uint8_t> payloadSpan;
};

class MediaTsPacketSink {
public:
    virtual ~MediaTsPacketSink() = default;

    // packet.payloadSpan is valid only for the duration of this synchronous call.
    virtual ::media::Status onPacket(const MediaTsPacketView& packet) = 0;
    virtual ::media::Status onContinuityLoss(uint16_t) { return ::media::Status::success(); }
};

class MediaTsPacketParser final {
public:
    static ::media::Result<std::unique_ptr<MediaTsPacketParser>> create(
        std::size_t packetStride,
        MediaTsPacketSink& sink);

    ::media::Status push(std::span<const uint8_t> bytes);

private:
    struct ContinuityState final {
        uint8_t counter = 0;
    };

    explicit MediaTsPacketParser(MediaTsPacketSink& sink);
    ::media::Status processBufferedBytes();
    ::media::Status parsePacket(std::span<const uint8_t> packet, uint64_t byteOffset);
    void consumeBufferedBytes(std::size_t count);
    std::size_t bufferedSize() const noexcept;

    MediaTsPacketSink& m_sink;
    std::vector<uint8_t> m_buffer;
    std::size_t m_bufferBegin = 0;
    uint64_t m_bufferOffset = 0;
    uint64_t m_nextInputOffset = 0;
    bool m_strideLocked = false;
    std::unordered_map<uint16_t, ContinuityState> m_continuity;
};

} // namespace media::ffmpeg::graph
