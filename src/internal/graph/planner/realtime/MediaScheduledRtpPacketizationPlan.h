#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h"
#include "internal/graph/protocol/rtp/MediaRtpAccessUnitEmissionContract.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaScheduledRtpPacketizationPlan final {
public:
    static ::media::Result<MediaScheduledRtpPacketizationPlan> create(
        MediaStreamKind streamKind, std::string codecName,
        int streamTimeBaseNumerator, int streamTimeBaseDenominator,
        int payloadType, std::size_t maximumDatagramBytes,
        MediaRtpAccessUnitEmissionContract emissionContract,
        std::optional<int> maximumAccessUnitSamples = std::nullopt);

    MediaStreamKind streamKind() const noexcept { return m_streamKind; }
    const std::string& codecName() const noexcept { return m_codecName; }
    int streamTimeBaseNumerator() const noexcept { return m_streamTimeBaseNumerator; }
    int streamTimeBaseDenominator() const noexcept { return m_streamTimeBaseDenominator; }
    MediaScheduledRtpPacketizationMode packetizationMode() const noexcept { return m_packetizationMode; }
    int payloadType() const noexcept { return m_payloadType; }
    std::size_t maximumDatagramBytes() const noexcept { return m_maximumDatagramBytes; }
    const MediaRtpAccessUnitEmissionContract& emissionContract() const noexcept
    {
        return m_emissionContract;
    }
    std::optional<int> maximumAccessUnitSamples() const noexcept { return m_maximumAccessUnitSamples; }
    friend bool operator==(const MediaScheduledRtpPacketizationPlan&,
                           const MediaScheduledRtpPacketizationPlan&) = default;

private:
    MediaScheduledRtpPacketizationPlan(
        MediaStreamKind streamKind, std::string codecName,
        int streamTimeBaseNumerator, int streamTimeBaseDenominator,
        MediaScheduledRtpPacketizationMode packetizationMode, int payloadType,
        std::size_t maximumDatagramBytes,
        MediaRtpAccessUnitEmissionContract emissionContract,
        std::optional<int> maximumAccessUnitSamples);

    MediaStreamKind m_streamKind;
    std::string m_codecName;
    int m_streamTimeBaseNumerator;
    int m_streamTimeBaseDenominator;
    MediaScheduledRtpPacketizationMode m_packetizationMode;
    int m_payloadType;
    std::size_t m_maximumDatagramBytes;
    MediaRtpAccessUnitEmissionContract m_emissionContract;
    std::optional<int> m_maximumAccessUnitSamples;
};

} // namespace media::ffmpeg::graph
