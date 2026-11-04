#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/protocol/codec/MediaAnnexBAccessUnitValidator.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaDeterministicVideoRtpPacketizer final {
public:
    // The payload aliases workspace only during this synchronous callback.
    using Sink = ::media::Status (*)(void*, std::span<const std::uint8_t>, bool);
    static ::media::Result<std::uint64_t> maximumDatagramsForPayloadWindow(
        std::uint64_t maximumPayloadBytes,
        std::uint64_t accessUnitCount,
        MediaAnnexBCodec codec,
        std::size_t maximumRtpPayloadBytes);

    static ::media::Result<std::uint64_t> maximumDatagramsPerAccessUnit(
        std::uint64_t maximumAccessUnitBytes,
        MediaAnnexBCodec codec,
        std::size_t maximumRtpPayloadBytes);

    static ::media::Status packetize(
        std::span<const std::uint8_t> accessUnit,
        MediaAnnexBCodec codec,
        const MediaEncodedPacketLayout& layout,
        std::span<std::uint8_t> workspace,
        void* state, Sink sink);

private:
    MediaDeterministicVideoRtpPacketizer() = delete;
};

} // namespace media::ffmpeg::graph
