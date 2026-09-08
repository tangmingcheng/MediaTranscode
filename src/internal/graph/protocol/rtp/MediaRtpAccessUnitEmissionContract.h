#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaRtpAccessUnitEmissionContract final {
public:
    static ::media::Result<MediaRtpAccessUnitEmissionContract> createVideo(
        MediaScheduledRtpPacketizationMode mode,
        MediaEncodedPacketLayout layout,
        std::uint64_t maximumAccessUnitPayloadBytes,
        std::size_t maximumDatagramBytes,
        std::string authority);

    static ::media::Result<MediaRtpAccessUnitEmissionContract> createAacLatm(
        std::uint64_t maximumAccessUnitPayloadBytes,
        std::size_t maximumDatagramBytes,
        std::string authority);

    std::uint64_t maximumAccessUnitPayloadBytes() const noexcept
    {
        return m_maximumAccessUnitPayloadBytes;
    }
    std::uint64_t maximumDatagramsPerAccessUnit() const noexcept
    {
        return m_maximumDatagramsPerAccessUnit;
    }
    MediaScheduledRtpPacketizationMode packetizationMode() const noexcept
    {
        return m_packetizationMode;
    }
    std::size_t maximumDatagramBytes() const noexcept
    {
        return m_maximumDatagramBytes;
    }
    const std::optional<MediaEncodedPacketLayout>& packetLayout() const noexcept
    {
        return m_packetLayout;
    }
    const std::string& authority() const noexcept { return m_authority; }

    friend bool operator==(const MediaRtpAccessUnitEmissionContract&,
                           const MediaRtpAccessUnitEmissionContract&) = default;

private:
    MediaRtpAccessUnitEmissionContract(
        std::uint64_t maximumAccessUnitPayloadBytes,
        std::uint64_t maximumDatagramsPerAccessUnit,
        MediaScheduledRtpPacketizationMode packetizationMode,
        std::size_t maximumDatagramBytes,
        std::optional<MediaEncodedPacketLayout> packetLayout,
        std::string authority) noexcept;

    std::uint64_t m_maximumAccessUnitPayloadBytes;
    std::uint64_t m_maximumDatagramsPerAccessUnit;
    MediaScheduledRtpPacketizationMode m_packetizationMode;
    std::size_t m_maximumDatagramBytes;
    std::optional<MediaEncodedPacketLayout> m_packetLayout;
    std::string m_authority;
};

} // namespace media::ffmpeg::graph
