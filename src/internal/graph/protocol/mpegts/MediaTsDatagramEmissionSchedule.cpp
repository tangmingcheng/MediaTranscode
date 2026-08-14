#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

struct UInt128 final {
    std::uint64_t high;
    std::uint64_t low;
};

UInt128 multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
    const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhs);
    const std::uint64_t lhsHigh = lhs >> 32;
    const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhs);
    const std::uint64_t rhsHigh = rhs >> 32;
    const std::uint64_t lowProduct = lhsLow * rhsLow;
    const std::uint64_t firstCross =
        lhsHigh * rhsLow + (lowProduct >> 32);
    const std::uint64_t secondCross =
        lhsLow * rhsHigh + static_cast<std::uint32_t>(firstCross);
    return UInt128{
        lhsHigh * rhsHigh + (firstCross >> 32) + (secondCross >> 32),
        (secondCross << 32) + static_cast<std::uint32_t>(lowProduct)};
}

struct DivisionResult final {
    std::uint64_t quotient;
    std::uint64_t remainder;
    bool quotientOverflow;
};

DivisionResult divide(UInt128 dividend, std::uint64_t divisor) noexcept
{
    std::uint64_t quotient = 0;
    std::uint64_t remainder = 0;
    bool overflow = false;
    for (int bitIndex = 127; bitIndex >= 0; --bitIndex) {
        const std::uint64_t bit = bitIndex >= 64
            ? (dividend.high >> (bitIndex - 64)) & 1u
            : (dividend.low >> bitIndex) & 1u;
        remainder = (remainder << 1) | bit;
        if (remainder < divisor) continue;
        remainder -= divisor;
        if (bitIndex >= 64) {
            overflow = true;
        } else {
            quotient |= std::uint64_t{1} << bitIndex;
        }
    }
    return DivisionResult{quotient, remainder, overflow};
}

::media::Result<std::uint64_t> scaledQuotient(
    std::uint64_t value,
    std::uint64_t scale,
    std::uint64_t divisor,
    bool roundUp)
{
    if (divisor == 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission scaling requires a nonzero divisor"));
    }
    const auto divided = divide(multiply(value, scale), divisor);
    if (divided.quotientOverflow ||
        (roundUp && divided.remainder != 0 &&
         divided.quotient ==
             (std::numeric_limits<std::uint64_t>::max)())) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission scaling is not representable"));
    }
    return ::media::Result<std::uint64_t>::success(
        divided.quotient +
        (roundUp && divided.remainder != 0 ? 1u : 0u));
}

::media::Result<MediaRunningTime> rateDuration(
    std::uint64_t bytes,
    std::int64_t bytesPerSecond,
    bool roundUp)
{
    if (bytesPerSecond <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission duration requires a positive wire rate"));
    }
    auto nanoseconds = scaledQuotient(
        bytes, NanosecondsPerSecond,
        static_cast<std::uint64_t>(bytesPerSecond), roundUp);
    if (!nanoseconds || nanoseconds.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<MediaRunningTime>::failure(
            nanoseconds ? ::media::ErrorInfo::invalidArgument(
                              "MPEG-TS emission duration exceeds running time")
                        : nanoseconds.error());
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(nanoseconds.value())));
}

::media::Result<std::int64_t> rateForWindow(
    std::uint64_t bytes,
    MediaRunningTime window)
{
    if (bytes == 0 || window.nanoseconds() <= 0) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit rate requires positive bytes and time"));
    }
    auto rate = scaledQuotient(
        bytes, NanosecondsPerSecond,
        static_cast<std::uint64_t>(window.nanoseconds()), true);
    if (!rate || rate.value() == 0 || rate.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::int64_t>::max)())) {
        return ::media::Result<std::int64_t>::failure(
            rate ? ::media::ErrorInfo::invalidArgument(
                       "MPEG-TS access-unit wire rate is not representable")
                 : rate.error());
    }
    return ::media::Result<std::int64_t>::success(
        static_cast<std::int64_t>(rate.value()));
}

} // namespace

class MediaTsDatagramEmissionScheduleState final {
public:
    struct StreamObservation final {
        MediaRunningTime emitOnMaster;
        std::int64_t wireBytesPerSecond;
    };
    struct ActiveAccessUnit final {
        MediaScheduledStream stream;
        MediaRunningTime emitOnMaster;
        MediaRunningTime dispatchOnMaster;
        MediaRunningTime serviceStart;
        MediaRunningTime completionDeadline;
        std::uint64_t totalWireBytes;
        std::uint64_t committedWireBytes;
        std::int64_t selectedWireBytesPerSecond;
        StreamObservation proposedObservation;
        std::optional<MediaRunningTime> latestActualEmission;
    };

    MediaTsDatagramEmissionScheduleState(
        MediaTsDatagramEmissionPlan selectedPlan,
        MediaRunningTime selectedOrigin) noexcept
        : plan(std::move(selectedPlan)),
          origin(selectedOrigin),
          availableAt(selectedOrigin)
    {
    }

    MediaTsDatagramEmissionPlan plan;
    MediaRunningTime origin;
    MediaRunningTime availableAt;
    std::optional<StreamObservation> videoObservation;
    std::optional<StreamObservation> audioObservation;
    std::optional<ActiveAccessUnit> activeAccessUnit;
    std::optional<std::uint64_t> pendingRevision;
    std::uint64_t nextRevision = 1;
};

namespace {

::media::Result<std::uint64_t> wireBytesForPayload(
    const MediaTsDatagramEmissionPlan& plan,
    std::size_t payloadBytes)
{
    if (payloadBytes == 0 || plan.packetSizeBytes() == 0 ||
        plan.maximumPayloadBytes() == 0 ||
        payloadBytes % plan.packetSizeBytes() != 0) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission payload is not complete transport packets"));
    }
    const auto payload = static_cast<std::uint64_t>(payloadBytes);
    const auto maximumPayload = static_cast<std::uint64_t>(
        plan.maximumPayloadBytes());
    const std::uint64_t datagrams =
        payload / maximumPayload + (payload % maximumPayload != 0 ? 1u : 0u);
    if (plan.perDatagramOverheadBytes() != 0 && datagrams >
        ((std::numeric_limits<std::uint64_t>::max)() - payload) /
            plan.perDatagramOverheadBytes()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission wire bytes are not representable"));
    }
    return ::media::Result<std::uint64_t>::success(
        payload +
        datagrams * plan.perDatagramOverheadBytes());
}

std::optional<MediaTsDatagramEmissionScheduleState::StreamObservation>&
observationFor(
    MediaTsDatagramEmissionScheduleState& state,
    MediaScheduledStream stream)
{
    return stream == MediaScheduledStream::Video
        ? state.videoObservation
        : state.audioObservation;
}

const std::optional<MediaTsDatagramEmissionScheduleState::StreamObservation>&
otherObservation(
    const MediaTsDatagramEmissionScheduleState& state,
    MediaScheduledStream stream)
{
    return stream == MediaScheduledStream::Video
        ? state.audioObservation
        : state.videoObservation;
}

} // namespace

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state,
    std::uint64_t revision,
    MediaRunningTime deadline,
    MediaRunningTime latestEmissionTime,
    MediaRunningTime plannedWait,
    std::size_t wireBytes,
    std::uint64_t nextCommittedWireBytes) noexcept
    : m_state(std::move(state)),
      m_revision(revision),
      m_deadline(deadline),
      m_latestEmissionTime(latestEmissionTime),
      m_plannedWait(plannedWait),
      m_wireBytes(wireBytes),
      m_nextCommittedWireBytes(nextCommittedWireBytes),
      m_active(true)
{
}

MediaTsPreparedDatagramEmission::~MediaTsPreparedDatagramEmission()
{
    cancel();
}

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    MediaTsPreparedDatagramEmission&& other) noexcept
    : m_state(std::move(other.m_state)),
      m_revision(other.m_revision),
      m_deadline(other.m_deadline),
      m_latestEmissionTime(other.m_latestEmissionTime),
      m_plannedWait(other.m_plannedWait),
      m_wireBytes(other.m_wireBytes),
      m_nextCommittedWireBytes(other.m_nextCommittedWireBytes),
      m_active(other.m_active)
{
    other.m_active = false;
}

MediaTsPreparedDatagramEmission&
MediaTsPreparedDatagramEmission::operator=(
    MediaTsPreparedDatagramEmission&& other) noexcept
{
    if (this == &other) return *this;
    cancel();
    m_state = std::move(other.m_state);
    m_revision = other.m_revision;
    m_deadline = other.m_deadline;
    m_latestEmissionTime = other.m_latestEmissionTime;
    m_plannedWait = other.m_plannedWait;
    m_wireBytes = other.m_wireBytes;
    m_nextCommittedWireBytes = other.m_nextCommittedWireBytes;
    m_active = other.m_active;
    other.m_active = false;
    return *this;
}

MediaRunningTime MediaTsPreparedDatagramEmission::deadline() const noexcept
{
    return m_deadline;
}

MediaRunningTime MediaTsPreparedDatagramEmission::plannedWait() const noexcept
{
    return m_plannedWait;
}

MediaRunningTime
MediaTsPreparedDatagramEmission::latestEmissionTime() const noexcept
{
    return m_latestEmissionTime;
}

std::size_t MediaTsPreparedDatagramEmission::wireBytes() const noexcept
{
    return m_wireBytes;
}

void MediaTsPreparedDatagramEmission::cancel() noexcept
{
    if (m_active && m_state &&
        m_state->pendingRevision == m_revision) {
        m_state->pendingRevision.reset();
    }
    m_active = false;
    m_state.reset();
}

::media::Result<MediaTsDatagramEmissionSchedule>
MediaTsDatagramEmissionSchedule::create(
    MediaTsDatagramEmissionPlan plan,
    MediaRunningTime origin)
{
    return ::media::Result<MediaTsDatagramEmissionSchedule>::success(
        MediaTsDatagramEmissionSchedule(
            std::make_shared<MediaTsDatagramEmissionScheduleState>(
                std::move(plan), origin)));
}

MediaTsDatagramEmissionSchedule::MediaTsDatagramEmissionSchedule(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaTsAccessUnitEmissionDecision>
MediaTsDatagramEmissionSchedule::beginAccessUnit(
    MediaScheduledStream stream,
    std::size_t totalPayloadBytes,
    MediaRunningTime emitOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime actualMasterNow)
{
    using Result = ::media::Result<MediaTsAccessUnitEmissionDecision>;
    if (!m_state || m_state->activeAccessUnit ||
        m_state->pendingRevision ||
        (stream != MediaScheduledStream::Video &&
         stream != MediaScheduledStream::Audio)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit emission cannot begin in its current state"));
    }
    auto transportWindow = dispatchOnMaster.checkedSubtract(emitOnMaster);
    if (!transportWindow ||
        transportWindow.value() != m_state->plan.accessUnitWindow()) {
        return Result::failure(
            transportWindow
                ? ::media::ErrorInfo::invalidArgument(
                      "MPEG-TS access-unit window differs from its plan")
                : transportWindow.error());
    }
    auto totalWire = wireBytesForPayload(m_state->plan, totalPayloadBytes);
    if (!totalWire) return Result::failure(totalWire.error());
    const MediaRunningTime serviceStart = (std::max)(
        actualMasterNow,
        (std::max)(emitOnMaster, m_state->availableAt));
    auto completionWindow = dispatchOnMaster.checkedSubtract(serviceStart);
    if (!completionWindow || completionWindow.value().nanoseconds() <= 0) {
        return Result::failure(
            completionWindow
                ? ::media::ErrorInfo::invalidArgument(
                      "MPEG-TS access-unit canonical window is exhausted")
                : completionWindow.error());
    }

    auto& current = observationFor(*m_state, stream);
    MediaRunningTime cadence = stream == MediaScheduledStream::Video
        ? m_state->plan.videoInitialServiceWindow()
        : m_state->plan.audioInitialServiceWindow().value_or(
              MediaRunningTime::fromNanoseconds(0));
    if (cadence.nanoseconds() <= 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit stream has no planned initial cadence"));
    }
    if (current) {
        auto observed = emitOnMaster.checkedSubtract(current->emitOnMaster);
        if (!observed || observed.value().nanoseconds() <= 0) {
            return Result::failure(
                observed
                    ? ::media::ErrorInfo::invalidArgument(
                          "MPEG-TS access-unit stream cadence did not advance")
                    : observed.error());
        }
        cadence = observed.value();
    }
    auto currentRate = rateForWindow(totalWire.value(), cadence);
    auto requiredRate = rateForWindow(
        totalWire.value(), completionWindow.value());
    if (!currentRate || !requiredRate) {
        return Result::failure(
            currentRate ? requiredRate.error() : currentRate.error());
    }
    std::int64_t aggregateRate = currentRate.value();
    const auto& other = otherObservation(*m_state, stream);
    if (other) {
        if (aggregateRate >
            (std::numeric_limits<std::int64_t>::max)() -
                other->wireBytesPerSecond) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS aggregate access-unit rate is not representable"));
        }
        aggregateRate += other->wireBytesPerSecond;
    }
    const std::int64_t selectedRate =
        (std::max)(aggregateRate, requiredRate.value());
    auto serviceDuration = rateDuration(
        totalWire.value(), selectedRate, true);
    if (!serviceDuration) return Result::failure(serviceDuration.error());
    auto completion = serviceStart.checkedAdd(serviceDuration.value());
    auto debt = serviceStart.checkedSubtract(emitOnMaster);
    if (!completion || !debt || completion.value() > dispatchOnMaster) {
        return Result::failure(
            !completion ? completion.error() :
            !debt ? debt.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit cannot meet its canonical dispatch deadline"));
    }
    m_state->activeAccessUnit.emplace(
        MediaTsDatagramEmissionScheduleState::ActiveAccessUnit{
            stream, emitOnMaster, dispatchOnMaster, serviceStart,
            completion.value(), totalWire.value(), 0, selectedRate,
            {emitOnMaster, currentRate.value()}, std::nullopt});
    return Result::success(MediaTsAccessUnitEmissionDecision{
        selectedRate, debt.value(), completion.value()});
}

::media::Result<MediaTsPreparedDatagramEmission>
MediaTsDatagramEmissionSchedule::prepareAccessUnit(
    std::size_t payloadBytes)
{
    using Result = ::media::Result<MediaTsPreparedDatagramEmission>;
    if (!m_state || !m_state->activeAccessUnit ||
        m_state->pendingRevision) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram cannot be prepared"));
    }
    auto wire = wireBytesForPayload(m_state->plan, payloadBytes);
    if (!wire) return Result::failure(wire.error());
    auto& active = *m_state->activeAccessUnit;
    if (active.committedWireBytes > active.totalWireBytes ||
        wire.value() > active.totalWireBytes - active.committedWireBytes ||
        wire.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram exceeds its materialized wire bytes"));
    }
    auto offset = rateDuration(
        active.committedWireBytes,
        active.selectedWireBytesPerSecond, false);
    if (!offset) return Result::failure(offset.error());
    auto deadline = active.serviceStart.checkedAdd(offset.value());
    auto plannedWait = deadline
        ? deadline.value().checkedSubtract(active.emitOnMaster)
        : ::media::Result<MediaRunningTime>::failure(deadline.error());
    if (!deadline || !plannedWait ||
        deadline.value() > active.dispatchOnMaster ||
        m_state->nextRevision ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::failure(
            !deadline ? deadline.error() :
            !plannedWait ? plannedWait.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit datagram deadline is not representable"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, deadline.value(), active.dispatchOnMaster,
        plannedWait.value(),
        static_cast<std::size_t>(wire.value()),
        active.committedWireBytes + wire.value()));
}

::media::Result<MediaTsPreparedDatagramEmission>
MediaTsDatagramEmissionSchedule::prepareMaintenance(
    std::size_t payloadBytes,
    MediaRunningTime deadline)
{
    using Result = ::media::Result<MediaTsPreparedDatagramEmission>;
    if (!m_state || m_state->activeAccessUnit ||
        m_state->pendingRevision || deadline < m_state->origin) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS maintenance datagram cannot be prepared"));
    }
    auto wire = wireBytesForPayload(m_state->plan, payloadBytes);
    auto latestEmission = deadline.checkedAdd(
        m_state->plan.accessUnitWindow());
    if (!wire || wire.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()) ||
        !latestEmission ||
        m_state->nextRevision ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::failure(
            !wire ? wire.error() :
            !latestEmission ? latestEmission.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance revision space is exhausted"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, deadline, latestEmission.value(),
        MediaRunningTime::fromNanoseconds(0),
        static_cast<std::size_t>(wire.value()), 0));
}

::media::Status MediaTsDatagramEmissionSchedule::commit(
    MediaTsPreparedDatagramEmission&& prepared,
    MediaRunningTime actualEmissionTime)
{
    if (!m_state || !prepared.m_active ||
        prepared.m_state.get() != m_state.get() ||
        m_state->pendingRevision != prepared.m_revision ||
        actualEmissionTime < prepared.m_deadline ||
        actualEmissionTime > prepared.m_latestEmissionTime) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission commit rejects a stale transaction"));
    }
    if (m_state->activeAccessUnit) {
        m_state->activeAccessUnit->committedWireBytes =
            prepared.m_nextCommittedWireBytes;
        auto& latest = m_state->activeAccessUnit->latestActualEmission;
        latest = latest
            ? (std::max)(*latest, actualEmissionTime)
            : actualEmissionTime;
    } else if (prepared.m_nextCommittedWireBytes != 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance commit contains access-unit progress"));
    }
    m_state->pendingRevision.reset();
    prepared.m_active = false;
    prepared.m_state.reset();
    return ::media::Status::success();
}

::media::Status MediaTsDatagramEmissionSchedule::completeAccessUnit()
{
    if (!m_state || !m_state->activeAccessUnit ||
        m_state->pendingRevision ||
        m_state->activeAccessUnit->committedWireBytes !=
            m_state->activeAccessUnit->totalWireBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit emission cannot complete"));
    }
    auto& active = *m_state->activeAccessUnit;
    observationFor(*m_state, active.stream) = active.proposedObservation;
    m_state->availableAt = active.latestActualEmission
        ? (std::max)(active.completionDeadline,
                     *active.latestActualEmission)
        : active.completionDeadline;
    m_state->activeAccessUnit.reset();
    return ::media::Status::success();
}

const MediaTsDatagramEmissionPlan&
MediaTsDatagramEmissionSchedule::plan() const noexcept
{
    return m_state->plan;
}

} // namespace media::ffmpeg::graph
