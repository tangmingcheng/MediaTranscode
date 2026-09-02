#include "internal/graph/planner/realtime/MediaRealtimeDeploymentPlanner.h"

#include "internal/graph/planner/realtime/MediaDatagramPacingRatePlanner.h"

#include "internal/graph/planner/realtime/MediaDatagramRouteProbe.h"
#include "internal/graph/planner/realtime/MediaRealtimeDatagramPayloadPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeTransportTimingPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeNetworkResourceLedgerPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"
#include "internal/graph/protocol/rtp/MediaDeterministicVideoRtpPacketizer.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t Ipv4HeaderBytes = 20;
constexpr std::uint64_t Ipv6HeaderBytes = 40;
constexpr std::uint64_t UdpHeaderBytes = 8;
constexpr std::uint64_t RtpHeaderBytes = 12;
constexpr std::uint64_t RtpAndMaximumFragmentHeaderBytes = 15;
constexpr std::uint64_t TsPacketBytes = 188;
constexpr std::uint64_t TsPayloadBytes = 184;
constexpr std::uint64_t VideoPesHeaderBytes = 19;
constexpr std::uint64_t AudioPesAndAdtsHeaderBytes = 21;
constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

std::uint64_t ceilDivide(std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value / divisor + (value % divisor != 0 ? 1U : 0U);
}

::media::Result<std::uint64_t> accessUnitsPerSecond(
    std::uint64_t numerator, std::uint64_t denominator, const char* fact)
{
    return MediaCheckedArithmetic::ceilScale(
        1, numerator, denominator, fact);
}

struct TsAdmissionDemand final {
    std::uint64_t sustainedBytesPerSecond = 0;
    std::uint64_t peakBytesPerSecond = 0;
    std::uint64_t burstBytes = 0;
    std::uint64_t atomicBytes = 0;
    std::uint64_t accessUnitsPerSecond = 0;
};

template <typename Emission>
::media::Result<TsAdmissionDemand> tsStreamAdmission(
    const Emission& emission,
    std::uint64_t protocolHeaderBytes)
{
    auto perAuSustained = MediaCheckedArithmetic::ceilScale(
        emission.sustainedPayloadBytesPerSecond,
        emission.accessUnitsPerSecondDenominator,
        emission.accessUnitsPerSecondNumerator,
        "admission TS sustained bytes per access unit");
    auto perAuPeak = MediaCheckedArithmetic::ceilScale(
        emission.peakPayloadBytesPerSecond,
        emission.accessUnitsPerSecondDenominator,
        emission.accessUnitsPerSecondNumerator,
        "admission TS peak bytes per access unit");
    auto sustainedPayload = perAuSustained
        ? MediaCheckedArithmetic::add(
              perAuSustained.value(), protocolHeaderBytes,
              "admission TS sustained access-unit headers")
        : perAuSustained;
    auto peakPayload = perAuPeak
        ? MediaCheckedArithmetic::add(
              perAuPeak.value(), protocolHeaderBytes,
              "admission TS peak access-unit headers")
        : perAuPeak;
    auto burstPayload = MediaCheckedArithmetic::add(
        emission.maximumBurstPayloadBytes, protocolHeaderBytes,
        "admission TS burst access-unit headers");
    auto atomicPayload = MediaCheckedArithmetic::add(
        emission.maximumAccessUnitPayloadBytes, protocolHeaderBytes,
        "admission TS atomic access-unit headers");
    auto sustainedPackets = sustainedPayload
        ? MediaCheckedArithmetic::ceilScale(
              sustainedPayload.value(), 1, TsPayloadBytes,
              "admission TS sustained packets per access unit")
        : sustainedPayload;
    auto peakPackets = peakPayload
        ? MediaCheckedArithmetic::ceilScale(
              peakPayload.value(), 1, TsPayloadBytes,
              "admission TS peak packets per access unit")
        : peakPayload;
    auto burstPackets = burstPayload
        ? MediaCheckedArithmetic::ceilScale(
              burstPayload.value(), 1, TsPayloadBytes,
              "admission TS burst packets")
        : burstPayload;
    auto atomicPackets = atomicPayload
        ? MediaCheckedArithmetic::ceilScale(
              atomicPayload.value(), 1, TsPayloadBytes,
              "admission TS atomic packets")
        : atomicPayload;
    auto sustainedPacketBytes = sustainedPackets
        ? MediaCheckedArithmetic::multiply(
              sustainedPackets.value(), TsPacketBytes,
              "admission TS sustained packet bytes")
        : sustainedPackets;
    auto peakPacketBytes = peakPackets
        ? MediaCheckedArithmetic::multiply(
              peakPackets.value(), TsPacketBytes,
              "admission TS peak packet bytes")
        : peakPackets;
    auto sustained = sustainedPacketBytes
        ? MediaCheckedArithmetic::ceilScale(
              sustainedPacketBytes.value(),
              emission.accessUnitsPerSecondNumerator,
              emission.accessUnitsPerSecondDenominator,
              "admission TS sustained stream bytes")
        : sustainedPacketBytes;
    auto peak = peakPacketBytes
        ? MediaCheckedArithmetic::ceilScale(
              peakPacketBytes.value(),
              emission.accessUnitsPerSecondNumerator,
              emission.accessUnitsPerSecondDenominator,
              "admission TS peak stream bytes")
        : peakPacketBytes;
    auto burst = burstPackets
        ? MediaCheckedArithmetic::multiply(
              burstPackets.value(), TsPacketBytes,
              "admission TS burst bytes")
        : burstPackets;
    auto atomic = atomicPackets
        ? MediaCheckedArithmetic::multiply(
              atomicPackets.value(), TsPacketBytes,
              "admission TS atomic bytes")
        : atomicPackets;
    auto unitRate = accessUnitsPerSecond(
        emission.accessUnitsPerSecondNumerator,
        emission.accessUnitsPerSecondDenominator,
        "admission TS access-unit rate");
    if (!sustained || !peak || !burst || !atomic || !unitRate) {
        return ::media::Result<TsAdmissionDemand>::failure(
            !sustained ? sustained.error() : !peak ? peak.error() :
            !burst ? burst.error() : !atomic ? atomic.error() :
            unitRate.error());
    }
    return ::media::Result<TsAdmissionDemand>::success({
        sustained.value(), peak.value(), burst.value(), atomic.value(),
        unitRate.value()});
}

::media::Result<MediaWireTrafficEnvelope> conservativeWire(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaPreparedRealtimeEmissionSet& emission,
    const MediaRealtimeDeploymentMtuFact& mtu)
{
    const auto ipHeader = mtu.addressFamily == MediaIpAddressFamily::Ipv4
        ? Ipv4HeaderBytes : Ipv6HeaderBytes;
    const auto maximumUdpPayload = (std::min)(
        mtu.senderMaximumPayloadBytes,
        mtu.maximumIpPacketBytes - ipHeader - UdpHeaderBytes);
    auto videoUnits = accessUnitsPerSecond(
        emission.video.accessUnitsPerSecondNumerator,
        emission.video.accessUnitsPerSecondDenominator,
        "video access-unit rate");
    auto audioUnits = emission.audio
        ? accessUnitsPerSecond(
              emission.audio->accessUnitsPerSecondNumerator,
              emission.audio->accessUnitsPerSecondDenominator,
              "audio access-unit rate")
        : ::media::Result<std::uint64_t>::success(0);
    if (!videoUnits || !audioUnits) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !videoUnits ? videoUnits.error() : audioUnits.error());
    }
    auto sustainedPayload = MediaCheckedArithmetic::add(
        emission.video.sustainedPayloadBytesPerSecond,
        emission.audio ? emission.audio->sustainedPayloadBytesPerSecond : 0,
        "aggregate sustained prepared payload");
    auto peakPayload = MediaCheckedArithmetic::add(
        emission.video.peakPayloadBytesPerSecond,
        emission.audio ? emission.audio->peakPayloadBytesPerSecond : 0,
        "aggregate peak prepared payload");
    auto burstPayload = MediaCheckedArithmetic::add(
        emission.video.maximumBurstPayloadBytes,
        emission.audio ? emission.audio->maximumBurstPayloadBytes : 0,
        "aggregate prepared burst payload");
    auto atomicPayload = MediaCheckedArithmetic::add(
        emission.video.maximumAccessUnitPayloadBytes,
        emission.audio ? emission.audio->maximumAccessUnitPayloadBytes : 0,
        "aggregate prepared atomic payload");
    if (!sustainedPayload || !peakPayload || !burstPayload ||
        !atomicPayload) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !sustainedPayload ? sustainedPayload.error() :
            !peakPayload ? peakPayload.error() :
            !burstPayload ? burstPayload.error() : atomicPayload.error());
    }
    std::uint64_t packetRate = 0;
    std::uint64_t burstDatagrams = 0;
    std::uint64_t atomicDatagrams = 0;
    std::uint64_t maximumProtocolPayload = maximumUdpPayload;
    std::uint64_t rtcpStreams = 0;
    if (request.output.streamLayout ==
        RealtimeOutputStreamLayout::SeparateStreams) {
        if (maximumUdpPayload <= RtpAndMaximumFragmentHeaderBytes) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                ::media::ErrorInfo::unsupported(
                    "route MTU cannot carry fragmented RTP payload"));
        }
        const auto capacity = maximumUdpPayload -
            RtpAndMaximumFragmentHeaderBytes;
        const auto codecName = canonicalCodecName(
            request.parameters.video.codecName);
        if (codecName != "h264" && codecName != "hevc") {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                ::media::ErrorInfo::unsupported(
                    "separate RTP admission requires H.264 or HEVC output"));
        }
        const auto codec = codecName == "h264"
            ? MediaAnnexBCodec::H264 : MediaAnnexBCodec::Hevc;
        auto videoPacketRate = MediaDeterministicVideoRtpPacketizer::
            maximumDatagramsForPayloadWindow(
                emission.video.peakPayloadBytesPerSecond,
                videoUnits.value(), codec,
                maximumUdpPayload - RtpHeaderBytes);
        auto audioPacketRate = emission.audio
            ? MediaCheckedArithmetic::add(
                  ceilDivide(
                      emission.audio->peakPayloadBytesPerSecond, capacity),
                  audioUnits.value(),
                  "audio RTP payload packet rate")
            : ::media::Result<std::uint64_t>::success(0);
        auto payloadPacketRate = videoPacketRate && audioPacketRate
            ? MediaCheckedArithmetic::add(
                  videoPacketRate.value(), audioPacketRate.value(),
                  "aggregate RTP payload packet rate")
            : (!videoPacketRate ? videoPacketRate : audioPacketRate);
        auto totalPacketRate = payloadPacketRate
            ? MediaCheckedArithmetic::add(
                  payloadPacketRate.value(), emission.audio ? 2U : 1U,
                  "aggregate RTP and RTCP packet rate")
            : payloadPacketRate;
        auto videoBurstDatagrams = MediaDeterministicVideoRtpPacketizer::
            maximumDatagramsForPayloadWindow(
                emission.video.maximumBurstPayloadBytes,
                emission.video.maximumEncoderRetainedFrames, codec,
                maximumUdpPayload - RtpHeaderBytes);
        auto audioBurstDatagrams = emission.audio
            ? MediaCheckedArithmetic::add(
                  ceilDivide(
                      emission.audio->maximumBurstPayloadBytes, capacity),
                  1U, "audio RTP burst boundary")
            : ::media::Result<std::uint64_t>::success(0);
        auto videoAtomicDatagrams = MediaDeterministicVideoRtpPacketizer::
            maximumDatagramsForPayloadWindow(
                emission.video.maximumAccessUnitPayloadBytes, 1U, codec,
                maximumUdpPayload - RtpHeaderBytes);
        auto audioAtomicDatagrams = emission.audio
            ? MediaCheckedArithmetic::add(
                  ceilDivide(
                      emission.audio->maximumAccessUnitPayloadBytes,
                      capacity),
                  1U, "audio RTP atomic access-unit boundary")
            : ::media::Result<std::uint64_t>::success(0);
        auto mediaBurstDatagrams =
            videoBurstDatagrams && audioBurstDatagrams
            ? MediaCheckedArithmetic::add(
                  videoBurstDatagrams.value(), audioBurstDatagrams.value(),
                  "aggregate RTP media burst datagrams")
            : (!videoBurstDatagrams
                   ? videoBurstDatagrams : audioBurstDatagrams);
        auto totalBurstDatagrams = mediaBurstDatagrams
            ? MediaCheckedArithmetic::add(
                  mediaBurstDatagrams.value(), emission.audio ? 4U : 2U,
                  "aggregate RTP and RTCP burst datagrams")
            : mediaBurstDatagrams;
        auto mediaAtomicDatagrams =
            videoAtomicDatagrams && audioAtomicDatagrams
            ? MediaCheckedArithmetic::add(
                  videoAtomicDatagrams.value(),
                  audioAtomicDatagrams.value(),
                  "aggregate RTP atomic media datagrams")
            : (!videoAtomicDatagrams
                   ? videoAtomicDatagrams : audioAtomicDatagrams);
        if (!totalPacketRate || !totalBurstDatagrams ||
            !mediaAtomicDatagrams) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !totalPacketRate ? totalPacketRate.error() :
                !totalBurstDatagrams ? totalBurstDatagrams.error() :
                mediaAtomicDatagrams.error());
        }
        packetRate = totalPacketRate.value();
        burstDatagrams = totalBurstDatagrams.value();
        atomicDatagrams = mediaAtomicDatagrams.value();
        rtcpStreams = emission.audio ? 2U : 1U;
        const auto rtpPacketRate = packetRate - rtcpStreams;
        const auto rtpBurstDatagrams = burstDatagrams - 2U * rtcpStreams;
        auto rtpRateHeaders = MediaCheckedArithmetic::multiply(
            rtpPacketRate, RtpAndMaximumFragmentHeaderBytes,
            "admission RTP header rate");
        auto rtpBurstHeaders = MediaCheckedArithmetic::multiply(
            rtpBurstDatagrams, RtpAndMaximumFragmentHeaderBytes,
            "admission RTP burst headers");
        auto rtpAtomicHeaders = MediaCheckedArithmetic::multiply(
            atomicDatagrams, RtpAndMaximumFragmentHeaderBytes,
            "admission RTP atomic headers");
        sustainedPayload = rtpRateHeaders
            ? MediaCheckedArithmetic::add(
                  sustainedPayload.value(), rtpRateHeaders.value(),
                  "admission RTP sustained bytes")
            : rtpRateHeaders;
        peakPayload = rtpRateHeaders
            ? MediaCheckedArithmetic::add(
                  peakPayload.value(), rtpRateHeaders.value(),
                  "admission RTP peak bytes")
            : rtpRateHeaders;
        burstPayload = rtpBurstHeaders
            ? MediaCheckedArithmetic::add(
                  burstPayload.value(), rtpBurstHeaders.value(),
                  "admission RTP burst bytes")
            : rtpBurstHeaders;
        atomicPayload = rtpAtomicHeaders
            ? MediaCheckedArithmetic::add(
                  atomicPayload.value(), rtpAtomicHeaders.value(),
                  "admission RTP atomic payload")
            : rtpAtomicHeaders;
        if (!sustainedPayload || !peakPayload || !burstPayload ||
            !atomicPayload) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !sustainedPayload ? sustainedPayload.error() :
                !peakPayload ? peakPayload.error() :
                !burstPayload ? burstPayload.error() :
                atomicPayload.error());
        }
    } else {
        auto videoTs = tsStreamAdmission(
            emission.video, VideoPesHeaderBytes);
        auto audioTs = emission.audio
            ? tsStreamAdmission(
                  *emission.audio, AudioPesAndAdtsHeaderBytes)
            : ::media::Result<TsAdmissionDemand>::success({});
        if (!videoTs || !audioTs) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !videoTs ? videoTs.error() : audioTs.error());
        }
        sustainedPayload = MediaCheckedArithmetic::add(
            videoTs.value().sustainedBytesPerSecond,
            audioTs.value().sustainedBytesPerSecond,
            "aggregate admission TS sustained bytes");
        peakPayload = MediaCheckedArithmetic::add(
            videoTs.value().peakBytesPerSecond,
            audioTs.value().peakBytesPerSecond,
            "aggregate admission TS peak bytes");
        burstPayload = MediaCheckedArithmetic::add(
            videoTs.value().burstBytes,
            audioTs.value().burstBytes,
            "aggregate admission TS burst bytes");
        atomicPayload = MediaCheckedArithmetic::add(
            videoTs.value().atomicBytes,
            audioTs.value().atomicBytes,
            "aggregate admission TS atomic bytes");
        auto aggregateTsUnits = MediaCheckedArithmetic::add(
            videoTs.value().accessUnitsPerSecond,
            audioTs.value().accessUnitsPerSecond,
            "aggregate TS access-unit rate");
        auto maintenancePackets = aggregateTsUnits
            ? MediaCheckedArithmetic::multiply(
                  aggregateTsUnits.value(), std::uint64_t{3},
                  "access-unit-cadence PAT PMT PCR admission maintenance")
            : aggregateTsUnits;
        auto maintenanceBytes = maintenancePackets
            ? MediaCheckedArithmetic::multiply(
                  maintenancePackets.value(), TsPacketBytes,
                  "TS maintenance bytes")
            : maintenancePackets;
        if (!sustainedPayload || !peakPayload || !burstPayload ||
            !atomicPayload || !maintenanceBytes) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !sustainedPayload ? sustainedPayload.error() :
                !peakPayload ? peakPayload.error() :
                !burstPayload ? burstPayload.error() :
                !atomicPayload ? atomicPayload.error() :
                maintenanceBytes.error());
        }
        sustainedPayload = MediaCheckedArithmetic::add(
            sustainedPayload.value(), maintenanceBytes.value(),
            "conservative TS sustained bytes");
        peakPayload = MediaCheckedArithmetic::add(
            peakPayload.value(), maintenanceBytes.value(),
            "conservative TS peak bytes");
        auto maintenanceBurst = MediaCheckedArithmetic::multiply(
            std::uint64_t{3}, TsPacketBytes,
            "TS maintenance burst bytes");
        burstPayload = maintenanceBurst
            ? MediaCheckedArithmetic::add(
                  burstPayload.value(), maintenanceBurst.value(),
                  "conservative TS burst bytes")
            : maintenanceBurst;
        atomicPayload = maintenanceBurst
            ? MediaCheckedArithmetic::add(
                  atomicPayload.value(), maintenanceBurst.value(),
                  "conservative TS atomic bytes")
            : maintenanceBurst;
        if (!sustainedPayload || !peakPayload || !burstPayload ||
            !atomicPayload) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !sustainedPayload ? sustainedPayload.error() :
                !peakPayload ? peakPayload.error() :
                !burstPayload ? burstPayload.error() :
                atomicPayload.error());
        }
        const bool rtp = request.output.transport ==
            MediaOutputTransportKind::RtpAvp;
        rtcpStreams = rtp ? 1U : 0U;
        const auto transportHeader = rtp ? std::uint64_t{12} : 0U;
        if (maximumUdpPayload <= transportHeader + TsPacketBytes) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                ::media::ErrorInfo::unsupported(
                    "route MTU cannot carry one complete MPEG-TS packet"));
        }
        const auto packetsPerDatagram =
            (maximumUdpPayload - transportHeader) / TsPacketBytes;
        maximumProtocolPayload = packetsPerDatagram * TsPacketBytes +
            transportHeader;
        auto mediaAndMaintenanceEvents = MediaCheckedArithmetic::multiply(
            aggregateTsUnits.value(), std::uint64_t{2},
            "TS access-unit and maintenance event rate");
        auto payloadAndEventRate = mediaAndMaintenanceEvents
            ? MediaCheckedArithmetic::add(
                  ceilDivide(peakPayload.value(),
                             packetsPerDatagram * TsPacketBytes),
                  mediaAndMaintenanceEvents.value(),
                  "aggregate TS payload and event Datagram rate")
            : mediaAndMaintenanceEvents;
        auto totalPacketRate = payloadAndEventRate
            ? MediaCheckedArithmetic::add(
                  payloadAndEventRate.value(), rtp ? 1U : 0U,
                  "aggregate MPEG-TS and RTCP packet rate")
            : payloadAndEventRate;
        auto totalBurstDatagrams = MediaCheckedArithmetic::add(
            ceilDivide(burstPayload.value(),
                       packetsPerDatagram * TsPacketBytes),
            rtp ? 2U : 0U,
            "aggregate MPEG-TS and RTCP burst datagrams");
        auto mediaAtomicDatagrams = MediaCheckedArithmetic::ceilScale(
            atomicPayload.value(), 1,
            packetsPerDatagram * TsPacketBytes,
            "aggregate MPEG-TS atomic datagrams");
        if (!totalPacketRate || !totalBurstDatagrams ||
            !mediaAtomicDatagrams) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !totalPacketRate ? totalPacketRate.error() :
                !totalBurstDatagrams ? totalBurstDatagrams.error() :
                mediaAtomicDatagrams.error());
        }
        packetRate = totalPacketRate.value();
        burstDatagrams = totalBurstDatagrams.value();
        atomicDatagrams = mediaAtomicDatagrams.value();
        if (rtp) {
            auto rtpRate = MediaCheckedArithmetic::multiply(
                packetRate, std::uint64_t{12}, "MP2T RTP headers");
            auto rtpBurst = MediaCheckedArithmetic::multiply(
                burstDatagrams, std::uint64_t{12}, "MP2T RTP burst headers");
            auto rtpAtomic = MediaCheckedArithmetic::multiply(
                atomicDatagrams, std::uint64_t{12},
                "MP2T RTP atomic headers");
            sustainedPayload = rtpRate
                ? MediaCheckedArithmetic::add(
                      sustainedPayload.value(), rtpRate.value(),
                      "MP2T sustained payload")
                : rtpRate;
            peakPayload = rtpRate
                ? MediaCheckedArithmetic::add(
                      peakPayload.value(), rtpRate.value(),
                      "MP2T peak payload")
                : rtpRate;
            burstPayload = rtpBurst
                ? MediaCheckedArithmetic::add(
                      burstPayload.value(), rtpBurst.value(),
                      "MP2T burst payload")
                : rtpBurst;
            atomicPayload = rtpAtomic
                ? MediaCheckedArithmetic::add(
                      atomicPayload.value(), rtpAtomic.value(),
                      "MP2T atomic payload")
                : rtpAtomic;
            if (!sustainedPayload || !peakPayload || !burstPayload ||
                !atomicPayload) {
                return ::media::Result<MediaWireTrafficEnvelope>::failure(
                    !sustainedPayload ? sustainedPayload.error() :
                    !peakPayload ? peakPayload.error() :
                    !burstPayload ? burstPayload.error() :
                    atomicPayload.error());
            }
        }
    }
    if (rtcpStreams > 0) {
        const auto cname = MediaRtpOutputIdentityPlanner::cname(
            request.mediaId);
        auto compound = MediaRtcpWireGeometry::compoundPayloadBytes(
            cname.size());
        auto rtcpRatePayload = compound
            ? MediaCheckedArithmetic::multiply(
                  compound.value(), rtcpStreams,
                  "RFC 3550 RTCP admission payload rate")
            : compound;
        auto compoundAndBye = compound
            ? MediaCheckedArithmetic::add(
                  compound.value(), MediaRtcpWireGeometry::byePayloadBytes(),
                  "RFC 3550 RTCP admission burst per stream")
            : compound;
        auto rtcpBurstPayload = compoundAndBye
            ? MediaCheckedArithmetic::multiply(
                  compoundAndBye.value(), rtcpStreams,
                  "RFC 3550 RTCP admission burst")
            : compoundAndBye;
        auto rtcpAtomicPayload = compound
            ? MediaCheckedArithmetic::multiply(
                  compound.value(), rtcpStreams,
                  "RFC 3550 RTCP admission atomic payload")
            : compound;
        sustainedPayload = rtcpRatePayload
            ? MediaCheckedArithmetic::add(
                  sustainedPayload.value(), rtcpRatePayload.value(),
                  "aggregate media and RTCP sustained payload")
            : rtcpRatePayload;
        peakPayload = rtcpRatePayload
            ? MediaCheckedArithmetic::add(
                  peakPayload.value(), rtcpRatePayload.value(),
                  "aggregate media and RTCP peak payload")
            : rtcpRatePayload;
        burstPayload = rtcpBurstPayload
            ? MediaCheckedArithmetic::add(
                  burstPayload.value(), rtcpBurstPayload.value(),
                  "aggregate media and RTCP burst payload")
            : rtcpBurstPayload;
        atomicPayload = rtcpAtomicPayload
            ? MediaCheckedArithmetic::add(
                  atomicPayload.value(), rtcpAtomicPayload.value(),
                  "aggregate media and RTCP atomic payload")
            : rtcpAtomicPayload;
        auto withRtcpAtomicDatagrams = MediaCheckedArithmetic::add(
            atomicDatagrams, rtcpStreams,
            "aggregate media and RTCP atomic datagrams");
        if (!sustainedPayload || !peakPayload || !burstPayload ||
            !atomicPayload || !withRtcpAtomicDatagrams) {
            return ::media::Result<MediaWireTrafficEnvelope>::failure(
                !sustainedPayload ? sustainedPayload.error() :
                !peakPayload ? peakPayload.error() :
                !burstPayload ? burstPayload.error() :
                !atomicPayload ? atomicPayload.error() :
                withRtcpAtomicDatagrams.error());
        }
        atomicDatagrams = withRtcpAtomicDatagrams.value();
    }
    const auto networkHeader = ipHeader + UdpHeaderBytes;
    auto networkRate = MediaCheckedArithmetic::multiply(
        packetRate, networkHeader, "conservative network header rate");
    auto networkBurst = MediaCheckedArithmetic::multiply(
        burstDatagrams, networkHeader, "conservative network burst headers");
    auto networkAtomic = MediaCheckedArithmetic::multiply(
        atomicDatagrams, networkHeader,
        "conservative network atomic headers");
    auto sustainedWire = networkRate
        ? MediaCheckedArithmetic::add(
              sustainedPayload.value(), networkRate.value(),
              "conservative sustained wire bytes")
        : networkRate;
    auto peakWire = networkRate
        ? MediaCheckedArithmetic::add(
              peakPayload.value(), networkRate.value(),
              "conservative peak wire bytes")
        : networkRate;
    auto burstWire = networkBurst
        ? MediaCheckedArithmetic::add(
              burstPayload.value(), networkBurst.value(),
              "conservative burst wire bytes")
        : networkBurst;
    auto atomicWire = networkAtomic
        ? MediaCheckedArithmetic::add(
              atomicPayload.value(), networkAtomic.value(),
              "conservative atomic wire bytes")
        : networkAtomic;
    if (!sustainedWire || !peakWire || !burstWire || !atomicWire ||
        packetRate == 0 || burstDatagrams == 0 || atomicDatagrams == 0) {
        return ::media::Result<MediaWireTrafficEnvelope>::failure(
            !sustainedWire ? sustainedWire.error() :
            !peakWire ? peakWire.error() :
            !burstWire ? burstWire.error() : atomicWire.error());
    }
    return ::media::Result<MediaWireTrafficEnvelope>::success({
        sustainedWire.value(), peakWire.value(), packetRate,
        burstWire.value(), burstDatagrams,
        atomicWire.value(), atomicDatagrams,
        maximumProtocolPayload,
        maximumProtocolPayload + networkHeader,
        "prepared-emission+protocol-upper-envelope+route-mtu"});
}

std::uint64_t endpointCount(const MediaRealtimeRtpTranscodeRequest& request)
{
    if (request.output.transport == MediaOutputTransportKind::UdpDatagrams) {
        return 1;
    }
    return request.parameters.execution.streamSet ==
            MediaTranscodeStreamSet::AudioVideo &&
            request.output.streamLayout ==
                RealtimeOutputStreamLayout::SeparateStreams
        ? 4 : 2;
}

::media::Result<MediaRealtimeDeploymentLatencyBudget> pacingLatency(
    const MediaWireTrafficEnvelope& wire,
    std::uint64_t provisionedWireCapacityBytesPerSecond,
    MediaRunningTime maximumResidence)
{
    if (wire.sustainedWireBytesPerSecond == 0 ||
        wire.peakWireBytesPerSecond == 0 || wire.burstWireBytes == 0 ||
        wire.maximumWireDatagramBytes == 0 ||
        provisionedWireCapacityBytesPerSecond <
            wire.peakWireBytesPerSecond) {
        return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
            ::media::ErrorInfo::unsupported(
                "prepared wire demand exceeds the configured managed service rate"));
    }
    auto requiredPacingRate =
        MediaDatagramPacingRatePlanner::requiredWireBytesPerSecond(
            wire, maximumResidence);
    if (!requiredPacingRate ||
        requiredPacingRate.value() > provisionedWireCapacityBytesPerSecond) {
        return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
            !requiredPacingRate ? requiredPacingRate.error() :
            ::media::ErrorInfo::unsupported(
                "prepared burst cannot be drained within the immutable residence by the provisioned service"));
    }
    auto burstDrain = MediaCheckedArithmetic::ceilScale(
        wire.burstWireBytes, NanosecondsPerSecond,
        requiredPacingRate.value(),
        "prepared deadline-admitted burst debt drain time");
    auto packetSerialization = MediaCheckedArithmetic::ceilScale(
        wire.maximumWireDatagramBytes, NanosecondsPerSecond,
        requiredPacingRate.value(),
        "prepared deadline-admitted maximum Datagram serialization time");
    auto target = burstDrain;
    if (!target || !packetSerialization ||
        target.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)()) ||
        packetSerialization.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
            !target ? target.error() :
            !packetSerialization ? packetSerialization.error() :
            ::media::ErrorInfo::invalidArgument(
                "prepared wire debt residence exceeds running-time range"));
    }
    const auto targetResidence = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(target.value()));
    if (maximumResidence <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
            ::media::ErrorInfo::invalidArgument(
                "maximum wire residence must be a positive deployment fact"));
    }
    if (targetResidence > maximumResidence) {
        return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
            ::media::ErrorInfo::unsupported(
                "prepared wire burst cannot meet the WebRTC maximum expected pacing queue residence"));
    }
    return ::media::Result<MediaRealtimeDeploymentLatencyBudget>::success({
        targetResidence,
        maximumResidence,
        "webrtc-no-feedback-default-2.5x+prepared-peak-and-burst-over-immutable-residence+webrtc-queue-time-admission+itu-y1221-gbra+rfc1363-maximum-rate-leaky-bucket",
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(packetSerialization.value())),
        "prepared-deadline-admitted-rate-maximum-datagram-serialization"});
}

} // namespace

::media::Result<MediaRealtimeDeploymentBasePlan>
MediaRealtimeDeploymentPlanner::planBase(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaPreparedRealtimeEmissionSet& emission)
{
    if (!request.deployment.provisionedEgressCapacityBitsPerSecond ||
        !request.deployment.maximumWireResidence ||
        *request.deployment.provisionedEgressCapacityBitsPerSecond < 8 ||
        *request.deployment.maximumWireResidence <=
            MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaRealtimeDeploymentBasePlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "realtime Datagram deployment requires provisioned egress capacity and maximum wire residence facts"));
    }
    auto route = MediaDatagramRouteProbe::probe(request);
    if (!route) {
        return ::media::Result<MediaRealtimeDeploymentBasePlan>::failure(
            route.error());
    }
    auto mtu = MediaRealtimeDatagramPayloadPlanner::plan(route.value());
    if (!mtu) {
        return ::media::Result<MediaRealtimeDeploymentBasePlan>::failure(
            mtu.error());
    }
    const auto provisionedWireCapacityBytesPerSecond =
        *request.deployment.provisionedEgressCapacityBitsPerSecond / 8;
    auto wire = conservativeWire(request, emission, mtu.value());
    auto latency = wire
        ? pacingLatency(
              wire.value(), provisionedWireCapacityBytesPerSecond,
              *request.deployment.maximumWireResidence)
        : ::media::Result<MediaRealtimeDeploymentLatencyBudget>::failure(
              wire.error());
    if (!wire || !latency) {
        return ::media::Result<MediaRealtimeDeploymentBasePlan>::failure(
            !wire ? wire.error() : latency.error());
    }
    auto transportTiming = MediaRealtimeTransportTimingPlanner::plan(
        latency.value());
    if (!transportTiming) {
        return ::media::Result<MediaRealtimeDeploymentBasePlan>::failure(
            transportTiming.error());
    }
    const auto count = endpointCount(request);
    const auto maximumResidence = latency.value().maximumResidence;
    MediaRealtimeDeploymentBasePlan result{
        {MediaDatagramServiceScopeKind::ProvisionedEgress,
         route.value().interfaceIndex,
         route.value().serviceScopeId,
         route.value().authority +
             "+caller-provisioned-egress+engine-owned-common-shaper"},
        std::move(mtu).value(),
        {route.value().addressFamily, route.value().localNumericAddress, 0,
         static_cast<std::uint16_t>(count),
         route.value().authority + "+kernel-ephemeral-bind"},
        std::move(latency).value(),
        {1, maximumResidence,
         MediaRealtimeTransmitEvidencePolicy::Report,
         "planner-owned-asynchronous-transmit-evidence"},
        std::move(transportTiming).value(),
        std::nullopt, std::move(wire).value(),
        provisionedWireCapacityBytesPerSecond, count};
    if (request.output.transport == MediaOutputTransportKind::RtpAvp) {
        result.rtcpSession = MediaRealtimeRtcpSessionCapability{
            2, "unicast-sender-and-single-destination"};
    }
    return ::media::Result<MediaRealtimeDeploymentBasePlan>::success(
        std::move(result));
}

::media::Result<MediaRealtimeDeploymentEnvelope>
MediaRealtimeDeploymentPlanner::complete(
    MediaRealtimeDeploymentBasePlan base)
{
    const auto& wire = base.admittedWire;
    auto pacingRate =
        MediaDatagramPacingRatePlanner::requiredWireBytesPerSecond(
            wire, base.latency.maximumResidence);
    if (!pacingRate || pacingRate.value() >
            base.provisionedWireCapacityBytesPerSecond) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            !pacingRate ? pacingRate.error() :
            ::media::ErrorInfo::unsupported(
                "prepared deadline-admitted pacing rate exceeds provisioned egress capacity"));
    }
    MediaRealtimeDeploymentManagedServiceFact service{
        base.provisionedWireCapacityBytesPerSecond,
        wire.authority +
            "+caller-provisioned-egress+prepared-wire-capacity-admission+managed-no-runtime-capacity-fallback"};
    auto residenceDatagrams = MediaCheckedArithmetic::bytesForResidence(
        wire.peakDatagramsPerSecond,
        base.latency.maximumResidence.nanoseconds(),
        "derived observation residence datagrams");
    if (!residenceDatagrams) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            residenceDatagrams.error());
    }
    auto runDatagrams = MediaCheckedArithmetic::add(
        residenceDatagrams.value(), wire.burstDatagrams,
        "derived observation run datagrams");
    if (!runDatagrams) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            runDatagrams.error());
    }
    base.observation.maximumRunDatagrams = (std::max)(
        (std::max)(std::uint64_t{1}, runDatagrams.value()),
        wire.maximumAtomicDatagrams);
    auto network = MediaRealtimeNetworkResourceLedgerPlanner::plan(
        base.latency, base.observation, wire,
        base.localPorts.addressFamily, base.endpointCount);
    if (!network) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            network.error());
    }
    auto totalNetwork = MediaCheckedArithmetic::add(
        network.value().admittedNetworkBytes,
        network.value().admittedSocketBytes,
        "derived aggregate network memory");
    if (!totalNetwork) {
        return ::media::Result<MediaRealtimeDeploymentEnvelope>::failure(
            totalNetwork.error());
    }
    MediaRealtimeDeploymentEnvelopeEncoding encoding{
        std::move(base.serviceScope), std::move(base.mtu), std::move(service),
        {totalNetwork.value(), "prepared-wire-resource-ledger"},
        std::move(base.localPorts), std::move(base.latency),
        std::move(base.observation), std::move(base.transportTiming),
        std::move(base.rtcpSession)};
    return MediaRealtimeDeploymentEnvelope::decode(std::move(encoding));
}

} // namespace media::ffmpeg::graph
