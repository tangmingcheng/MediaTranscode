#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

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
    ::media::Status parsePacket();

    MediaTsPacketSink& m_sink;
    std::array<uint8_t, 188> m_packet{};
    std::size_t m_retainedSize = 0;
    uint64_t m_nextInputOffset = 0;
    uint64_t m_packetOffset = 0;
    std::unordered_map<uint16_t, ContinuityState> m_continuity;
};

} // namespace media::ffmpeg::graph
