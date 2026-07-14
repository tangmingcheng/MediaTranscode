#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::uint64_t TimestampModulus = std::uint64_t{1} << 33;
constexpr std::uint64_t PcrModulus = TimestampModulus * 300;

::media::Result<std::int64_t> checkedScaleNanoseconds(
    std::int64_t nanoseconds,
    std::int64_t ticksPerSecond)
{
    const std::int64_t seconds = nanoseconds / NanosecondsPerSecond;
    const std::int64_t remainder = nanoseconds % NanosecondsPerSecond;
    if (seconds > std::numeric_limits<std::int64_t>::max() / ticksPerSecond ||
        seconds < std::numeric_limits<std::int64_t>::min() / ticksPerSecond) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS timestamp scale overflow"));
    }
    const std::int64_t whole = seconds * ticksPerSecond;
    const std::int64_t scaledRemainder = remainder * ticksPerSecond;
    const std::int64_t rounding = scaledRemainder >= 0
        ? NanosecondsPerSecond / 2 : -(NanosecondsPerSecond / 2);
    const std::int64_t fraction = (scaledRemainder + rounding) / NanosecondsPerSecond;
    if ((fraction > 0 && whole > std::numeric_limits<std::int64_t>::max() - fraction) ||
        (fraction < 0 && whole < std::numeric_limits<std::int64_t>::min() - fraction)) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS timestamp addition overflow"));
    }
    return ::media::Result<std::int64_t>::success(whole + fraction);
}

std::uint64_t positiveModulo(std::int64_t value, std::uint64_t modulus) noexcept
{
    if (value >= 0) return static_cast<std::uint64_t>(value) % modulus;
    const std::uint64_t magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1;
    const std::uint64_t remainder = magnitude % modulus;
    return remainder == 0 ? 0 : modulus - remainder;
}

std::uint64_t magnitude(std::int64_t value) noexcept
{
    return value >= 0
        ? static_cast<std::uint64_t>(value)
        : static_cast<std::uint64_t>(-(value + 1)) + 1;
}

std::uint64_t absoluteDistance(std::int64_t lhs, std::int64_t rhs) noexcept
{
    const std::uint64_t lhsMagnitude = magnitude(lhs);
    const std::uint64_t rhsMagnitude = magnitude(rhs);
    if ((lhs < 0) == (rhs < 0)) {
        return lhsMagnitude >= rhsMagnitude
            ? lhsMagnitude - rhsMagnitude
            : rhsMagnitude - lhsMagnitude;
    }
    return lhsMagnitude + rhsMagnitude;
}

::media::Status invalid(const char* reason)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        std::string("MPEG-TS output clock ") + reason));
}

} // namespace

::media::Result<MediaTsOutputClockGenerator> MediaTsOutputClockGenerator::create(
    MediaTsOutputClockPolicy policy,
    MediaPlaybackEpoch epoch)
{
    if (policy.pcrInterval.nanoseconds() <= 0 ||
        policy.maximumPcrGap.nanoseconds() <= policy.pcrInterval.nanoseconds() ||
        policy.maximumPcrJitter.nanoseconds() <= 0 ||
        policy.maximumPcrJitter.nanoseconds() >= policy.pcrInterval.nanoseconds() ||
        policy.timestampTimeBaseNumerator != 1 ||
        policy.timestampTimeBaseDenominator != 90'000 || epoch.generation == 0) {
        return ::media::Result<MediaTsOutputClockGenerator>::failure(
            invalid("policy is incomplete or unsupported").error());
    }
    return ::media::Result<MediaTsOutputClockGenerator>::success(
        MediaTsOutputClockGenerator(std::move(policy), epoch));
}

MediaTsOutputClockGenerator::MediaTsOutputClockGenerator(
    MediaTsOutputClockPolicy policy,
    MediaPlaybackEpoch epoch) noexcept
    : m_policy(std::move(policy)), m_epoch(epoch)
{
}

::media::Status MediaTsOutputClockGenerator::validateGeneration(
    std::uint64_t generation) const
{
    return generation == m_epoch.generation
        ? ::media::Status::success()
        : invalid("generation does not match playback epoch");
}

::media::Result<std::int64_t> MediaTsOutputClockGenerator::outputNanoseconds(
    MediaRunningTime masterTime) const
{
    auto delta = masterTime.checkedSubtract(m_epoch.masterRelease);
    if (!delta) return ::media::Result<std::int64_t>::failure(delta.error());
    auto output = m_epoch.sourceStart.checkedAdd(delta.value());
    if (!output) return ::media::Result<std::int64_t>::failure(output.error());
    return ::media::Result<std::int64_t>::success(output.value().nanoseconds());
}

::media::Result<std::int64_t> MediaTsOutputClockGenerator::timestampTicks(
    MediaRunningTime masterTime) const
{
    auto nanoseconds = outputNanoseconds(masterTime);
    return nanoseconds ? checkedScaleNanoseconds(nanoseconds.value(), 90'000)
                       : ::media::Result<std::int64_t>::failure(nanoseconds.error());
}

::media::Result<std::int64_t> MediaTsOutputClockGenerator::pcrTicks(
    MediaRunningTime masterTime) const
{
    auto nanoseconds = outputNanoseconds(masterTime);
    return nanoseconds ? checkedScaleNanoseconds(nanoseconds.value(), 27'000'000)
                       : ::media::Result<std::int64_t>::failure(nanoseconds.error());
}

::media::Result<MediaTsPacketClock> MediaTsOutputClockGenerator::project(
    std::uint64_t generation,
    MediaScheduledStream stream,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaRunningTime transportDecodeLead)
{
    if (auto status = validateGeneration(generation); !status) {
        return ::media::Result<MediaTsPacketClock>::failure(status.error());
    }
    auto actualLead = dispatchOnMaster.checkedSubtract(emitOnMaster);
    if (!actualLead || transportDecodeLead.nanoseconds() <= 0 ||
        actualLead.value() != transportDecodeLead) {
        return ::media::Result<MediaTsPacketClock>::failure(
            invalid("dispatch-to-emission lead differs from transport plan").error());
    }
    auto pts = timestampTicks(presentationOnMaster);
    auto dts = timestampTicks(dispatchOnMaster);
    if (!pts) return ::media::Result<MediaTsPacketClock>::failure(pts.error());
    if (!dts) return ::media::Result<MediaTsPacketClock>::failure(dts.error());
    if (dts.value() > pts.value()) {
        return ::media::Result<MediaTsPacketClock>::failure(
            invalid("decode timestamp exceeds presentation timestamp").error());
    }
    std::optional<std::int64_t>* lastExtendedDts = nullptr;
    switch (stream) {
    case MediaScheduledStream::Video:
        lastExtendedDts = &m_lastVideoExtendedDts;
        break;
    case MediaScheduledStream::Audio:
        lastExtendedDts = &m_lastAudioExtendedDts;
        break;
    default:
        return ::media::Result<MediaTsPacketClock>::failure(
            invalid("stream kind is unsupported").error());
    }
    if (*lastExtendedDts && dts.value() < **lastExtendedDts) {
        return ::media::Result<MediaTsPacketClock>::failure(
            invalid("decode timeline regressed").error());
    }
    *lastExtendedDts = dts.value();
    return ::media::Result<MediaTsPacketClock>::success(MediaTsPacketClock{
        pts.value(), dts.value(), positiveModulo(pts.value(), TimestampModulus),
        positiveModulo(dts.value(), TimestampModulus)});
}

::media::Result<std::vector<MediaTsPcrClock>>
MediaTsOutputClockGenerator::advancePcrThrough(
    std::uint64_t generation,
    MediaRunningTime masterTime)
{
    if (auto status = validateGeneration(generation); !status) {
        return ::media::Result<std::vector<MediaTsPcrClock>>::failure(status.error());
    }
    if (masterTime < m_epoch.masterRelease ||
        (m_lastPcrMasterTime && masterTime < *m_lastPcrMasterTime)) {
        return ::media::Result<std::vector<MediaTsPcrClock>>::failure(
            invalid("PCR schedule regressed").error());
    }
    const MediaRunningTime previous = m_lastPcrMasterTime
        ? *m_lastPcrMasterTime : m_epoch.masterRelease;
    auto gap = masterTime.checkedSubtract(previous);
    if (!gap || gap.value() > m_policy.maximumPcrGap) {
        return ::media::Result<std::vector<MediaTsPcrClock>>::failure(
            invalid("PCR request exceeded maximum gap").error());
    }

    MediaRunningTime next = m_epoch.masterRelease;
    if (m_lastPcrMasterTime) {
        auto nextResult = m_lastPcrMasterTime->checkedAdd(m_policy.pcrInterval);
        if (!nextResult) {
            return ::media::Result<std::vector<MediaTsPcrClock>>::failure(
                nextResult.error());
        }
        next = nextResult.value();
    }
    std::vector<MediaTsPcrClock> result;
    std::optional<MediaRunningTime> lastGenerated;
    while (next <= masterTime) {
        auto extended = pcrTicks(next);
        if (!extended) {
            return ::media::Result<std::vector<MediaTsPcrClock>>::failure(
                extended.error());
        }
        result.push_back(MediaTsPcrClock{
            generation, next, extended.value(),
            positiveModulo(extended.value(), PcrModulus)});
        lastGenerated = next;
        auto following = next.checkedAdd(m_policy.pcrInterval);
        if (!following) break;
        next = following.value();
    }
    if (lastGenerated) m_lastPcrMasterTime = *lastGenerated;
    return ::media::Result<std::vector<MediaTsPcrClock>>::success(std::move(result));
}

::media::Status MediaTsOutputClockGenerator::validateSerializedPcr(
    const MediaTsPcrClock& planned,
    std::int64_t serializedExtended27Mhz) const
{
    if (auto status = validateGeneration(planned.generation); !status) {
        return status;
    }
    auto elapsed = planned.masterTime.checkedSubtract(m_epoch.masterRelease);
    if (!elapsed || elapsed.value().nanoseconds() < 0 ||
        elapsed.value().nanoseconds() % m_policy.pcrInterval.nanoseconds() != 0) {
        return invalid("serialized PCR sample is outside the planned cadence");
    }
    auto expected = pcrTicks(planned.masterTime);
    if (!expected || planned.extended27Mhz != expected.value() ||
        planned.wire27Mhz != positiveModulo(expected.value(), PcrModulus)) {
        return invalid("serialized PCR sample identity is invalid");
    }
    const std::uint64_t delta = absoluteDistance(
        serializedExtended27Mhz, planned.extended27Mhz);
    auto tolerance = checkedScaleNanoseconds(
        m_policy.maximumPcrJitter.nanoseconds(), 27'000'000);
    if (!tolerance) return ::media::Status::failure(tolerance.error());
    return delta <= static_cast<std::uint64_t>(tolerance.value())
        ? ::media::Status::success()
        : invalid("serialized PCR exceeds planned jitter");
}

} // namespace media::ffmpeg::graph
