#include "internal/graph/protocol/mpegts/MediaTsOutputClockGenerator.h"

#include <limits>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaTsOutputClockControlState final {
    enum class PendingKind : std::uint8_t { None, Packet, Pcr };

    std::optional<std::int64_t> lastVideoExtendedDts;
    std::optional<std::int64_t> lastAudioExtendedDts;
    std::optional<MediaRunningTime> lastPcrMasterTime;
    std::uint64_t nextRevision = 1;
    std::uint64_t pendingRevision = 0;
    PendingKind pendingKind = PendingKind::None;
};

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

MediaTsPreparedPacketClock::MediaTsPreparedPacketClock(
    std::weak_ptr<MediaTsOutputClockControlState> owner,
    std::uint64_t revision,
    MediaScheduledStream stream,
    MediaTsPacketClock clock) noexcept
    : m_owner(std::move(owner))
    , m_revision(revision)
    , m_stream(stream)
    , m_clock(clock)
    , m_valid(true)
{
}

MediaTsPreparedPacketClock::~MediaTsPreparedPacketClock()
{
    cancel();
}

MediaTsPreparedPacketClock::MediaTsPreparedPacketClock(
    MediaTsPreparedPacketClock&& other) noexcept
    : m_owner(std::move(other.m_owner))
    , m_revision(other.m_revision)
    , m_stream(other.m_stream)
    , m_clock(other.m_clock)
    , m_valid(other.m_valid)
{
    other.m_valid = false;
}

MediaTsPreparedPacketClock& MediaTsPreparedPacketClock::operator=(
    MediaTsPreparedPacketClock&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_owner = std::move(other.m_owner);
    m_revision = other.m_revision;
    m_stream = other.m_stream;
    m_clock = other.m_clock;
    m_valid = other.m_valid;
    other.m_valid = false;
    return *this;
}

void MediaTsPreparedPacketClock::cancel() noexcept
{
    if (!m_valid) return;
    if (auto owner = m_owner.lock(); owner &&
        owner->pendingKind == MediaTsOutputClockControlState::PendingKind::Packet &&
        owner->pendingRevision == m_revision) {
        owner->pendingKind = MediaTsOutputClockControlState::PendingKind::None;
        owner->pendingRevision = 0;
    }
    m_valid = false;
}

MediaTsPreparedPcrClock::MediaTsPreparedPcrClock(
    std::weak_ptr<MediaTsOutputClockControlState> owner,
    std::uint64_t revision,
    MediaTsPcrClock clock) noexcept
    : m_owner(std::move(owner))
    , m_revision(revision)
    , m_clock(clock)
    , m_valid(true)
{
}

MediaTsPreparedPcrClock::~MediaTsPreparedPcrClock()
{
    cancel();
}

MediaTsPreparedPcrClock::MediaTsPreparedPcrClock(
    MediaTsPreparedPcrClock&& other) noexcept
    : m_owner(std::move(other.m_owner))
    , m_revision(other.m_revision)
    , m_clock(other.m_clock)
    , m_valid(other.m_valid)
{
    other.m_valid = false;
}

MediaTsPreparedPcrClock& MediaTsPreparedPcrClock::operator=(
    MediaTsPreparedPcrClock&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_owner = std::move(other.m_owner);
    m_revision = other.m_revision;
    m_clock = other.m_clock;
    m_valid = other.m_valid;
    other.m_valid = false;
    return *this;
}

void MediaTsPreparedPcrClock::cancel() noexcept
{
    if (!m_valid) return;
    if (auto owner = m_owner.lock(); owner &&
        owner->pendingKind == MediaTsOutputClockControlState::PendingKind::Pcr &&
        owner->pendingRevision == m_revision) {
        owner->pendingKind = MediaTsOutputClockControlState::PendingKind::None;
        owner->pendingRevision = 0;
    }
    m_valid = false;
}

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
    MediaPlaybackEpoch epoch)
    : m_policy(std::move(policy))
    , m_epoch(epoch)
    , m_control(std::make_shared<MediaTsOutputClockControlState>())
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

::media::Result<MediaTsPreparedPacketClock> MediaTsOutputClockGenerator::preparePacket(
    std::uint64_t generation,
    MediaScheduledStream stream,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaRunningTime transportDecodeLead)
{
    if (auto status = validateGeneration(generation); !status) {
        return ::media::Result<MediaTsPreparedPacketClock>::failure(status.error());
    }
    auto actualLead = dispatchOnMaster.checkedSubtract(emitOnMaster);
    if (!actualLead || transportDecodeLead.nanoseconds() <= 0 ||
        actualLead.value() != transportDecodeLead) {
        return ::media::Result<MediaTsPreparedPacketClock>::failure(
            invalid("dispatch-to-emission lead differs from transport plan").error());
    }
    auto pts = timestampTicks(presentationOnMaster);
    auto dts = timestampTicks(dispatchOnMaster);
    if (!pts) return ::media::Result<MediaTsPreparedPacketClock>::failure(pts.error());
    if (!dts) return ::media::Result<MediaTsPreparedPacketClock>::failure(dts.error());
    if (dts.value() > pts.value()) {
        return ::media::Result<MediaTsPreparedPacketClock>::failure(
            invalid("decode timestamp exceeds presentation timestamp").error());
    }
    std::optional<std::int64_t>* lastExtendedDts = nullptr;
    switch (stream) {
    case MediaScheduledStream::Video:
        lastExtendedDts = &m_control->lastVideoExtendedDts;
        break;
    case MediaScheduledStream::Audio:
        lastExtendedDts = &m_control->lastAudioExtendedDts;
        break;
    default:
        return ::media::Result<MediaTsPreparedPacketClock>::failure(
            invalid("stream kind is unsupported").error());
    }
    if (*lastExtendedDts && dts.value() < **lastExtendedDts) {
        return ::media::Result<MediaTsPreparedPacketClock>::failure(
            invalid("decode timeline regressed").error());
    }
    if (m_control->pendingKind != MediaTsOutputClockControlState::PendingKind::None ||
        m_control->nextRevision == 0 ||
        m_control->nextRevision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPreparedPacketClock>::failure(
            invalid("already has a pending transaction or exhausted revisions").error());
    }
    const std::uint64_t revision = m_control->nextRevision++;
    m_control->pendingKind = MediaTsOutputClockControlState::PendingKind::Packet;
    m_control->pendingRevision = revision;
    return ::media::Result<MediaTsPreparedPacketClock>::success(
        MediaTsPreparedPacketClock(
            m_control, revision, stream,
            MediaTsPacketClock{
                pts.value(), dts.value(), positiveModulo(pts.value(), TimestampModulus),
                positiveModulo(dts.value(), TimestampModulus)}));
}

::media::Status MediaTsOutputClockGenerator::commitPacket(
    MediaTsPreparedPacketClock&& prepared)
{
    auto owner = prepared.m_owner.lock();
    if (!prepared.m_valid || owner != m_control ||
        owner->pendingKind != MediaTsOutputClockControlState::PendingKind::Packet ||
        owner->pendingRevision != prepared.m_revision) {
        return invalid("packet transaction token is stale or foreign");
    }
    switch (prepared.m_stream) {
    case MediaScheduledStream::Video:
        owner->lastVideoExtendedDts = prepared.m_clock.extendedDts;
        break;
    case MediaScheduledStream::Audio:
        owner->lastAudioExtendedDts = prepared.m_clock.extendedDts;
        break;
    default:
        return invalid("packet transaction stream is unsupported");
    }
    owner->pendingKind = MediaTsOutputClockControlState::PendingKind::None;
    owner->pendingRevision = 0;
    prepared.m_valid = false;
    return ::media::Status::success();
}

::media::Result<MediaTsPreparedPcrClock>
MediaTsOutputClockGenerator::preparePcr(
    std::uint64_t generation,
    MediaRunningTime exactDeadline)
{
    if (auto status = validateGeneration(generation); !status) {
        return ::media::Result<MediaTsPreparedPcrClock>::failure(status.error());
    }
    if (m_control->pendingKind != MediaTsOutputClockControlState::PendingKind::None ||
        m_control->nextRevision == 0 ||
        m_control->nextRevision == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Result<MediaTsPreparedPcrClock>::failure(
            invalid("already has a pending transaction or exhausted revisions").error());
    }
    MediaRunningTime expected = m_epoch.masterRelease;
    if (m_control->lastPcrMasterTime) {
        auto next = m_control->lastPcrMasterTime->checkedAdd(m_policy.pcrInterval);
        if (!next) {
            return ::media::Result<MediaTsPreparedPcrClock>::failure(next.error());
        }
        expected = next.value();
    }
    if (exactDeadline != expected) {
        return ::media::Result<MediaTsPreparedPcrClock>::failure(
            invalid("PCR deadline is not the next planned sample").error());
    }
    auto extended = pcrTicks(exactDeadline);
    if (!extended) {
        return ::media::Result<MediaTsPreparedPcrClock>::failure(extended.error());
    }
    const std::uint64_t revision = m_control->nextRevision++;
    m_control->pendingKind = MediaTsOutputClockControlState::PendingKind::Pcr;
    m_control->pendingRevision = revision;
    return ::media::Result<MediaTsPreparedPcrClock>::success(
        MediaTsPreparedPcrClock(
            m_control, revision,
            MediaTsPcrClock{
                generation, exactDeadline, extended.value(),
                positiveModulo(extended.value(), PcrModulus)}));
}

::media::Status MediaTsOutputClockGenerator::commitPcr(
    MediaTsPreparedPcrClock&& prepared)
{
    auto owner = prepared.m_owner.lock();
    if (!prepared.m_valid || owner != m_control ||
        owner->pendingKind != MediaTsOutputClockControlState::PendingKind::Pcr ||
        owner->pendingRevision != prepared.m_revision) {
        return invalid("PCR transaction token is stale or foreign");
    }
    owner->lastPcrMasterTime = prepared.m_clock.masterTime;
    owner->pendingKind = MediaTsOutputClockControlState::PendingKind::None;
    owner->pendingRevision = 0;
    prepared.m_valid = false;
    return ::media::Status::success();
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
