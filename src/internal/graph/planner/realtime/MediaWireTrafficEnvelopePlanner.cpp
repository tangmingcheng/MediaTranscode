#include "internal/graph/planner/realtime/MediaWireTrafficEnvelopePlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimePlanningArithmetic.h"
#include "internal/graph/planner/realtime/MediaWireBurstGeometry.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t RtpHeaderBytes = 12;
constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t TsPacketBytes = 188;
constexpr std::uint64_t TsPayloadBytes = 184;
constexpr std::uint64_t VideoPesHeaderBytes = 19;
constexpr std::uint64_t AudioPesAndAdtsHeaderBytes = 21;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

::media::Result<std::uint64_t> add(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    return MediaRealtimePlanningArithmetic::add(left, right, fact);
}

::media::Result<std::uint64_t> multiply(
    std::uint64_t left, std::uint64_t right, const char* fact)
{
    return MediaRealtimePlanningArithmetic::multiply(left, right, fact);
}

::media::Result<std::uint64_t> ceilScale(
    std::uint64_t value,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const char* fact)
{
    return MediaRealtimePlanningArithmetic::ceilScale(
        value, numerator, denominator, fact);
}

std::uint64_t ipHeaderBytes(
    const MediaRealtimeDeploymentEnvelope& deployment) noexcept
{
    return deployment.encode().mtu.addressFamily == MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes : Ipv6HeaderBytes;
}

::media::Result<std::uint64_t> rtcpCompoundBytes(
    std::size_t cnameBytes)
{
    return MediaRtcpWireGeometry::compoundPayloadBytes(cnameBytes);
}

struct WireDemand final {
    std::uint64_t sustainedPayload = 0;
    std::uint64_t peakPayload = 0;
    std::uint64_t packetsPerSecond = 0;
    std::uint64_t burstPayload = 0;
    std::uint64_t burstPayloadDatagrams = 0;
    std::uint64_t burstDiscreteDatagrams = 0;
};

template <typename Emission>
::media::Result<WireDemand> elementaryDemand(
    const Emission& emission,
    const MediaScheduledRtpOutputPlan& output)
{
    const auto maximumDatagram = static_cast<std::uint64_t>(
        output.packetization.maximumDatagramBytes());
    const auto& contract = output.packetization.emissionContract();
    if (contract.maximumDatagramsPerAccessUnit() == 0 ||
        contract.maximumAccessUnitPayloadBytes() !=
            emission.maximumAccessUnitPayloadBytes) {
        return ::media::Result<WireDemand>::failure(
            ::media::ErrorInfo::unsupported(
                "elementary RTP requires matching prepared emission and packetization contract"));
    }
    if constexpr (std::is_same_v<Emission,
                  MediaPreparedEncoderEmissionEnvelope>) {
        if (!emission.encodedPacketLayout) {
            return ::media::Result<WireDemand>::failure(
                ::media::ErrorInfo::unsupported(
                    "video RTP requires authoritative opened packet layout"));
        }
    }
    const std::uint64_t fragmentationHeader =
        output.packetization.packetizationMode() ==
                MediaScheduledRtpPacketizationMode::H264AnnexB
            ? 2U
            : output.packetization.packetizationMode() ==
                    MediaScheduledRtpPacketizationMode::HevcAnnexB
                ? 3U : 0U;
    if (maximumDatagram <= RtpHeaderBytes + fragmentationHeader) {
        return ::media::Result<WireDemand>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP MTU cannot carry a fragmented payload"));
    }
    const auto payloadCapacity =
        maximumDatagram - RtpHeaderBytes - fragmentationHeader;
    auto packetRate = ceilScale(
        contract.maximumDatagramsPerAccessUnit(),
        emission.accessUnitsPerSecondNumerator,
        emission.accessUnitsPerSecondDenominator,
        "RTP contracted datagram rate");
    const auto burstPackets = contract.maximumDatagramsPerAccessUnit();
    if (!packetRate) {
        return ::media::Result<WireDemand>::failure(
            packetRate.error());
    }
    auto sustainedHeaders = packetRate
        ? multiply(packetRate.value(), RtpHeaderBytes + fragmentationHeader,
                   "RTP sustained headers")
        : packetRate;
    auto sustained = sustainedHeaders
        ? add(emission.sustainedPayloadBytesPerSecond,
              sustainedHeaders.value(), "RTP sustained payload")
        : sustainedHeaders;
    auto peakHeaders = packetRate
        ? multiply(packetRate.value(), RtpHeaderBytes + fragmentationHeader,
                   "RTP peak headers")
        : packetRate;
    auto peak = peakHeaders
        ? add(emission.peakPayloadBytesPerSecond, peakHeaders.value(),
              "RTP peak payload")
        : peakHeaders;
    auto burstHeaders = multiply(
        burstPackets, RtpHeaderBytes + fragmentationHeader,
        "RTP burst headers");
    auto burst = burstHeaders
        ? add(emission.maximumBurstPayloadBytes, burstHeaders.value(),
              "RTP burst payload")
        : burstHeaders;
    if (!packetRate || !sustained || !peak || !burst) {
        return ::media::Result<WireDemand>::failure(
            !packetRate ? packetRate.error() : !sustained ? sustained.error() :
            !peak ? peak.error() : burst.error());
    }
    return ::media::Result<WireDemand>::success(
        {sustained.value(), peak.value(), packetRate.value(), burst.value(),
         burstPackets});
}

::media::Result<WireDemand> addRtcp(
    WireDemand demand,
    std::string_view cname,
    const MediaRtcpReportingPolicy& reporting)
{
    auto compound = rtcpCompoundBytes(cname.size());
    const auto intervalNs = reporting.minimumAdmissionInterval().nanoseconds();
    auto rate = compound && intervalNs > 0
        ? ceilScale(compound.value(), NanosecondsPerSecond,
                    static_cast<std::uint64_t>(intervalNs), "RTCP rate")
        : ::media::Result<std::uint64_t>::failure(
              ::media::ErrorInfo::notInitialized(
                  "RTCP schedule requires compound bytes and interval"));
    if (!compound || !rate) {
        return ::media::Result<WireDemand>::failure(
            !compound ? compound.error() : rate.error());
    }
    auto sustained = add(demand.sustainedPayload, rate.value(),
                         "RTP and RTCP sustained bytes");
    auto peak = add(demand.peakPayload, rate.value(),
                    "RTP and RTCP peak bytes");
    auto burst = add(demand.burstPayload,
                     compound.value() + MediaRtcpWireGeometry::byePayloadBytes(),
                     "RTP and RTCP burst bytes");
    auto packets = add(demand.packetsPerSecond, 1U,
                       "RTP and RTCP packet rate");
    auto burstDiscreteDatagrams = add(
        demand.burstDiscreteDatagrams, 2U,
        "RTP and RTCP discrete burst datagrams");
    if (!sustained || !peak || !burst || !packets ||
        !burstDiscreteDatagrams) {
        return ::media::Result<WireDemand>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !burst ? burst.error() : !packets ? packets.error() :
            burstDiscreteDatagrams.error());
    }
    return ::media::Result<WireDemand>::success(
        {sustained.value(), peak.value(), packets.value(), burst.value(),
         demand.burstPayloadDatagrams, burstDiscreteDatagrams.value()});
}

::media::Result<WireDemand> combine(WireDemand left, WireDemand right)
{
    auto sustained = add(left.sustainedPayload, right.sustainedPayload,
                         "aggregate sustained payload");
    auto peak = add(left.peakPayload, right.peakPayload,
                    "aggregate peak payload");
    auto packets = add(left.packetsPerSecond, right.packetsPerSecond,
                       "aggregate packet rate");
    auto burst = add(left.burstPayload, right.burstPayload,
                     "aggregate burst payload");
    auto burstPayloadDatagrams = add(
        left.burstPayloadDatagrams, right.burstPayloadDatagrams,
        "aggregate burst payload datagrams");
    auto burstDiscreteDatagrams = add(
        left.burstDiscreteDatagrams, right.burstDiscreteDatagrams,
        "aggregate burst discrete datagrams");
    if (!sustained || !peak || !packets || !burst ||
        !burstPayloadDatagrams || !burstDiscreteDatagrams) {
        return ::media::Result<WireDemand>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !packets ? packets.error() : !burst ? burst.error() :
            !burstPayloadDatagrams ? burstPayloadDatagrams.error() :
            burstDiscreteDatagrams.error());
    }
    return ::media::Result<WireDemand>::success(
        {sustained.value(), peak.value(), packets.value(), burst.value(),
         burstPayloadDatagrams.value(), burstDiscreteDatagrams.value()});
}

::media::Result<MediaWireTrafficEnvelope> finish(
    const MediaRealtimeDeploymentEnvelope& deployment,
    WireDemand demand,
    std::uint64_t maximumUdpPayload,
    std::string authority)
{
    const auto networkHeader = ipHeaderBytes(deployment) + UdpHeaderBytes;
    auto sustainedHeaders = multiply(
        demand.packetsPerSecond, networkHeader, "network sustained headers");
    auto sustained = sustainedHeaders
        ? add(demand.sustainedPayload, sustainedHeaders.value(),
              "sustained wire demand") : sustainedHeaders;
    auto peakHeaders = multiply(
        demand.packetsPerSecond, networkHeader, "network peak headers");
    auto peak = peakHeaders
        ? add(demand.peakPayload, peakHeaders.value(), "peak wire demand")
        : peakHeaders;
    auto burst = MediaWireBurstGeometry::create(
        demand.burstPayload, demand.burstPayloadDatagrams,
        demand.burstDiscreteDatagrams, maximumUdpPayload, networkHeader);
    auto maximumWire = add(maximumUdpPayload, networkHeader,
                           "maximum wire datagram");
    if (!sustained || !peak || !burst || !maximumWire ||
        maximumUdpPayload == 0) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !burst ? burst.error() : !maximumWire ? maximumWire.error() :
            ::media::ErrorInfo::invalidArgument("UDP payload is zero"));
    }
    return ::media::Result<MediaWireTrafficEnvelope>::success({
        sustained.value(), peak.value(), demand.packetsPerSecond,
        burst.value().wireBytes, burst.value().datagramCount,
        maximumUdpPayload, maximumWire.value(),
        std::move(authority)});
}

template <typename Emission>
::media::Result<WireDemand> tsStreamDemand(
    const Emission& emission,
    std::uint64_t protocolHeaderBytes)
{
    auto perAuSustained = ceilScale(
        emission.sustainedPayloadBytesPerSecond,
        emission.accessUnitsPerSecondDenominator,
        emission.accessUnitsPerSecondNumerator,
        "TS sustained bytes per access unit");
    auto perAuPeak = ceilScale(
        emission.peakPayloadBytesPerSecond,
        emission.accessUnitsPerSecondDenominator,
        emission.accessUnitsPerSecondNumerator,
        "TS peak bytes per access unit");
    auto sustainedPayload = perAuSustained
        ? add(perAuSustained.value(), protocolHeaderBytes,
              "TS sustained access-unit headers") : perAuSustained;
    auto peakPayload = perAuPeak
        ? add(perAuPeak.value(), protocolHeaderBytes,
              "TS peak access-unit headers") : perAuPeak;
    auto burstPayload = add(
        emission.maximumBurstPayloadBytes, protocolHeaderBytes,
        "TS burst access-unit headers");
    auto sustainedPackets = sustainedPayload
        ? ceilScale(sustainedPayload.value(), 1, TsPayloadBytes,
                    "TS sustained packets per access unit")
        : sustainedPayload;
    auto peakPackets = peakPayload
        ? ceilScale(peakPayload.value(), 1, TsPayloadBytes,
                    "TS peak packets per access unit") : peakPayload;
    auto burstPackets = burstPayload
        ? ceilScale(burstPayload.value(), 1, TsPayloadBytes,
                    "TS burst packets") : burstPayload;
    auto sustainedPacketBytes = sustainedPackets
        ? multiply(sustainedPackets.value(), TsPacketBytes,
                   "TS sustained packet bytes")
        : sustainedPackets;
    auto peakPacketBytes = peakPackets
        ? multiply(peakPackets.value(), TsPacketBytes,
                   "TS peak packet bytes")
        : peakPackets;
    auto sustainedTs = sustainedPacketBytes
        ? ceilScale(sustainedPacketBytes.value(),
                    emission.accessUnitsPerSecondNumerator,
                    emission.accessUnitsPerSecondDenominator,
                    "TS sustained stream bytes") : sustainedPacketBytes;
    auto peakTs = peakPacketBytes
        ? ceilScale(peakPacketBytes.value(),
                    emission.accessUnitsPerSecondNumerator,
                    emission.accessUnitsPerSecondDenominator,
                    "TS peak stream bytes") : peakPacketBytes;
    auto unitRate = ceilScale(
        1, emission.accessUnitsPerSecondNumerator,
        emission.accessUnitsPerSecondDenominator,
        "TS access-unit rate");
    auto burstTs = burstPackets
        ? multiply(burstPackets.value(), TsPacketBytes, "TS burst bytes")
        : burstPackets;
    if (!sustainedTs || !peakTs || !unitRate || !burstTs) {
        return ::media::Result<WireDemand>::failure(
            !sustainedTs ? sustainedTs.error() : !peakTs ? peakTs.error() :
            !unitRate ? unitRate.error() : burstTs.error());
    }
    return ::media::Result<WireDemand>::success(
        {sustainedTs.value(), peakTs.value(), unitRate.value(),
         burstTs.value(), burstPackets.value(), 0U});
}

::media::Result<WireDemand> tsDemand(
    const MediaPreparedRealtimeEmissionSet& emission,
    const MediaTsMuxPlan& mux,
    bool rtp)
{
    auto demand = tsStreamDemand(emission.video, VideoPesHeaderBytes);
    if (!demand) return demand;
    if (emission.audio) {
        auto audio = tsStreamDemand(
            *emission.audio, AudioPesAndAdtsHeaderBytes);
        demand = audio ? combine(demand.value(), audio.value()) : audio;
        if (!demand) return demand;
    }
    const auto psiNs =
        mux.timingPolicy().psiRepeatInterval().value.nanoseconds();
    const auto pcrNs = mux.timingPolicy().pcrInterval().value.nanoseconds();
    if (psiNs <= 0 || pcrNs <= 0) {
        return ::media::Result<WireDemand>::failure(
            ::media::ErrorInfo::notInitialized(
                "TS maintenance cadence is unavailable"));
    }
    auto psiEvents = ceilScale(
        1, NanosecondsPerSecond, static_cast<std::uint64_t>(psiNs),
        "PAT/PMT cadence");
    auto pcrEvents = ceilScale(
        1, NanosecondsPerSecond, static_cast<std::uint64_t>(pcrNs),
        "PCR cadence");
    auto maintenanceEvents = psiEvents && pcrEvents
        ? add(psiEvents.value(), pcrEvents.value(), "TS maintenance events")
        : (!psiEvents ? psiEvents : pcrEvents);
    auto patPmtPackets = psiEvents
        ? multiply(psiEvents.value(), 2U, "PAT/PMT packet rate")
        : psiEvents;
    auto patPmtBytes = patPmtPackets
        ? multiply(patPmtPackets.value(), TsPacketBytes,
                   "PAT/PMT byte rate")
        : patPmtPackets;
    auto pcrBytes = pcrEvents
        ? multiply(pcrEvents.value(), TsPacketBytes, "PCR byte rate")
        : pcrEvents;
    auto maintenanceBytes = patPmtBytes && pcrBytes
        ? add(patPmtBytes.value(), pcrBytes.value(),
              "TS maintenance bytes")
        : (!patPmtBytes ? patPmtBytes : pcrBytes);
    if (!maintenanceEvents || !maintenanceBytes) {
        return ::media::Result<WireDemand>::failure(
            !maintenanceEvents ? maintenanceEvents.error() :
            maintenanceBytes.error());
    }
    auto sustained = add(demand.value().sustainedPayload,
                         maintenanceBytes.value(),
                         "TS sustained maintenance bytes");
    auto peak = add(demand.value().peakPayload,
                    maintenanceBytes.value(),
                    "TS peak maintenance bytes");
    auto maintenanceBurst = multiply(3U, TsPacketBytes,
                                     "TS maintenance burst bytes");
    auto burst = maintenanceBurst
        ? add(demand.value().burstPayload, maintenanceBurst.value(),
              "TS burst maintenance bytes")
        : maintenanceBurst;
    auto tsPayloadPerDatagram = multiply(
        static_cast<std::uint64_t>(
            mux.parameters().maximumPacketsPerDatagram),
        TsPacketBytes, "TS payload per datagram");
    if (!sustained || !peak || !burst || !tsPayloadPerDatagram) {
        return ::media::Result<WireDemand>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !burst ? burst.error() : tsPayloadPerDatagram.error());
    }
    demand.value().sustainedPayload = sustained.value();
    demand.value().peakPayload = peak.value();
    demand.value().burstPayload = burst.value();
    auto burstDatagrams = ceilScale(
        demand.value().burstPayload, 1, tsPayloadPerDatagram.value(),
        "TS burst datagram count");
    if (!burstDatagrams) {
        return ::media::Result<WireDemand>::failure(burstDatagrams.error());
    }
    demand.value().burstPayloadDatagrams = burstDatagrams.value();
    auto datagrams = ceilScale(
        demand.value().peakPayload, 1, tsPayloadPerDatagram.value(),
        "TS datagram rate");
    if (!datagrams) return ::media::Result<WireDemand>::failure(datagrams.error());
    auto mediaAndDatagrams = add(
        demand.value().packetsPerSecond, datagrams.value(),
        "TS media datagram rate");
    auto totalDatagrams = mediaAndDatagrams
        ? add(mediaAndDatagrams.value(), maintenanceEvents.value(),
              "TS aggregate datagram rate")
        : mediaAndDatagrams;
    if (!totalDatagrams) {
        return ::media::Result<WireDemand>::failure(totalDatagrams.error());
    }
    demand.value().packetsPerSecond = totalDatagrams.value();
    if (rtp) {
        auto rtpHeaders = multiply(
            demand.value().packetsPerSecond, RtpHeaderBytes,
            "MP2T RTP sustained headers");
        if (!rtpHeaders) {
            return ::media::Result<WireDemand>::failure(rtpHeaders.error());
        }
        auto burstHeaders = multiply(
            demand.value().burstPayloadDatagrams, RtpHeaderBytes,
            "MP2T RTP burst headers");
        auto withSustainedHeaders = add(
            demand.value().sustainedPayload, rtpHeaders.value(),
            "MP2T RTP sustained bytes");
        auto withPeakHeaders = add(
            demand.value().peakPayload, rtpHeaders.value(),
            "MP2T RTP peak bytes");
        auto withBurstHeaders = burstHeaders
            ? add(demand.value().burstPayload, burstHeaders.value(),
                  "MP2T RTP burst bytes")
            : burstHeaders;
        if (!burstHeaders || !withSustainedHeaders || !withPeakHeaders ||
            !withBurstHeaders) {
            return ::media::Result<WireDemand>::failure(
                !burstHeaders ? burstHeaders.error() :
                !withSustainedHeaders ? withSustainedHeaders.error() :
                !withPeakHeaders ? withPeakHeaders.error() :
                withBurstHeaders.error());
        }
        demand.value().sustainedPayload = withSustainedHeaders.value();
        demand.value().peakPayload = withPeakHeaders.value();
        demand.value().burstPayload = withBurstHeaders.value();
    }
    return demand;
}

} // namespace

::media::Result<MediaWireTrafficEnvelope>
MediaWireTrafficEnvelopePlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedRealtimeEmissionSet& emission,
    const MediaVideoOnlySeparateRtpOutputRuntimePlan& output)
{
    auto video = elementaryDemand(emission.video, output.video);
    auto withRtcp = video
        ? addRtcp(video.value(), output.video.cname,
                  output.video.rtcpReporting)
        : video;
    return withRtcp
        ? finish(deployment, withRtcp.value(),
                 output.video.packetization.maximumDatagramBytes(),
                 emission.video.authority + "+rtp+rtcp")
        : ::media::Result<MediaWireTrafficEnvelope>::failure(
              withRtcp.error());
}

::media::Result<MediaWireTrafficEnvelope>
MediaWireTrafficEnvelopePlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedRealtimeEmissionSet& emission,
    const MediaSeparateRtpOutputRuntimePlan& output)
{
    if (!emission.audio) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V RTP wire planning requires prepared audio emission"));
    }
    auto video = elementaryDemand(emission.video, output.video);
    auto audio = elementaryDemand(*emission.audio, output.audio);
    auto videoRtcp = video
        ? addRtcp(video.value(), output.video.cname,
                  output.video.rtcpReporting)
        : video;
    auto audioRtcp = audio
        ? addRtcp(audio.value(), output.audio.cname,
                  output.audio.rtcpReporting)
        : audio;
    auto combined = videoRtcp && audioRtcp
        ? combine(videoRtcp.value(), audioRtcp.value())
        : ::media::Result<WireDemand>::failure(
              !videoRtcp ? videoRtcp.error() : audioRtcp.error());
    return combined
        ? finish(deployment, combined.value(),
                 (std::min)(output.video.packetization.maximumDatagramBytes(),
                            output.audio.packetization.maximumDatagramBytes()),
                 emission.video.authority + "+" + emission.audio->authority +
                     "+rtp+rtcp")
        : ::media::Result<MediaWireTrafficEnvelope>::failure(combined.error());
}

::media::Result<MediaWireTrafficEnvelope>
MediaWireTrafficEnvelopePlanner::plan(
    const MediaRealtimeDeploymentEnvelope& deployment,
    const MediaPreparedRealtimeEmissionSet& emission,
    const MediaProjectMpegTsRuntimeOutputPlan& output)
{
    const auto& mux = output.protocol.muxPlan();
    const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(&output.transport);
    auto demand = tsDemand(emission, mux, rtp != nullptr);
    if (!demand) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(demand.error());
    }
    if (rtp) {
        auto rtcp = addRtcp(
            demand.value(), rtp->cname(), rtp->rtcpReporting());
        if (!rtcp) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                rtcp.error());
        }
        demand = std::move(rtcp);
    }
    auto maximumTsPayload = multiply(
        static_cast<std::uint64_t>(
            mux.parameters().maximumPacketsPerDatagram),
        TsPacketBytes, "maximum TS UDP payload");
    auto maximumUdpPayload = maximumTsPayload
        ? add(maximumTsPayload.value(), rtp ? RtpHeaderBytes : 0U,
              "maximum MPEG-TS UDP payload")
        : maximumTsPayload;
    if (!maximumUdpPayload) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            maximumUdpPayload.error());
    }
    return finish(deployment, demand.value(), maximumUdpPayload.value(),
                  emission.video.authority +
                      (emission.audio ? "+" + emission.audio->authority : "") +
                      "+mpegts-discrete-emission");
}

} // namespace media::ffmpeg::graph
