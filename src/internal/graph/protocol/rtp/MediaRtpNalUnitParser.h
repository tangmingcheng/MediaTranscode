#pragma once

#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaRtpNalUnit final {
public:
    static MediaRtpNalUnit borrowed(std::span<const std::uint8_t> bytes);
    static MediaRtpNalUnit owned(std::vector<std::uint8_t> bytes);

    std::span<const std::uint8_t> bytes() const noexcept;

private:
    explicit MediaRtpNalUnit(std::span<const std::uint8_t> bytes);
    explicit MediaRtpNalUnit(std::vector<std::uint8_t> bytes);

    std::span<const std::uint8_t> m_borrowed;
    std::vector<std::uint8_t> m_owned;
    bool m_ownsBytes = false;
};

struct MediaRtpNalUnitBatch final {
    std::uint32_t timestamp = 0;
    bool marker = false;
    bool discardedFragment = false;
    std::vector<MediaRtpNalUnit> nalUnits;
};

class MediaH264RtpNalUnitParser final {
public:
    explicit MediaH264RtpNalUnitParser(std::uint8_t payloadType) noexcept;

    ::media::Result<MediaRtpNalUnitBatch> push(const MediaRtpPacket& packet);
    void discontinuity() noexcept;

private:
    std::uint8_t m_payloadType = 0;
    std::vector<std::uint8_t> m_fragment;
    std::optional<std::uint8_t> m_fragmentHeader;
    std::optional<std::uint32_t> m_fragmentTimestamp;
};

class MediaHevcRtpNalUnitParser final {
public:
    explicit MediaHevcRtpNalUnitParser(std::uint8_t payloadType) noexcept;

    ::media::Result<MediaRtpNalUnitBatch> push(const MediaRtpPacket& packet);
    void discontinuity() noexcept;

private:
    std::uint8_t m_payloadType = 0;
    std::vector<std::uint8_t> m_fragment;
    std::optional<std::uint16_t> m_fragmentHeader;
    std::optional<std::uint32_t> m_fragmentTimestamp;
};

} // namespace media::ffmpeg::graph
