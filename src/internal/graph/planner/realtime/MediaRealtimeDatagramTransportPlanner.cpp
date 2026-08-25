#include "internal/graph/planner/realtime/MediaRealtimeDatagramTransportPlanner.h"

#include "internal/graph/model/MediaNumericIpAddress.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <utility>
#include <variant>
#include <new>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t BitsPerByte = 8;
constexpr std::uint64_t Kilo = 1000;
constexpr std::uint64_t RtpHeaderBytes = 12;
constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t TsPacketBytes = 188;
constexpr std::uint64_t TsPayloadBytes = 184;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

::media::Result<std::uint64_t> checkedAdd(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " is not representable"));
    }
    return ::media::Result<std::uint64_t>::success(left + right);
}

::media::Result<std::uint64_t> checkedCeilScale(
    std::uint64_t value,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const char* fact)
{
    if (denominator == 0 || numerator == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " has an invalid ratio"));
    }
    const auto quotient = value / denominator;
    const auto remainder = value % denominator;
    if (quotient > (std::numeric_limits<std::uint64_t>::max)() / numerator ||
        remainder > (std::numeric_limits<std::uint64_t>::max)() / numerator) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string(fact) + " is not representable"));
    }
    const auto whole = quotient * numerator;
    const auto scaledRemainder = remainder * numerator;
    const auto fractional = scaledRemainder / denominator +
        (scaledRemainder % denominator != 0 ? 1U : 0U);
    return checkedAdd(whole, fractional, fact);
}

::media::Result<MediaPreparedEncoderEmissionEnvelope> videoEmission(
    const MediaPipelinePlan& pipeline,
    MediaRational frameRate)
{
    const auto& emission = pipeline.selected.encoder.preparedEmission;
    if (!pipeline.enabled || pipeline.branchMode != MediaBranchMode::TranscodeFrame ||
        !emission || emission->sustainedPayloadBytesPerSecond == 0 ||
        emission->peakPayloadBytesPerSecond <
            emission->sustainedPayloadBytesPerSecond ||
        emission->maximumAccessUnitPayloadBytes == 0 ||
        emission->maximumBurstPayloadBytes == 0 ||
        emission->authority.empty() || emission->backend.empty() ||
        !frameRate.isKnown() || frameRate.num <= 0 || frameRate.den <= 0) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire traffic planning requires opened encoder emission readback"));
    }
    if (emission->accessUnitsPerSecondNumerator !=
            static_cast<std::uint64_t>(frameRate.num) ||
        emission->accessUnitsPerSecondDenominator !=
            static_cast<std::uint64_t>(frameRate.den)) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            ::media::ErrorInfo::invalidArgument(
                "opened encoder cadence readback conflicts with output planning"));
    }
    return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::success(
        *emission);
}

::media::Result<MediaPreparedEncoderEmissionEnvelope> audioEmission(
    const MediaAudioPipelinePlan& pipeline)
{
    if (!pipeline.enabled || !pipeline.resolvedOutput ||
        !pipeline.resolvedOutput->bitrateKbps() ||
        *pipeline.resolvedOutput->bitrateKbps() <= 0 ||
        pipeline.resolvedOutput->sampleRate() <= 0 ||
        pipeline.resolvedOutput->codecFrameSamples() <= 0) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            ::media::ErrorInfo::notInitialized(
                "wire traffic planning requires prepared audio emission and cadence readback"));
    }
    const auto targetKbps = *pipeline.resolvedOutput->bitrateKbps();
    const auto peakKbps = pipeline.resolvedOutput->maxBitrateKbps()
        .value_or(targetKbps);
    auto sustained = checkedCeilScale(
        static_cast<std::uint64_t>(targetKbps), Kilo, BitsPerByte,
        "audio sustained byte rate");
    auto peak = checkedCeilScale(
        static_cast<std::uint64_t>(peakKbps), Kilo, BitsPerByte,
        "audio peak byte rate");
    if (!sustained || !peak) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            !sustained ? sustained.error() : peak.error());
    }
    auto accessUnit = checkedCeilScale(
        peak.value(),
        static_cast<std::uint64_t>(
            pipeline.resolvedOutput->codecFrameSamples()),
        static_cast<std::uint64_t>(pipeline.resolvedOutput->sampleRate()),
        "audio access-unit bytes");
    if (!accessUnit) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            accessUnit.error());
    }
    const auto burst = pipeline.resolvedOutput->bufferSizeKbits()
        ? checkedCeilScale(
              static_cast<std::uint64_t>(
                  *pipeline.resolvedOutput->bufferSizeKbits()),
              Kilo, BitsPerByte, "audio buffer bytes")
        : ::media::Result<std::uint64_t>::success(accessUnit.value());
    if (!burst) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            burst.error());
    }
    return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::success({
        sustained.value(), peak.value(), accessUnit.value(), burst.value(),
        static_cast<std::uint64_t>(pipeline.resolvedOutput->sampleRate()),
        static_cast<std::uint64_t>(
            pipeline.resolvedOutput->codecFrameSamples()),
        pipeline.resolvedOutput->encoderName() + ":prepared-audio-output"});
}

::media::Result<MediaPreparedEncoderEmissionEnvelope> aggregateEmission(
    MediaPreparedEncoderEmissionEnvelope video,
    const MediaAudioPipelinePlan* audio)
{
    if (!audio) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::success(
            std::move(video));
    }
    auto plannedAudio = audioEmission(*audio);
    if (!plannedAudio) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            plannedAudio.error());
    }
    auto sustained = checkedAdd(
        video.sustainedPayloadBytesPerSecond,
        plannedAudio.value().sustainedPayloadBytesPerSecond,
        "aggregate encoded sustained byte rate");
    auto peak = checkedAdd(
        video.peakPayloadBytesPerSecond,
        plannedAudio.value().peakPayloadBytesPerSecond,
        "aggregate encoded peak byte rate");
    auto accessUnit = checkedAdd(
        video.maximumAccessUnitPayloadBytes,
        plannedAudio.value().maximumAccessUnitPayloadBytes,
        "aggregate access-unit bytes");
    auto burst = checkedAdd(
        video.maximumBurstPayloadBytes,
        plannedAudio.value().maximumBurstPayloadBytes,
        "aggregate encoded burst bytes");
    if (!sustained || !peak || !accessUnit || !burst) {
        return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !accessUnit ? accessUnit.error() : burst.error());
    }
    video.sustainedPayloadBytesPerSecond = sustained.value();
    video.peakPayloadBytesPerSecond = peak.value();
    video.maximumAccessUnitPayloadBytes = accessUnit.value();
    video.maximumBurstPayloadBytes = burst.value();
    video.authority += "+" + plannedAudio.value().authority;
    return ::media::Result<MediaPreparedEncoderEmissionEnvelope>::success(
        std::move(video));
}

::media::Result<MediaWireTrafficEnvelope> wireEnvelope(
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedEncoderEmissionEnvelope& encoded,
    std::uint64_t protocolPayloadBytes,
    std::uint64_t protocolHeaderBytes,
    bool mpegTs,
    const MediaTsMuxPlan* muxPlan)
{
    const auto& facts = deployment.encode();
    const auto ipHeaderBytes = facts.mtu.addressFamily ==
            MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes
        : Ipv6HeaderBytes;
    const auto wireHeaderBytes = ipHeaderBytes + UdpHeaderBytes +
        protocolHeaderBytes;
    auto scalePayload = [&](std::uint64_t value, const char* fact) {
        return mpegTs
            ? checkedCeilScale(value, TsPacketBytes, TsPayloadBytes, fact)
            : ::media::Result<std::uint64_t>::success(value);
    };
    auto sustainedPayload = scalePayload(
        encoded.sustainedPayloadBytesPerSecond,
        "mux sustained payload rate");
    auto peakPayload = scalePayload(
        encoded.peakPayloadBytesPerSecond,
        "mux peak payload rate");
    auto burstPayload = scalePayload(
        encoded.maximumBurstPayloadBytes,
        "mux burst payload");
    if (!sustainedPayload || !peakPayload || !burstPayload ||
        protocolPayloadBytes == 0) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !sustainedPayload ? sustainedPayload.error() :
            !peakPayload ? peakPayload.error() :
            !burstPayload ? burstPayload.error() :
            ::media::ErrorInfo::invalidArgument(
                "wire protocol payload capacity is zero"));
    }
    if (mpegTs && muxPlan) {
        const auto repeatNs = muxPlan->parameters().psiRepeatInterval.nanoseconds();
        if (repeatNs <= 0) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS PSI cadence is unavailable"));
        }
        auto maintenance = checkedCeilScale(
            2U * TsPacketBytes, NanosecondsPerSecond,
            static_cast<std::uint64_t>(repeatNs),
            "MPEG-TS PSI wire payload rate");
        if (!maintenance) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                maintenance.error());
        }
        sustainedPayload = checkedAdd(
            sustainedPayload.value(), maintenance.value(),
            "MPEG-TS sustained payload and PSI");
        peakPayload = checkedAdd(
            peakPayload.value(), maintenance.value(),
            "MPEG-TS peak payload and PSI");
        burstPayload = checkedAdd(
            burstPayload.value(), 2U * TsPacketBytes,
            "MPEG-TS burst payload and PSI");
        if (!sustainedPayload || !peakPayload || !burstPayload) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !sustainedPayload ? sustainedPayload.error() :
                !peakPayload ? peakPayload.error() : burstPayload.error());
        }
    }
    auto addHeaders = [&](std::uint64_t payload, const char* fact) {
        auto packets = checkedCeilScale(payload, 1, protocolPayloadBytes, fact);
        auto headers = packets
            ? checkedCeilScale(packets.value(), wireHeaderBytes, 1, fact)
            : packets;
        return headers ? checkedAdd(payload, headers.value(), fact) : headers;
    };
    auto sustainedWire = addHeaders(
        sustainedPayload.value(), "sustained wire demand");
    auto peakWire = addHeaders(peakPayload.value(), "peak wire demand");
    auto burstWire = addHeaders(burstPayload.value(), "wire burst demand");
    auto peakPackets = checkedCeilScale(
        peakPayload.value(), 1, protocolPayloadBytes,
        "peak Datagram rate");
    auto maximumWireDatagram = checkedAdd(
        protocolPayloadBytes, wireHeaderBytes,
        "maximum wire Datagram bytes");
    if (!sustainedWire || !peakWire || !burstWire || !peakPackets ||
        !maximumWireDatagram) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !sustainedWire ? sustainedWire.error() : !peakWire ? peakWire.error() :
            !burstWire ? burstWire.error() : !peakPackets ? peakPackets.error() :
            maximumWireDatagram.error());
    }
    return ::media::Result<MediaWireTrafficEnvelope>::success({
        sustainedWire.value(), peakWire.value(), peakPackets.value(),
        burstWire.value(), facts.mtu.senderMaximumPayloadBytes,
        maximumWireDatagram.value(), encoded.authority +
            (mpegTs ? "+mpegts-emission" : "+rtp-packetization")});
}

void appendRtpEndpoints(
    std::vector<MediaDatagramRemoteEndpointFact>& endpoints,
    const MediaRtpRemoteEndpointPair& transport,
    MediaDatagramProtocolEndpointRole rtpRole,
    MediaDatagramProtocolEndpointRole rtcpRole)
{
    const auto& rtp = transport.remoteRtpEndpoint();
    const auto& rtcp = transport.remoteRtcpEndpoint();
    const auto nextId = static_cast<std::uint64_t>(endpoints.size()) + 1U;
    endpoints.push_back({nextId, rtpRole, rtp.addressFamily(),
                         rtp.numericAddress(), rtp.port()});
    endpoints.push_back({nextId + 1U, rtcpRole, rtcp.addressFamily(),
                         rtcp.numericAddress(), rtcp.port()});
}

::media::Result<MediaDatagramTransportPlanTemplate> planRtp(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    std::vector<MediaDatagramRemoteEndpointFact> endpoints,
    MediaWireTrafficEnvelope wireTraffic)
{
    return MediaDatagramTransportPlanTemplate::create(
        sessionKey, deployment, std::move(endpoints), std::move(wireTraffic));
}

} // namespace

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaVideoOnlySeparateRtpOutputRuntimePlan& output,
    const MediaPipelinePlan& videoPipeline,
    MediaRational outputFrameRate)
{
    auto encoded = videoEmission(videoPipeline, outputFrameRate);
    const auto maximumDatagramBytes =
        output.video.packetization.maximumDatagramBytes();
    auto wire = !encoded
        ? ::media::Result<MediaWireTrafficEnvelope>::failure(encoded.error())
        : maximumDatagramBytes <= RtpHeaderBytes
        ? ::media::Result<MediaWireTrafficEnvelope>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "elementary RTP MTU cannot carry an RTP payload"))
        : wireEnvelope(
              deployment, encoded.value(),
              maximumDatagramBytes - RtpHeaderBytes, RtpHeaderBytes,
              false, nullptr);
    if (!wire) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            wire.error());
    }
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(2);
        appendRtpEndpoints(
            endpoints, output.video.transport,
            MediaDatagramProtocolEndpointRole::VideoRtp,
            MediaDatagramProtocolEndpointRole::VideoRtcp);
        return planRtp(sessionKey, deployment, std::move(endpoints),
                       std::move(wire).value());
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            ::media::ErrorInfo::allocationFailed(
                "VideoOnly RTP Datagram transport planning"));
    }
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaSeparateRtpOutputRuntimePlan& output,
    const MediaPipelinePlan& videoPipeline,
    MediaRational outputFrameRate,
    const MediaAudioPipelinePlan& audioPipeline)
{
    auto video = videoEmission(videoPipeline, outputFrameRate);
    auto encoded = video
        ? aggregateEmission(std::move(video).value(), &audioPipeline)
        : ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
              video.error());
    const auto maximumDatagramBytes = (std::min)(
        output.video.packetization.maximumDatagramBytes(),
        output.audio.packetization.maximumDatagramBytes());
    auto wire = !encoded
        ? ::media::Result<MediaWireTrafficEnvelope>::failure(encoded.error())
        : maximumDatagramBytes <= RtpHeaderBytes
        ? ::media::Result<MediaWireTrafficEnvelope>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "aggregate RTP MTU cannot carry an RTP payload"))
        : wireEnvelope(
              deployment, encoded.value(),
              maximumDatagramBytes - RtpHeaderBytes, RtpHeaderBytes,
              false, nullptr);
    if (!wire) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            wire.error());
    }
    try {
        std::vector<MediaDatagramRemoteEndpointFact> endpoints;
        endpoints.reserve(4);
        appendRtpEndpoints(
            endpoints, output.video.transport,
            MediaDatagramProtocolEndpointRole::VideoRtp,
            MediaDatagramProtocolEndpointRole::VideoRtcp);
        appendRtpEndpoints(
            endpoints, output.audio.transport,
            MediaDatagramProtocolEndpointRole::AudioRtp,
            MediaDatagramProtocolEndpointRole::AudioRtcp);
        return planRtp(sessionKey, deployment, std::move(endpoints),
                       std::move(wire).value());
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            ::media::ErrorInfo::allocationFailed(
                "A/V RTP Datagram transport planning"));
    }
}

::media::Result<MediaDatagramTransportPlanTemplate>
MediaRealtimeDatagramTransportPlanner::plan(
    const std::string& sessionKey,
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaProjectMpegTsRuntimeOutputPlan& output,
    const MediaPipelinePlan& videoPipeline,
    MediaRational outputFrameRate,
    const MediaAudioPipelinePlan* audioPipeline)
{
    auto video = videoEmission(videoPipeline, outputFrameRate);
    auto encoded = video
        ? aggregateEmission(std::move(video).value(), audioPipeline)
        : ::media::Result<MediaPreparedEncoderEmissionEnvelope>::failure(
              video.error());
    if (!encoded) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            encoded.error());
    }
    const auto& mux = output.protocol.muxPlan();
    const auto rtpOutput = std::holds_alternative<MediaMpegTsRtpOutputPlan>(
        output.transport);
    const auto protocolPayloadBytes =
        static_cast<std::uint64_t>(mux.parameters().maximumPacketsPerDatagram) *
        TsPacketBytes;
    auto wire = wireEnvelope(
        deployment, encoded.value(), protocolPayloadBytes,
        rtpOutput ? RtpHeaderBytes : 0, true, &mux);
    if (!wire) {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            wire.error());
    }
    if (const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
            &output.transport)) {
        try {
            std::vector<MediaDatagramRemoteEndpointFact> endpoints;
            endpoints.reserve(2);
            appendRtpEndpoints(
                endpoints, rtp->transport(),
                MediaDatagramProtocolEndpointRole::MpegTsRtp,
                MediaDatagramProtocolEndpointRole::MpegTsRtcp);
            return planRtp(sessionKey, deployment, std::move(endpoints),
                           std::move(wire).value());
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "MPEG-TS RTP Datagram transport planning"));
        }
    }
    const auto* udp = std::get_if<MediaMpegTsUdpOutputPlan>(
        &output.transport);
    auto parsed = udp
        ? parseRtpUdpUrlEndpoint(udp->url)
        : ::media::Result<MediaRtpUrlEndpoint>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "MPEG-TS transport variant is unavailable"));
    if (!parsed || parsed.value().scheme != "udp") {
        return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
            parsed ? ::media::ErrorInfo::invalidArgument(
                         "MPEG-TS Datagram transport requires udp://")
                   : parsed.error());
    }
    MediaIpAddressFamily family = MediaIpAddressFamily::Ipv4;
    if (!MediaNumericIpAddress::create(family, parsed.value().host)) {
        family = MediaIpAddressFamily::Ipv6;
        if (!MediaNumericIpAddress::create(family, parsed.value().host)) {
            return ::media::Result<MediaDatagramTransportPlanTemplate>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS UDP destination must be a numeric address"));
        }
    }
    return MediaDatagramTransportPlanTemplate::create(
        sessionKey, deployment,
        {{1, MediaDatagramProtocolEndpointRole::MpegTsUdp,
          family, parsed.value().host, parsed.value().port}},
        std::move(wire).value());
}

} // namespace media::ffmpeg::graph
