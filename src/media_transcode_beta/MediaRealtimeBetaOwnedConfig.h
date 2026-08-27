#pragma once

#include "media_transcode/Result.h"
#include "media_transcode_beta/realtime.h"

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"

#include <cstdint>
#include <string>
#include <variant>

namespace media::beta {

class MediaRealtimeBetaOwnedConfig final {
public:
    struct CbrRateControl final {
        int targetBitrateKbps;
        int bufferSizeKbits;
    };
    struct VbrRateControl final {
        int minimumBitrateKbps;
        int targetBitrateKbps;
        int maximumBitrateKbps;
        int bufferSizeKbits;
    };
    using RateControl = std::variant<CbrRateControl, VbrRateControl>;

    static ::media::Result<MediaRealtimeBetaOwnedConfig> create(
        const mt_beta_realtime_config* config,
        const mt_beta_realtime_callbacks* callbacks,
        mt_beta_realtime_session** session);

    const std::string& mediaId() const noexcept;
    const std::string& bindAddress() const noexcept;
    std::uint16_t inputPort() const noexcept;
    mt_beta_video_codec inputCodec() const noexcept;
    std::uint8_t inputPayloadType() const noexcept;
    std::uint32_t inputClockRate() const noexcept;
    const std::string& destinationAddress() const noexcept;
    std::uint16_t destinationPort() const noexcept;
    mt_beta_video_codec outputCodec() const noexcept;
    std::uint32_t width() const noexcept;
    std::uint32_t height() const noexcept;
    std::uint32_t frameRateNumerator() const noexcept;
    std::uint32_t frameRateDenominator() const noexcept;
    std::uint32_t gopFrames() const noexcept;
    const ffmpeg::graph::MediaRealtimeDeploymentEnvelope& deployment() const noexcept;
    const RateControl& rateControl() const noexcept;
    ffmpeg::graph::MediaIpAddressFamily addressFamily() const noexcept;
    mt_beta_realtime_event_callback eventCallback() const noexcept;
    void* eventUserData() const noexcept;

private:
    MediaRealtimeBetaOwnedConfig(
        std::string mediaId,
        std::string bindAddress,
        std::uint16_t inputPort,
        mt_beta_video_codec inputCodec,
        std::uint8_t inputPayloadType,
        std::uint32_t inputClockRate,
        std::string destinationAddress,
        std::uint16_t destinationPort,
        mt_beta_video_codec outputCodec,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frameRateNumerator,
        std::uint32_t frameRateDenominator,
        std::uint32_t gopFrames,
        ffmpeg::graph::MediaRealtimeDeploymentEnvelope deployment,
        RateControl rateControl,
        ffmpeg::graph::MediaIpAddressFamily addressFamily,
        mt_beta_realtime_event_callback eventCallback,
        void* eventUserData);

    std::string m_mediaId;
    std::string m_bindAddress;
    std::uint16_t m_inputPort;
    mt_beta_video_codec m_inputCodec;
    std::uint8_t m_inputPayloadType;
    std::uint32_t m_inputClockRate;
    std::string m_destinationAddress;
    std::uint16_t m_destinationPort;
    mt_beta_video_codec m_outputCodec;
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint32_t m_frameRateNumerator;
    std::uint32_t m_frameRateDenominator;
    std::uint32_t m_gopFrames;
    ffmpeg::graph::MediaRealtimeDeploymentEnvelope m_deployment;
    RateControl m_rateControl;
    ffmpeg::graph::MediaIpAddressFamily m_addressFamily;
    mt_beta_realtime_event_callback m_eventCallback;
    void* m_eventUserData;
};

} // namespace media::beta
