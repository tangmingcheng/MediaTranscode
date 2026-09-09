#include "media_transcode_beta/MediaRealtimeBetaRequestMapper.h"

#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"
#include "internal/graph/model/MediaNumericIpAddress.h"

#include <cstdint>
#include <limits>
#include <ratio>
#include <string>
#include <utility>
#include <variant>

namespace media::beta {
namespace {

struct CbrRateControl { int targetBitrateKbps; };
struct VbrRateControl {
    int minimumBitrateKbps;
    int targetBitrateKbps;
    int maximumBitrateKbps;
};
using RateControl = std::variant<CbrRateControl, VbrRateControl>;

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

::media::Result<RateControl> copyRateControl(
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
            return ::media::Result<RateControl>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "CBR target bitrate must be positive"));
        }
        auto bitrate = checkedKiloBits(
            output.rate_control.cbr.bitrate_bps, "CBR bitrate bps");
        if (!bitrate) {
            return ::media::Result<RateControl>::failure(
                bitrate.error());
        }
        return ::media::Result<RateControl>::success(
            CbrRateControl{
                bitrate.value() });
    }
    if (output.rate_control_mode != MT_BETA_RATE_CONTROL_VBR) {
        return ::media::Result<RateControl>::failure(
            ::media::ErrorInfo::invalidArgument("rate control mode is unsupported"));
    }

    const auto& vbr = output.rate_control.vbr;
    if (vbr.target_bitrate_bps == 0U || vbr.min_bitrate_bps == 0U ||
        vbr.max_bitrate_bps == 0U || vbr.min_bitrate_bps > vbr.target_bitrate_bps ||
        vbr.target_bitrate_bps > vbr.max_bitrate_bps) {
        return ::media::Result<RateControl>::failure(
                ::media::ErrorInfo::invalidArgument("VBR bitrate range must satisfy min <= target <= max"));
    }
    auto target = checkedKiloBits(vbr.target_bitrate_bps, "VBR target bitrate bps");
    auto minimum = checkedKiloBits(vbr.min_bitrate_bps, "VBR minimum bitrate bps");
    auto maximum = checkedKiloBits(vbr.max_bitrate_bps, "VBR maximum bitrate bps");
    if (!target) return ::media::Result<RateControl>::failure(target.error());
    if (!minimum) return ::media::Result<RateControl>::failure(minimum.error());
    if (!maximum) return ::media::Result<RateControl>::failure(maximum.error());
    return ::media::Result<RateControl>::success(
        VbrRateControl{
            minimum.value(), target.value(), maximum.value() });
}


std::string endpointHost(const ffmpeg::graph::MediaNumericIpAddress& address)
{
    return address.addressFamily() == ffmpeg::graph::MediaIpAddressFamily::Ipv6
        ? "[" + address.presentation() + "]" : address.presentation();
}

std::string codecName(mt_beta_video_codec codec)
{
    return codec == MT_BETA_VIDEO_CODEC_H264 ? "h264" : "hevc";
}

} // namespace

::media::Result<ffmpeg::graph::MediaRealtimeVideoOutputRequest>
MediaRealtimeBetaRequestMapper::mapOutput(const mt_beta_video_output& output)
{
    using Result = ::media::Result<ffmpeg::graph::MediaRealtimeVideoOutputRequest>;
    if (auto status = requireText(output.destination_address, "destination address"); !status)
        return Result::failure(status.error());
    const auto validPositive = [](std::uint32_t value) {
        return value > 0 && value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    };
    if (!output.destination_port || !isVideoCodec(output.codec) ||
        !validPositive(output.width) || !validPositive(output.height) ||
        !validPositive(output.frame_rate_num) || !validPositive(output.frame_rate_den) ||
        !validPositive(output.gop_frames)) {
        return Result::failure(::media::ErrorInfo::invalidArgument("video output facts are invalid"));
    }
    auto destination = parseNumericAddress(output.destination_address);
    if (!destination) return Result::failure(destination.error());
    auto rate = copyRateControl(output);
    if (!rate) return Result::failure(rate.error());
    ffmpeg::graph::MediaRealtimeVideoOutputRequest request;
    using Layout = ffmpeg::graph::RealtimeOutputStreamLayout;
    using Transport = ffmpeg::graph::MediaOutputTransportKind;
    switch (output.protocol) {
    case MT_BETA_OUTPUT_ELEMENTARY_RTP:
        request.output.streamLayout = Layout::SeparateStreams;
        request.output.transport = Transport::RtpAvp;
        break;
    case MT_BETA_OUTPUT_MPEGTS_RTP:
        request.output.streamLayout = Layout::MuxedTransportStream;
        request.output.transport = Transport::RtpAvp;
        break;
    case MT_BETA_OUTPUT_MPEGTS_UDP:
        request.output.streamLayout = Layout::MuxedTransportStream;
        request.output.transport = Transport::UdpDatagrams;
        break;
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument("output protocol is unsupported"));
    }
    const auto host = endpointHost(destination.value());
    if (request.output.transport == Transport::RtpAvp) {
        if (output.destination_port % 2U != 0U) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP output requires an even RTP port and its following RTCP port"));
        }
        request.output.host = host;
        request.output.basePort = output.destination_port;
    } else {
        request.output.url = "udp://" + host + ":" + std::to_string(output.destination_port);
    }
    auto& video = request.video;
    video.codecName = codecName(output.codec);
    video.width = static_cast<int>(output.width);
    video.height = static_cast<int>(output.height);
    video.frameRate.numerator = static_cast<int>(output.frame_rate_num);
    video.frameRate.denominator = static_cast<int>(output.frame_rate_den);
    video.gop = static_cast<int>(output.gop_frames);
    if (const auto* cbr = std::get_if<CbrRateControl>(&rate.value())) {
        video.rateControl = ffmpeg::graph::MediaRateControlMode::Cbr;
        video.bitrateKbps = cbr->targetBitrateKbps;
    } else {
        const auto& vbr = std::get<VbrRateControl>(rate.value());
        video.rateControl = ffmpeg::graph::MediaRateControlMode::Vbr;
        video.bitrateKbps = vbr.targetBitrateKbps;
        video.minBitrateKbps = vbr.minimumBitrateKbps;
        video.maxBitrateKbps = vbr.maximumBitrateKbps;
    }
    return Result::success(std::move(request));
}

::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>
MediaRealtimeBetaRequestMapper::map(const mt_beta_realtime_config& config)
{
    using Result = ::media::Result<ffmpeg::graph::MediaRealtimeRtpTranscodeRequest>;
    if (auto status = requireText(config.media_id, "media id"); !status)
        return Result::failure(status.error());
    if (config.deployment.provisioned_egress_capacity_bps < 8U ||
        !config.deployment.maximum_wire_residence_ms) {
        return Result::failure(::media::ErrorInfo::invalidArgument("deployment facts are invalid"));
    }
    auto output = mapOutput(config.initial_output);
    if (!output) return Result::failure(output.error());
    ffmpeg::graph::MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = config.media_id;
    request.output = std::move(output.value().output);
    request.parameters.video = std::move(output.value().video);
    const auto& profile = MediaRealtimeBetaFixedProfile::current();
    request.parameters.execution.streamSet = profile.streamSet;
    request.input.openTimeoutMs = profile.openTimeoutMs;
    request.input.readTimeoutMs = profile.readTimeoutMs;
    request.input.analyzeDurationUs = profile.analyzeDurationUs;
    request.input.probeSizeBytes = profile.probeSizeBytes;
    using InputType = ffmpeg::graph::RealtimeInputType;
    switch (config.input.kind) {
    case MT_BETA_INPUT_RTP_VIDEO: {
        const auto& input = config.input.source.rtp;
        if (auto status = requireText(input.bind_address, "bind address"); !status)
            return Result::failure(status.error());
        if (!input.port || input.payload_type > 127U || !input.clock_rate ||
            input.clock_rate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
            !isVideoCodec(input.codec)) {
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP session facts are invalid"));
        }
        auto address = parseNumericAddress(input.bind_address);
        if (!address) return Result::failure(address.error());
        request.input.type = InputType::RtpPort;
        request.input.videoRtp.url = "rtp://" + endpointHost(address.value()) + ":" + std::to_string(input.port);
        request.input.videoRtp.codecName = codecName(input.codec);
        request.input.videoRtp.payloadType = input.payload_type;
        request.input.videoRtp.clockRate = static_cast<int>(input.clock_rate);
        if (input.fmtp && *input.fmtp) request.input.videoRtp.fmtp = input.fmtp;
        break;
    }
    case MT_BETA_INPUT_URL: {
        const auto& input = config.input.source.url;
        if (auto status = requireText(input.url, "input URL"); !status)
            return Result::failure(status.error());
        request.input.type = InputType::Url;
        request.input.url = input.url;
        if (input.rtsp_transport) request.input.rtspTransport = input.rtsp_transport;
        break;
    }
    case MT_BETA_INPUT_MPEGTS_UDP: {
        const auto& input = config.input.source.mpegts_udp;
        if (auto status = requireText(input.url, "MPEG-TS UDP URL"); !status)
            return Result::failure(status.error());
        if (!input.maximum_pcr_gap_ms) {
            return Result::failure(::media::ErrorInfo::invalidArgument("source maximum PCR gap must be positive"));
        }
        request.input.type = InputType::MpegTsUdp;
        request.input.url = input.url;
        request.input.mpegTsClock.maximumPcrGap = ffmpeg::graph::MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(input.maximum_pcr_gap_ms) * 1'000'000);
        break;
    }
    default:
        return Result::failure(::media::ErrorInfo::invalidArgument("input kind is unsupported"));
    }
    request.deployment.provisionedEgressCapacityBitsPerSecond = config.deployment.provisioned_egress_capacity_bps;
    request.deployment.maximumWireResidence = ffmpeg::graph::MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(config.deployment.maximum_wire_residence_ms) * 1'000'000);
    return Result::success(std::move(request));
}

} // namespace media::beta
