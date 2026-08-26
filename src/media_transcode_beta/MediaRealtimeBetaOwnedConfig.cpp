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
        if (output.rate_control.cbr.bitrate_bps == 0U ||
            output.vbv_buffer_size_bits == 0U) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "CBR bitrate and VBV buffer size must be positive"));
        }
        auto bitrate = checkedKiloBits(
            output.rate_control.cbr.bitrate_bps, "CBR bitrate bps");
        if (!bitrate) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                bitrate.error());
        }
        auto bufferSize = checkedKiloBits(
            output.vbv_buffer_size_bits, "VBV buffer size bits");
        if (!bufferSize) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                bufferSize.error());
        }
        return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::success(
            { output.rate_control_mode, bitrate.value(), 0, 0,
              bufferSize.value() });
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
    int bufferSizeKbits = 0;
    if (output.vbv_buffer_size_bits != 0U) {
        auto bufferSize = checkedKiloBits(
            output.vbv_buffer_size_bits, "VBV buffer size bits");
        if (!bufferSize) {
            return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::failure(
                bufferSize.error());
        }
        bufferSizeKbits = bufferSize.value();
    }
    return ::media::Result<MediaRealtimeBetaOwnedConfig::RateControl>::success(
        { output.rate_control_mode, target.value(), minimum.value(), maximum.value(),
          bufferSizeKbits });
}

::media::Result<ffmpeg::graph::MediaRealtimeDeploymentEnvelope>
copyDeployment(const mt_beta_realtime_deployment& source)
{
    using namespace ffmpeg::graph;
    const auto runningMilliseconds = [](std::uint64_t value) {
        if (value == 0 || value > static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max() / 1'000'000)) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "deployment duration is outside the running-time range"));
        }
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(value) * 1'000'000));
    };
    for (const auto& field : {
             std::pair{source.scope_id, "deployment scope id"},
             std::pair{source.scope_authority, "deployment scope authority"},
             std::pair{source.mtu_authority, "deployment MTU authority"},
             std::pair{source.service_authority, "deployment service authority"},
             std::pair{source.resource_authority, "deployment resource authority"},
             std::pair{source.local_address, "deployment local address"},
             std::pair{source.local_authority, "deployment local authority"},
             std::pair{source.latency_authority, "deployment latency authority"},
             std::pair{source.observation_authority, "deployment observation authority"}}) {
        if (auto valid = requireText(field.first, field.second); !valid) {
            return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
                valid.error());
        }
    }
    MediaDatagramServiceScopeKind scope =
        MediaDatagramServiceScopeKind::Unknown;
    if (source.scope_kind == MT_BETA_EGRESS_SCOPE_MANAGED) {
        scope = MediaDatagramServiceScopeKind::ManagedEgress;
    } else if (source.scope_kind == MT_BETA_EGRESS_SCOPE_PROVISIONED) {
        scope = MediaDatagramServiceScopeKind::ProvisionedEgress;
    }
    auto maximumResidence = runningMilliseconds(source.maximum_residence_ms);
    auto targetResidence = runningMilliseconds(source.target_residence_ms);
    auto drainResidence = runningMilliseconds(
        source.observation_drain_residence_ms);
    if (!maximumResidence || !targetResidence || !drainResidence) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            !maximumResidence ? maximumResidence.error() :
            !targetResidence ? targetResidence.error() : drainResidence.error());
    }
    MediaRealtimeDeploymentEnvelopeEncoding encoding;
    encoding.serviceScope = {
        scope, source.scope_id, source.scope_authority};
    if (source.address_family != MT_BETA_IP_ADDRESS_FAMILY_IPV4 &&
        source.address_family != MT_BETA_IP_ADDRESS_FAMILY_IPV6) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "deployment address family must be IPv4 or IPv6"));
    }
    const auto mtuAddressFamily =
        source.address_family == MT_BETA_IP_ADDRESS_FAMILY_IPV4
            ? MediaIpAddressFamily::Ipv4
            : MediaIpAddressFamily::Ipv6;
    encoding.mtu = {
        mtuAddressFamily, source.mtu_authority,
        source.maximum_ip_packet_bytes, source.sender_maximum_payload_bytes};
    encoding.service = {
        source.sustained_wire_bytes_per_second,
        source.peak_wire_bytes_per_second, source.burst_wire_bytes,
        source.service_authority};
    MediaRealtimeGraphResourceBudgetScope graphResourceScope =
        MediaRealtimeGraphResourceBudgetScope::Unknown;
    if (source.graph_resource_scope ==
        MT_BETA_GRAPH_RESOURCE_ENGINE_MANAGED_PAYLOAD_AND_RESERVED_STORAGE) {
        graphResourceScope = MediaRealtimeGraphResourceBudgetScope::
            EngineManagedPayloadAndReservedStorage;
    } else if (source.graph_resource_scope ==
               MT_BETA_GRAPH_RESOURCE_ENGINE_MANAGED_PAYLOAD_AND_RESERVED_STORAGE_PLUS_DEVICE) {
        graphResourceScope = MediaRealtimeGraphResourceBudgetScope::
            EngineManagedPayloadAndReservedStoragePlusDevice;
    } else {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "deployment graph memory scope is invalid"));
    }
    encoding.resources = {
        graphResourceScope,
        source.maximum_graph_payload_and_reserved_storage_bytes,
        source.maximum_network_memory_bytes,
        source.maximum_socket_memory_bytes,
        source.resource_authority};
    auto localAddress = parseNumericAddress(source.local_address);
    if (!localAddress) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            localAddress.error());
    }
    encoding.localPorts = {
        localAddress.value().addressFamily(), source.local_address,
        source.local_first_port, source.local_port_count,
        source.local_authority};
    auto maximumReleaseJitter = runningMilliseconds(
        source.maximum_release_jitter_ms);
    if (!maximumReleaseJitter) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            maximumReleaseJitter.error());
    }
    if (auto valid = requireText(
            source.release_jitter_authority,
            "release jitter authority"); !valid) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            valid.error());
    }
    encoding.latency = {
        targetResidence.value(), maximumResidence.value(),
        source.latency_authority, maximumReleaseJitter.value(),
        source.release_jitter_authority};
    MediaRealtimeTransmitEvidencePolicy evidencePolicy =
        MediaRealtimeTransmitEvidencePolicy::Unknown;
    if (source.tx_evidence_policy == MT_BETA_TX_EVIDENCE_DISABLED) {
        evidencePolicy = MediaRealtimeTransmitEvidencePolicy::Disabled;
    } else if (source.tx_evidence_policy == MT_BETA_TX_EVIDENCE_REPORT) {
        evidencePolicy = MediaRealtimeTransmitEvidencePolicy::Report;
    } else if (source.tx_evidence_policy == MT_BETA_TX_EVIDENCE_FAIL) {
        evidencePolicy = MediaRealtimeTransmitEvidencePolicy::Fail;
    }
    encoding.observation = {
        source.observation_run_datagrams,
        drainResidence.value(),
        evidencePolicy,
        source.observation_authority};
    const bool hasReceiverTiming = source.receiver_timing_authority != nullptr ||
        source.receiver_transport_decode_lead_ms != 0;
    if (hasReceiverTiming) {
        if (auto valid = requireText(
                source.receiver_timing_authority,
                "receiver timing authority"); !valid) {
            return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
                valid.error());
        }
        auto decodeLead = runningMilliseconds(
            source.receiver_transport_decode_lead_ms);
        if (!decodeLead) {
            return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
                decodeLead.error());
        }
        encoding.receiverTiming = MediaRealtimeReceiverTimingCapability{
            decodeLead.value(),
            source.receiver_timing_authority};
    }
    return MediaRealtimeDeploymentEnvelope::decode(std::move(encoding));
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
    ffmpeg::graph::MediaRealtimeDeploymentEnvelope deployment,
    RateControl rateControl,
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
    , m_deployment(std::move(deployment))
    , m_rateControl(rateControl)
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
        config->output.gop_frames > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
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
    auto deployment = copyDeployment(config->deployment);
    if (!deployment) {
        return ::media::Result<MediaRealtimeBetaOwnedConfig>::failure(
            deployment.error());
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
            std::move(deployment).value(),
            rateControl.value(),
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
const ffmpeg::graph::MediaRealtimeDeploymentEnvelope&
MediaRealtimeBetaOwnedConfig::deployment() const noexcept { return m_deployment; }
const MediaRealtimeBetaOwnedConfig::RateControl& MediaRealtimeBetaOwnedConfig::rateControl() const noexcept { return m_rateControl; }
ffmpeg::graph::MediaIpAddressFamily MediaRealtimeBetaOwnedConfig::addressFamily() const noexcept { return m_addressFamily; }
mt_beta_realtime_event_callback MediaRealtimeBetaOwnedConfig::eventCallback() const noexcept { return m_eventCallback; }
void* MediaRealtimeBetaOwnedConfig::eventUserData() const noexcept { return m_eventUserData; }

} // namespace media::beta
