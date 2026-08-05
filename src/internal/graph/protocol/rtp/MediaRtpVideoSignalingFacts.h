#pragma once

#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaH264SignalingFacts final {
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
    std::string profileLevelId;
};

struct MediaHevcSignalingFacts final {
    std::vector<std::uint8_t> vps;
    std::vector<std::uint8_t> sps;
    std::vector<std::uint8_t> pps;
};

using MediaRtpVideoSignalingFacts =
    std::variant<MediaH264SignalingFacts, MediaHevcSignalingFacts>;

struct MediaDetectedRtpVideoSignaling final {
    std::string codecName;
    std::uint8_t payloadType = 0;
    int clockRate = 0;
    std::uint32_t ssrc = 0;
    MediaRtpVideoSignalingFacts facts;
    std::size_t packetCount = 0;
    std::size_t datagramBytes = 0;
    std::int64_t elapsedMilliseconds = 0;
};

struct MediaRtpVideoSignalingObservation final {
    bool epochChanged = false;
    bool complete = false;
};

class MediaRtpVideoSignalingObserver final {
public:
    static ::media::Result<MediaRtpVideoSignalingObserver> create(
        std::string codecName,
        std::uint8_t payloadType,
        int clockRate);

    ::media::Result<MediaRtpVideoSignalingObservation> observe(
        const MediaRtpPacket& packet);
    ::media::Result<MediaDetectedRtpVideoSignaling> detected(
        std::size_t packetCount,
        std::size_t datagramBytes,
        std::int64_t elapsedMilliseconds) const;

private:
    using NalParser = std::variant<MediaH264RtpNalUnitParser,
                                   MediaHevcRtpNalUnitParser>;

    MediaRtpVideoSignalingObserver(std::string codecName,
                                   std::uint8_t payloadType,
                                   int clockRate,
                                   NalParser parser);
    void resetEpoch(std::uint32_t ssrc) noexcept;
    bool complete() const noexcept;

    std::string m_codecName;
    std::uint8_t m_payloadType = 0;
    int m_clockRate = 0;
    NalParser m_parser;
    std::optional<std::uint32_t> m_ssrc;
    std::optional<std::vector<std::uint8_t>> m_vps;
    std::optional<std::vector<std::uint8_t>> m_sps;
    std::optional<std::vector<std::uint8_t>> m_pps;
};

::media::Result<std::string> serializeRtpVideoFmtp(
    const MediaRtpVideoSignalingFacts& facts);

} // namespace media::ffmpeg::graph
