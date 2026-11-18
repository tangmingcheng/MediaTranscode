#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRtcpWireGeometry final {
public:
    static constexpr std::size_t senderReportPayloadBytes() noexcept { return 28; }
    static constexpr std::size_t sdesCnamePrefixBytes() noexcept { return 10; }
    static ::media::Result<std::uint64_t> sdesPayloadBytes(std::size_t cnameBytes);
    static ::media::Result<std::uint64_t> compoundPayloadBytes(
        std::size_t cnameBytes);
    static ::media::Result<std::uint64_t> compoundWireBytes(
        std::size_t cnameBytes, MediaIpAddressFamily addressFamily);
    static constexpr std::uint64_t byePayloadBytes() noexcept { return 8; }

private:
    MediaRtcpWireGeometry() = delete;
};

} // namespace media::ffmpeg::graph
