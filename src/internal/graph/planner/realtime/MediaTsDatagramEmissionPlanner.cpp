#include "internal/graph/planner/realtime/MediaTsDatagramEmissionPlanner.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t BitsPerByte = 8;
constexpr std::uint64_t MaximumTsPayloadBytes = 184;
constexpr std::uint64_t MaximumPesHeaderBytes = 19;
constexpr std::uint64_t AacAdtsHeaderBytes = 7;
constexpr std::uint64_t FirstPacketAdaptationBytes = 2;

::media::Result<std::uint64_t> checkedAdd(
    std::uint64_t left,
    std::uint64_t right,
    const char* message)
{
    if (right > (std::numeric_limits<std::uint64_t>::max)() - left) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(message));
    }
    return ::media::Result<std::uint64_t>::success(left + right);
}

::media::Result<std::uint64_t> checkedMultiply(
    std::uint64_t left,
    std::uint64_t right,
    const char* message)
{
    if (left != 0 &&
        right > (std::numeric_limits<std::uint64_t>::max)() / left) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(message));
    }
    return ::media::Result<std::uint64_t>::success(left * right);
}

std::uint64_t divideUp(std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value / divisor + (value % divisor != 0 ? 1u : 0u);
}

::media::Result<std::uint64_t> maximumBitrateKbps(
    const MediaEncoderRateControlPlan& rateControl)
{
    std::optional<int> selected;
    switch (rateControl.mode) {
    case MediaRateControlMode::Auto:
    case MediaRateControlMode::Cbr:
        selected = rateControl.targetBitrateKbps;
        break;
    case MediaRateControlMode::Vbr:
    case MediaRateControlMode::Cvbr:
        selected = rateControl.maximumBitrateKbps;
        break;
    case MediaRateControlMode::Crf:
        break;
    }
    if (!selected || *selected <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "scheduled MPEG-TS emission requires a bounded encoder bitrate"));
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(*selected));
}

::media::Result<std::uint64_t> unitsPerSecond(MediaRunningTime cadence)
{
    if (cadence.nanoseconds() <= 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled MPEG-TS emission requires a positive cadence"));
    }
    return ::media::Result<std::uint64_t>::success(divideUp(
        NanosecondsPerSecond,
        static_cast<std::uint64_t>(cadence.nanoseconds())));
}

::media::Result<std::uint64_t> streamLogicalBytesPerSecond(
    const MediaEncoderRateControlPlan& rateControl,
    MediaRunningTime cadence,
    std::uint64_t framingHeaderBytes)
{
    auto bitrate = maximumBitrateKbps(rateControl);
    auto units = unitsPerSecond(cadence);
    if (!bitrate || !units) {
        return ::media::Result<std::uint64_t>::failure(
            bitrate ? units.error() : bitrate.error());
    }
    auto bits = checkedMultiply(
        bitrate.value(), 1000,
        "scheduled MPEG-TS elementary bitrate is not representable");
    auto perUnit = checkedAdd(
        MaximumPesHeaderBytes, framingHeaderBytes,
        "scheduled MPEG-TS framing overhead is not representable");
    auto perUnitOverhead = perUnit
        ? checkedAdd(
              perUnit.value(), FirstPacketAdaptationBytes,
              "scheduled MPEG-TS unit overhead is not representable")
        : ::media::Result<std::uint64_t>::failure(perUnit.error());
    auto unitOverhead = perUnitOverhead
        ? checkedMultiply(
              units.value(), perUnitOverhead.value(),
              "scheduled MPEG-TS unit overhead is not representable")
        : ::media::Result<std::uint64_t>::failure(perUnitOverhead.error());
    if (!bits || !unitOverhead) {
        return ::media::Result<std::uint64_t>::failure(
            bits ? unitOverhead.error() : bits.error());
    }
    return checkedAdd(
        divideUp(bits.value(), BitsPerByte), unitOverhead.value(),
        "scheduled MPEG-TS logical byte rate is not representable");
}

} // namespace

::media::Result<std::int64_t>
MediaTsDatagramEmissionPlanner::plannedWireBytesPerSecond(
    std::size_t transportPacketBytes,
    std::size_t maximumPacketsPerDatagram,
    std::size_t perDatagramOverheadBytes,
    MediaRunningTime psiRepeatInterval,
    MediaRunningTime pcrInterval,
    const MediaTsDatagramEmissionPlanningFacts& facts)
{
    using Result = ::media::Result<std::int64_t>;
    if (transportPacketBytes == 0 || maximumPacketsPerDatagram == 0 ||
        facts.maximumQueuedBytes == 0 ||
        facts.audioAccessUnitCadence.has_value() !=
            facts.audioRateControl.has_value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled MPEG-TS emission planning facts are incomplete"));
    }
    auto logical = streamLogicalBytesPerSecond(
        facts.videoRateControl, facts.videoAccessUnitCadence, 0);
    auto videoUnits = unitsPerSecond(facts.videoAccessUnitCadence);
    if (!logical || !videoUnits) {
        return Result::failure(logical ? videoUnits.error() : logical.error());
    }
    std::uint64_t accessUnits = videoUnits.value();
    if (facts.audioRateControl) {
        auto audioLogical = streamLogicalBytesPerSecond(
            *facts.audioRateControl, *facts.audioAccessUnitCadence,
            AacAdtsHeaderBytes);
        auto audioUnits = unitsPerSecond(*facts.audioAccessUnitCadence);
        if (!audioLogical || !audioUnits) {
            return Result::failure(
                audioLogical ? audioUnits.error() : audioLogical.error());
        }
        auto combined = checkedAdd(
            logical.value(), audioLogical.value(),
            "scheduled MPEG-TS aggregate logical rate is not representable");
        auto combinedUnits = checkedAdd(
            accessUnits, audioUnits.value(),
            "scheduled MPEG-TS access-unit rate is not representable");
        if (!combined || !combinedUnits) {
            return Result::failure(
                combined ? combinedUnits.error() : combined.error());
        }
        logical = std::move(combined);
        accessUnits = combinedUnits.value();
    }
    auto psi = unitsPerSecond(psiRepeatInterval);
    auto pcr = unitsPerSecond(pcrInterval);
    if (!psi || !pcr) return Result::failure(psi ? pcr.error() : psi.error());
    auto psiPackets = checkedMultiply(
        psi.value(), 2,
        "scheduled MPEG-TS PSI packet rate is not representable");
    auto mediaPackets = checkedAdd(
        divideUp(logical.value(), MaximumTsPayloadBytes), accessUnits,
        "scheduled MPEG-TS media packet rate is not representable");
    auto maintenancePackets = psiPackets
        ? checkedAdd(
              psiPackets.value(), pcr.value(),
              "scheduled MPEG-TS maintenance packet rate is not representable")
        : ::media::Result<std::uint64_t>::failure(psiPackets.error());
    auto totalPackets = mediaPackets && maintenancePackets
        ? checkedAdd(
              mediaPackets.value(), maintenancePackets.value(),
              "scheduled MPEG-TS packet rate is not representable")
        : ::media::Result<std::uint64_t>::failure(
              mediaPackets ? maintenancePackets.error() : mediaPackets.error());
    if (!totalPackets) return Result::failure(totalPackets.error());
    auto payloadBytes = checkedMultiply(
        totalPackets.value(), transportPacketBytes,
        "scheduled MPEG-TS transport byte rate is not representable");
    auto maintenanceEvents = psiPackets
        ? checkedAdd(
              psiPackets.value(), pcr.value(),
              "scheduled MPEG-TS maintenance event rate is not representable")
        : ::media::Result<std::uint64_t>::failure(psiPackets.error());
    auto events = maintenanceEvents
        ? checkedAdd(
              accessUnits, maintenanceEvents.value(),
              "scheduled MPEG-TS event rate is not representable")
        : ::media::Result<std::uint64_t>::failure(
              maintenanceEvents.error());
    auto datagrams = events
        ? checkedAdd(
              divideUp(totalPackets.value(), maximumPacketsPerDatagram),
              events.value(),
              "scheduled MPEG-TS datagram rate is not representable")
        : ::media::Result<std::uint64_t>::failure(events.error());
    auto overhead = datagrams
        ? checkedMultiply(
              datagrams.value(), perDatagramOverheadBytes,
              "scheduled MPEG-TS datagram overhead is not representable")
        : ::media::Result<std::uint64_t>::failure(datagrams.error());
    auto wire = payloadBytes && overhead
        ? checkedAdd(
              payloadBytes.value(), overhead.value(),
              "scheduled MPEG-TS wire byte rate is not representable")
        : ::media::Result<std::uint64_t>::failure(
              payloadBytes ? overhead.error() : payloadBytes.error());
    if (!wire || wire.value() == 0 ||
        wire.value() > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return Result::failure(
            wire ? ::media::ErrorInfo::invalidArgument(
                       "scheduled MPEG-TS wire rate exceeds its type")
                 : wire.error());
    }
    return Result::success(static_cast<std::int64_t>(wire.value()));
}

::media::Result<MediaRunningTime>
MediaTsDatagramEmissionPlanner::maximumResidence(
    const MediaTsDatagramEmissionPlanningFacts& facts)
{
    auto bitrate = maximumBitrateKbps(facts.videoRateControl);
    if (!bitrate || !facts.videoRateControl.bufferSizeKbits ||
        *facts.videoRateControl.bufferSizeKbits <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            bitrate ? ::media::ErrorInfo::notInitialized(
                           "scheduled MPEG-TS emission requires an encoder buffer bound")
                     : bitrate.error());
    }
    auto scaled = checkedMultiply(
        static_cast<std::uint64_t>(
            *facts.videoRateControl.bufferSizeKbits),
        NanosecondsPerSecond,
        "scheduled MPEG-TS residence is not representable");
    if (!scaled) {
        return ::media::Result<MediaRunningTime>::failure(scaled.error());
    }
    const std::uint64_t nanoseconds = divideUp(
        scaled.value(), bitrate.value());
    if (nanoseconds == 0 || nanoseconds > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled MPEG-TS residence exceeds running time"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(nanoseconds)));
}

} // namespace media::ffmpeg::graph
