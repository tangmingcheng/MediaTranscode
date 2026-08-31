#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"

#include "internal/graph/model/MediaNumericIpAddress.h"

#include <limits>
#include <ratio>
#include <utility>

namespace media::beta {
namespace {

::media::Status requireText(const char* text, const char* field)
{
    if (text == nullptr || *text == '\0') {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(std::string(field) + " is required"));
    }
    return ::media::Status::success();
}

bool isVideoCodec(mt_beta_video_codec codec) noexcept
{
    return codec == MT_BETA_VIDEO_CODEC_H264 || codec == MT_BETA_VIDEO_CODEC_HEVC;
}

::media::Result<ffmpeg::graph::MediaNumericIpAddress> parseNumericAddress(const char* text)
{
    auto ipv4 = ffmpeg::graph::MediaNumericIpAddress::create(
        ffmpeg::graph::MediaIpAddressFamily::Ipv4, text);
    if (ipv4) {
        return ipv4;
    }
    return ffmpeg::graph::MediaNumericIpAddress::create(
        ffmpeg::graph::MediaIpAddressFamily::Ipv6, text);
}

::media::Result<MediaRealtimeBetaOwnedConfig::RateControl> copyRateControl(
    const mt_beta_video_output& output)
{
    const auto checkedKiloBits = [](
        std::uint64_t bits,
        const char* field) -> ::media::Result<int> {
        if (bits % std::kilo::num != 0U ||
            bits / std::kilo::num >
                static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            return ::media::Result<int>::failure(
                ::media::ErrorInfo::invalidArgument(
                    std::string(field) +
                    " is not exactly representable as integer kilobits"));
        }
        return ::media::Result<int>::success(
            static_cast<int>(bits / std::kilo::num));
    };
    if (output.rate_control_mode == MT_BETA_RATE_CONTROL_CBR) {
        if (output.rate_control.cbr.bitrate_bps == 0U) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "CBR target bitrate must be positive"));
        }
        auto bitrate = checkedKiloBits(
            output.rate_control.cbr.bitrate_bps, "CBR bitrate bps");
        if (!bitrate) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                bitrate.error());
        }
        return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::success(
            MediaRealtimeBetaOwnedConfig::CbrRateControl{
                bitrate.value() });
    }
    if (output.rate_control_mode != MT_BETA_RATE_CONTROL_VBR) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
            ::media::ErrorInfo::invalidArgument("rate control mode is unsupported"));
    }

    const auto& vbr = output.rate_control.vbr;
    if (vbr.target_bitrate_bps == 0U || vbr.min_bitrate_bps == 0U ||
        vbr.max_bitrate_bps == 0U || vbr.min_bitrate_bps > vbr.target_bitrate_bps ||
        vbr.target_bitrate_bps > vbr.max_bitrate_bps) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                ::media::ErrorInfo::invalidArgument("VBR bitrate range must satisfy min <= target <= max"));
    }
    auto target = checkedKiloBits(vbr.target_bitrate_bps, "VBR target bitrate bps");
    auto minimum = checkedKiloBits(vbr.min_bitrate_bps, "VBR minimum bitrate bps");
    auto maximum = checkedKiloBits(vbr.max_bitrate_bps, "VBR maximum bitrate bps");
    if (!target) return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(target.error());
    if (!minimum) return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(minimum.error());
    if (!maximum) return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(maximum.error());
    return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::success(
        MediaRealtimeBetaOwnedConfig::VbrRateControl{
            minimum.value(), target.value(), maximum.value() });
}

} // namespace

MediaRealtimeBetaOwnedConfig::MediaRealtimeBetaOwnedConfig(
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
    RateControl rateControl,
    std::uint64_t provisionedEgressCapacityBitsPerSecond,
    std::uint32_t maximumWireResidenceMilliseconds,
    ffmpeg::graph::MediaIpAddressFamily addressFamily,
    mt_beta_realtime_event_callback eventCallback,
    void* eventUserData)
    : m_mediaId(std::move(mediaId))
    , m_bindAddress(std::move(bindAddress))
    , m_inputPort(inputPort)
    , m_inputCodec(inputCodec)
    , m_inputPayloadType(inputPayloadType)
    , m_inputClockRate(inputClockRate)
    , m_destinationAddress(std::move(destinationAddress))
    , m_destinationPort(destinationPort)
    , m_outputCodec(outputCodec)
    , m_width(width)
    , m_height(height)
    , m_frameRateNumerator(frameRateNumerator)
    , m_frameRateDenominator(frameRateDenominator)
    , m_gopFrames(gopFrames)
    , m_rateControl(rateControl)
    , m_provisionedEgressCapacityBitsPerSecond(
          provisionedEgressCapacityBitsPerSecond)
    , m_maximumWireResidenceMilliseconds(maximumWireResidenceMilliseconds)
    , m_addressFamily(addressFamily)
    , m_eventCallback(eventCallback)
    , m_eventUserData(eventUserData)
{
}

::media::Result<MediaRealtimeBetaOwnedConfig> MediaRealtimeBetaOwnedConfig::create(
    const mt_beta_realtime_config* config,
    const mt_beta_realtime_callbacks* callbacks,
    mt_beta_realtime_session** session)
{
    if (config == nullptr || callbacks == nullptr || session == nullptr ||
        callbacks->on_event == nullptr) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(
            ::media::ErrorInfo::invalidArgument(
                "config, callbacks, session output, and event callback are required"));
    }
    if (auto status = requireText(config->media_id, "media id"); !status) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(status.error());
    }
    if (auto status = requireText(config->input.bind_address, "bind address"); !status) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(status.error());
    }
    if (auto status = requireText(config->output.destination_address, "destination address"); !status) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(status.error());
    }
    if (config->input.port == 0U || config->output.destination_port == 0U ||
        config->input.payload_type > (std::numeric_limits<std::uint8_t>::max() >> 1U) ||
        config->input.clock_rate == 0U ||
        config->input.clock_rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        !isVideoCodec(config->input.codec) || !isVideoCodec(config->output.codec) ||
        config->output.width == 0U ||
        config->output.height == 0U || config->output.frame_rate_num == 0U ||
        config->output.frame_rate_den == 0U || config->output.gop_frames == 0U ||
        config->output.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config->output.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config->output.frame_rate_num > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config->output.frame_rate_den > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config->output.gop_frames > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        config->deployment.provisioned_egress_capacity_bps < 8U ||
        config->deployment.maximum_wire_residence_ms == 0U) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(
            ::media::ErrorInfo::invalidArgument("realtime video facts are invalid"));
    }

    auto bindAddress = parseNumericAddress(config->input.bind_address);
    if (!bindAddress) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(bindAddress.error());
    }
    auto destinationAddress = parseNumericAddress(config->output.destination_address);
    if (!destinationAddress) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(destinationAddress.error());
    }
    auto rateControl = copyRateControl(config->output);
    if (!rateControl) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(rateControl.error());
    }
    return ::media::Result<MediaRealtimeBetaOwnedConfig>::success(
        MediaRealtimeBetaOwnedConfig(
            config->media_id,
            bindAddress.value().presentation(),
            config->input.port,
            config->input.codec,
            config->input.payload_type,
            config->input.clock_rate,
            destinationAddress.value().presentation(),
            config->output.destination_port,
            config->output.codec,
            config->output.width,
            config->output.height,
            config->output.frame_rate_num,
            config->output.frame_rate_den,
            config->output.gop_frames,
            rateControl.value(),
            config->deployment.provisioned_egress_capacity_bps,
            config->deployment.maximum_wire_residence_ms,
            bindAddress.value().addressFamily(),
            callbacks->on_event,
            callbacks->user_data));
}

const std::string& MediaRealtimeBetaOwnedConfig::mediaId() const noexcept { return m_mediaId; }
const std::string& MediaRealtimeBetaOwnedConfig::bindAddress() const noexcept { return m_bindAddress; }
std::uint16_t MediaRealtimeBetaOwnedConfig::inputPort() const noexcept { return m_inputPort; }
mt_beta_video_codec MediaRealtimeBetaOwnedConfig::inputCodec() const noexcept { return m_inputCodec; }
std::uint8_t MediaRealtimeBetaOwnedConfig::inputPayloadType() const noexcept { return m_inputPayloadType; }
std::uint32_t MediaRealtimeBetaOwnedConfig::inputClockRate() const noexcept { return m_inputClockRate; }
const std::string& MediaRealtimeBetaOwnedConfig::destinationAddress() const noexcept { return m_destinationAddress; }
std::uint16_t MediaRealtimeBetaOwnedConfig::destinationPort() const noexcept { return m_destinationPort; }
mt_beta_video_codec MediaRealtimeBetaOwnedConfig::outputCodec() const noexcept { return m_outputCodec; }
std::uint32_t MediaRealtimeBetaOwnedConfig::width() const noexcept { return m_width; }
std::uint32_t MediaRealtimeBetaOwnedConfig::height() const noexcept { return m_height; }
std::uint32_t MediaRealtimeBetaOwnedConfig::frameRateNumerator() const noexcept { return m_frameRateNumerator; }
std::uint32_t MediaRealtimeBetaOwnedConfig::frameRateDenominator() const noexcept { return m_frameRateDenominator; }
std::uint32_t MediaRealtimeBetaOwnedConfig::gopFrames() const noexcept { return m_gopFrames; }
const MediaRealtimeBetaOwnedConfig::RateControl& MediaRealtimeBetaOwnedConfig::rateControl() const noexcept { return m_rateControl; }
std::uint64_t MediaRealtimeBetaOwnedConfig::provisionedEgressCapacityBitsPerSecond() const noexcept { return m_provisionedEgressCapacityBitsPerSecond; }
std::uint32_t MediaRealtimeBetaOwnedConfig::maximumWireResidenceMilliseconds() const noexcept { return m_maximumWireResidenceMilliseconds; }
ffmpeg::graph::MediaIpAddressFamily MediaRealtimeBetaOwnedConfig::addressFamily() const noexcept { return m_addressFamily; }
mt_beta_realtime_event_callback MediaRealtimeBetaOwnedConfig::eventCallback() const noexcept { return m_eventCallback; }
void* MediaRealtimeBetaOwnedConfig::eventUserData() const noexcept { return m_eventUserData; }

} // namespace media::beta
