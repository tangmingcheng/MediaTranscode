#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;

struct UInt128 final {
    std::uint64_t high;
    std::uint64_t low;
};

UInt128 multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
#if defined(_MSC_VER) && defined(_M_X64)
    std::uint64_t high = 0;
    const std::uint64_t low = _umul128(lhs, rhs, &high);
    return UInt128{high, low};
#elif defined(__SIZEOF_INT128__)
    const auto product = static_cast<unsigned __int128>(lhs) * rhs;
    return UInt128{
        static_cast<std::uint64_t>(product >> 64),
        static_cast<std::uint64_t>(product)};
#else
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
#endif
}

struct DivisionResult final {
    std::uint64_t quotient;
    std::uint64_t remainder;
    bool quotientOverflow;
};

DivisionResult divide(UInt128 dividend, std::uint64_t divisor) noexcept
{
#if defined(_MSC_VER) && defined(_M_X64)
    if (dividend.high >= divisor) {
        return DivisionResult{0, 0, true};
    }
    std::uint64_t remainder = 0;
    const std::uint64_t quotient = _udiv128(
        dividend.high, dividend.low, divisor, &remainder);
    return DivisionResult{quotient, remainder, false};
#elif defined(__SIZEOF_INT128__)
    const auto value =
        (static_cast<unsigned __int128>(dividend.high) << 64) |
        dividend.low;
    const auto quotient = value / divisor;
    if (quotient > (std::numeric_limits<std::uint64_t>::max)()) {
        return DivisionResult{0, 0, true};
    }
    return DivisionResult{
        static_cast<std::uint64_t>(quotient),
        static_cast<std::uint64_t>(value % divisor), false};
#else
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
#endif
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
        MediaRunningTime timelineAvailableAt;
        std::uint64_t totalWireBytes;
        std::uint64_t committedWireBytes;
        std::int64_t selectedWireBytesPerSecond;
        StreamObservation proposedObservation;
    };
    struct ActiveMaintenanceGroup final {
        MediaRunningTime notBefore;
        MediaRunningTime completionDeadline;
        MediaRunningTime reservationStart;
        MediaRunningTime timelineAvailableAt;
        std::uint64_t totalWireBytes;
        std::uint64_t committedWireBytes;
        std::int64_t selectedWireBytesPerSecond;
        std::int64_t proposedMaintenanceWireBytesPerSecond;
        bool scheduledReservation;
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
    std::int64_t transportWireBytesPerSecond = 0;
    std::int64_t maintenanceWireBytesPerSecond = 0;
    std::optional<StreamObservation> videoObservation;
    std::optional<StreamObservation> audioObservation;
    std::optional<ActiveAccessUnit> activeAccessUnit;
    std::optional<ActiveMaintenanceGroup> activeMaintenanceGroup;
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

::media::Result<std::uint64_t> wireBytesForGroup(
    const MediaTsDatagramEmissionPlan& plan,
    std::size_t payloadBytes,
    std::size_t datagramCount)
{
    if (payloadBytes == 0 || datagramCount == 0 ||
        plan.packetSizeBytes() == 0 || plan.maximumPayloadBytes() == 0 ||
        payloadBytes % plan.packetSizeBytes() != 0 ||
        datagramCount > (std::numeric_limits<std::size_t>::max)() /
            plan.packetSizeBytes() ||
        datagramCount > (std::numeric_limits<std::size_t>::max)() /
            plan.maximumPayloadBytes() ||
        payloadBytes < datagramCount * plan.packetSizeBytes() ||
        payloadBytes > datagramCount * plan.maximumPayloadBytes()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance group geometry is invalid"));
    }
    const auto payload = static_cast<std::uint64_t>(payloadBytes);
    const auto datagrams = static_cast<std::uint64_t>(datagramCount);
    if (plan.perDatagramOverheadBytes() != 0 && datagrams >
        ((std::numeric_limits<std::uint64_t>::max)() - payload) /
            plan.perDatagramOverheadBytes()) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance group wire bytes are not representable"));
    }
    return ::media::Result<std::uint64_t>::success(
        payload + datagrams * plan.perDatagramOverheadBytes());
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

struct AccessUnitReservation final {
    MediaRunningTime notBefore;
    MediaRunningTime completion;
    MediaRunningTime plannedWait;
    MediaRunningTime serviceDuration;
    std::uint64_t wireBytes;
    std::uint64_t nextCommittedWireBytes;
    std::int64_t selectedWireBytesPerSecond;
};

::media::Result<AccessUnitReservation> accessUnitReservation(
    MediaTsDatagramEmissionScheduleState& state,
    std::size_t payloadBytes)
{
    using Result = ::media::Result<AccessUnitReservation>;
    if (!state.activeAccessUnit || state.activeMaintenanceGroup ||
        state.pendingRevision) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram cannot be reserved"));
    }
    auto wire = wireBytesForPayload(state.plan, payloadBytes);
    if (!wire) return Result::failure(wire.error());
    auto& active = *state.activeAccessUnit;
    if (active.committedWireBytes > active.totalWireBytes ||
        wire.value() > active.totalWireBytes - active.committedWireBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram exceeds its materialized wire bytes"));
    }
    const std::uint64_t remainingWireBytes =
        active.totalWireBytes - active.committedWireBytes;
    auto remainingWindow = active.dispatchOnMaster.checkedSubtract(
        active.timelineAvailableAt);
    auto requiredRate = remainingWindow &&
            remainingWindow.value().nanoseconds() > 0
        ? rateForWindow(remainingWireBytes, remainingWindow.value())
        : ::media::Result<std::int64_t>::failure(
              ::media::ErrorInfo::invalidArgument(
                  "MPEG-TS access-unit remaining service window is exhausted"));
    if (!requiredRate) return Result::failure(requiredRate.error());
    const std::int64_t selectedRate = (std::max)(
        active.selectedWireBytesPerSecond, requiredRate.value());
    auto serviceDuration = rateDuration(wire.value(), selectedRate, true);
    auto plannedWait = active.timelineAvailableAt.checkedSubtract(
        active.emitOnMaster);
    auto completion = serviceDuration
        ? active.timelineAvailableAt.checkedAdd(serviceDuration.value())
        : ::media::Result<MediaRunningTime>::failure(serviceDuration.error());
    if (!serviceDuration || !plannedWait || !completion ||
        serviceDuration.value().nanoseconds() <= 0 ||
        completion.value() > active.dispatchOnMaster) {
        return Result::failure(
            !serviceDuration ? serviceDuration.error() :
            !plannedWait ? plannedWait.error() :
            !completion ? completion.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit datagram reservation is not representable"));
    }
    return Result::success(AccessUnitReservation{
        active.timelineAvailableAt, completion.value(), plannedWait.value(),
        serviceDuration.value(), wire.value(),
        active.committedWireBytes + wire.value(), selectedRate});
}

} // namespace

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state,
    std::uint64_t revision,
    MediaRunningTime deadline,
    MediaRunningTime latestEmissionTime,
    MediaRunningTime plannedWait,
    MediaRunningTime serviceDuration,
        std::size_t wireBytes,
        std::uint64_t nextCommittedWireBytes,
        MediaRunningTime reservationCompletion,
        std::int64_t selectedWireBytesPerSecond,
        bool maintenanceReservation) noexcept
    : m_state(std::move(state)),
      m_revision(revision),
      m_deadline(deadline),
      m_latestEmissionTime(latestEmissionTime),
      m_plannedWait(plannedWait),
      m_serviceDuration(serviceDuration),
      m_wireBytes(wireBytes),
      m_nextCommittedWireBytes(nextCommittedWireBytes),
      m_reservationCompletion(reservationCompletion),
      m_selectedWireBytesPerSecond(selectedWireBytesPerSecond),
      m_maintenanceReservation(maintenanceReservation),
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
      m_serviceDuration(other.m_serviceDuration),
      m_wireBytes(other.m_wireBytes),
      m_nextCommittedWireBytes(other.m_nextCommittedWireBytes),
      m_reservationCompletion(other.m_reservationCompletion),
      m_selectedWireBytesPerSecond(other.m_selectedWireBytesPerSecond),
      m_maintenanceReservation(other.m_maintenanceReservation),
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
    m_serviceDuration = other.m_serviceDuration;
    m_wireBytes = other.m_wireBytes;
    m_nextCommittedWireBytes = other.m_nextCommittedWireBytes;
    m_reservationCompletion = other.m_reservationCompletion;
    m_selectedWireBytesPerSecond = other.m_selectedWireBytesPerSecond;
    m_maintenanceReservation = other.m_maintenanceReservation;
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

MediaRunningTime
MediaTsPreparedDatagramEmission::serviceDuration() const noexcept
{
    return m_serviceDuration;
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
        m_state->activeMaintenanceGroup || m_state->pendingRevision ||
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
    auto completionWindow = dispatchOnMaster.checkedSubtract(
        serviceStart);
    if (!completionWindow || completionWindow.value().nanoseconds() <= 0) {
        std::ostringstream message;
        message << "MPEG-TS access-unit canonical window is exhausted"
                << " stream="
                << (stream == MediaScheduledStream::Video ? "video" : "audio")
                << " payload_bytes=" << totalPayloadBytes
                << " wire_bytes=" << totalWire.value()
                << " emit_ns=" << emitOnMaster.nanoseconds()
                << " dispatch_ns=" << dispatchOnMaster.nanoseconds()
                << " actual_now_ns=" << actualMasterNow.nanoseconds()
                << " available_at_ns=" << m_state->availableAt.nanoseconds()
                << " service_start_ns=" << serviceStart.nanoseconds();
        return Result::failure(
            completionWindow
                ? ::media::ErrorInfo::invalidArgument(
                      message.str())
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
    if (m_state->maintenanceWireBytesPerSecond < 0 ||
        aggregateRate > (std::numeric_limits<std::int64_t>::max)() -
            m_state->maintenanceWireBytesPerSecond) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS aggregate maintenance rate is not representable"));
    }
    aggregateRate += m_state->maintenanceWireBytesPerSecond;
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
    if (!completion || !debt ||
        completion.value() > dispatchOnMaster) {
        return Result::failure(
            !completion ? completion.error() :
            !debt ? debt.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit cannot meet its canonical dispatch deadline"));
    }
    m_state->activeAccessUnit.emplace(
        MediaTsDatagramEmissionScheduleState::ActiveAccessUnit{
            stream, emitOnMaster, dispatchOnMaster, serviceStart,
            totalWire.value(), 0, selectedRate,
            {emitOnMaster, currentRate.value()}});
    return Result::success(MediaTsAccessUnitEmissionDecision{
        selectedRate, debt.value(), completion.value()});
}

::media::Result<MediaTsPreparedDatagramEmission>
MediaTsDatagramEmissionSchedule::prepareAccessUnit(
    std::size_t payloadBytes)
{
    using Result = ::media::Result<MediaTsPreparedDatagramEmission>;
    if (!m_state) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram cannot be prepared"));
    }
    auto reservation = accessUnitReservation(*m_state, payloadBytes);
    if (!reservation) return Result::failure(reservation.error());
    if (reservation.value().wireBytes >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()) ||
        m_state->nextRevision ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit datagram transaction is not representable"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, reservation.value().notBefore,
        m_state->activeAccessUnit->dispatchOnMaster,
        reservation.value().plannedWait,
        reservation.value().serviceDuration,
        static_cast<std::size_t>(reservation.value().wireBytes),
        reservation.value().nextCommittedWireBytes,
        reservation.value().completion,
        reservation.value().selectedWireBytesPerSecond, false));
}

::media::Result<MediaTsDatagramReservationPreview>
MediaTsDatagramEmissionSchedule::previewAccessUnit(
    std::size_t payloadBytes) const
{
    if (!m_state) {
        return ::media::Result<MediaTsDatagramReservationPreview>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS access-unit reservation preview has no schedule"));
    }
    auto reservation = accessUnitReservation(*m_state, payloadBytes);
    if (!reservation) {
        return ::media::Result<MediaTsDatagramReservationPreview>::failure(
            reservation.error());
    }
    return ::media::Result<MediaTsDatagramReservationPreview>::success(
        MediaTsDatagramReservationPreview{
            reservation.value().notBefore,
            reservation.value().completion});
}

::media::Status MediaTsDatagramEmissionSchedule::beginMaintenanceGroup(
    std::size_t totalPayloadBytes,
    std::size_t datagramCount,
    MediaRunningTime notBefore,
    MediaRunningTime completionDeadline)
{
    if (!m_state || m_state->activeMaintenanceGroup ||
        m_state->pendingRevision || notBefore < m_state->origin ||
        completionDeadline <= notBefore) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance group cannot begin"));
    }
    auto totalWire = wireBytesForGroup(
        m_state->plan, totalPayloadBytes, datagramCount);
    if (!totalWire) {
        return ::media::Status::failure(totalWire.error());
    }
    const bool scheduledReservation =
        m_state->plan.usesScheduledDatagramOutput();
    const MediaRunningTime parentAvailableAt = m_state->activeAccessUnit
        ? m_state->activeAccessUnit->timelineAvailableAt
        : m_state->availableAt;
    const MediaRunningTime serviceStart = scheduledReservation
        ? (std::max)(notBefore, parentAvailableAt)
        : notBefore;
    std::int64_t selectedRate = 0;
    std::int64_t proposedMaintenanceRate =
        m_state->maintenanceWireBytesPerSecond;
    if (scheduledReservation) {
        auto maintenancePeriod = completionDeadline.checkedSubtract(notBefore);
        auto periodicMaintenanceRate = maintenancePeriod &&
                maintenancePeriod.value().nanoseconds() > 0
            ? rateForWindow(totalWire.value(), maintenancePeriod.value())
            : ::media::Result<std::int64_t>::failure(
                  ::media::ErrorInfo::invalidArgument(
                      "MPEG-TS maintenance period is exhausted"));
        if (!periodicMaintenanceRate) {
            return ::media::Status::failure(
                periodicMaintenanceRate.error());
        }
        proposedMaintenanceRate = (std::max)(
            proposedMaintenanceRate, periodicMaintenanceRate.value());
        auto groupWindow = completionDeadline.checkedSubtract(serviceStart);
        if (!groupWindow || groupWindow.value().nanoseconds() <= 0) {
            return ::media::Status::failure(
                groupWindow
                    ? ::media::ErrorInfo::invalidArgument(
                          "MPEG-TS maintenance group completion window is exhausted")
                    : groupWindow.error());
        }
        auto requiredGroupRate = rateForWindow(
            totalWire.value(), groupWindow.value());
        if (!requiredGroupRate) {
            return ::media::Status::failure(requiredGroupRate.error());
        }
        selectedRate = (std::max)(
            m_state->transportWireBytesPerSecond,
            requiredGroupRate.value());
        if (m_state->activeAccessUnit) {
            auto activeWindow = m_state->activeAccessUnit->dispatchOnMaster
                .checkedSubtract(serviceStart);
            const std::uint64_t remainingMediaWire =
                m_state->activeAccessUnit->totalWireBytes -
                m_state->activeAccessUnit->committedWireBytes;
            if (!activeWindow || activeWindow.value().nanoseconds() <= 0 ||
                remainingMediaWire >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        totalWire.value()) {
                return ::media::Status::failure(
                    activeWindow && activeWindow.value().nanoseconds() > 0
                        ? ::media::ErrorInfo::invalidArgument(
                              "MPEG-TS active maintenance demand is not representable")
                        : activeWindow
                            ? ::media::ErrorInfo::invalidArgument(
                                  "MPEG-TS active maintenance exhausted the access-unit window")
                            : activeWindow.error());
            }
            auto requiredAggregateRate = rateForWindow(
                remainingMediaWire + totalWire.value(),
                activeWindow.value());
            if (!requiredAggregateRate) {
                return ::media::Status::failure(
                    requiredAggregateRate.error());
            }
            selectedRate = (std::max)(
                selectedRate,
                (std::max)(
                    m_state->activeAccessUnit->selectedWireBytesPerSecond,
                    requiredAggregateRate.value()));
        }
    }
    m_state->activeMaintenanceGroup.emplace(
        MediaTsDatagramEmissionScheduleState::ActiveMaintenanceGroup{
            notBefore, completionDeadline, serviceStart, serviceStart,
            totalWire.value(), 0, selectedRate, proposedMaintenanceRate,
            scheduledReservation});
    return ::media::Status::success();
}

::media::Result<MediaTsPreparedDatagramEmission>
MediaTsDatagramEmissionSchedule::prepareMaintenance(
    std::size_t payloadBytes)
{
    using Result = ::media::Result<MediaTsPreparedDatagramEmission>;
    if (!m_state || !m_state->activeMaintenanceGroup ||
        m_state->pendingRevision) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS maintenance datagram cannot be prepared"));
    }
    auto wire = wireBytesForPayload(m_state->plan, payloadBytes);
    auto& group = *m_state->activeMaintenanceGroup;
    if (!wire || group.committedWireBytes > group.totalWireBytes ||
        wire.value() > group.totalWireBytes - group.committedWireBytes) {
        return Result::failure(
            wire ? ::media::ErrorInfo::invalidArgument(
                       "MPEG-TS maintenance datagram exceeds its group geometry")
                 : wire.error());
    }
    const MediaRunningTime serviceStart = group.timelineAvailableAt;
    const std::uint64_t nextCommittedWireBytes =
        group.committedWireBytes + wire.value();
    auto cumulativeDuration = group.scheduledReservation
        ? rateDuration(
              nextCommittedWireBytes,
              group.selectedWireBytesPerSecond, true)
        : ::media::Result<MediaRunningTime>::success(
              MediaRunningTime::fromNanoseconds(0));
    auto reservationCompletion = cumulativeDuration
        ? group.reservationStart.checkedAdd(cumulativeDuration.value())
        : ::media::Result<MediaRunningTime>::failure(
              cumulativeDuration.error());
    auto serviceDuration = reservationCompletion
        ? reservationCompletion.value().checkedSubtract(serviceStart)
        : ::media::Result<MediaRunningTime>::failure(
              reservationCompletion.error());
    auto plannedWait = serviceStart.checkedSubtract(group.notBefore);
    if (!wire || wire.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()) ||
        !serviceDuration || !reservationCompletion ||
        !plannedWait ||
        (group.scheduledReservation &&
         reservationCompletion.value() > group.completionDeadline) ||
        m_state->nextRevision ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        if (wire && serviceDuration && reservationCompletion && plannedWait &&
            group.scheduledReservation &&
            reservationCompletion.value() > group.completionDeadline) {
            std::ostringstream message;
            message << "MPEG-TS maintenance cannot reserve its global service window"
                    << " group_not_before_ns=" << group.notBefore.nanoseconds()
                    << " group_deadline_ns="
                    << group.completionDeadline.nanoseconds()
                    << " service_start_ns=" << serviceStart.nanoseconds()
                    << " service_duration_ns="
                    << serviceDuration.value().nanoseconds()
                    << " reservation_completion_ns="
                    << reservationCompletion.value().nanoseconds()
                    << " datagram_wire_bytes=" << wire.value()
                    << " committed_wire_bytes=" << group.committedWireBytes
                    << " total_wire_bytes=" << group.totalWireBytes
                    << " selected_wire_bytes_per_second="
                    << group.selectedWireBytesPerSecond;
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(message.str()));
        }
        return Result::failure(
            !wire ? wire.error() :
            !serviceDuration ? serviceDuration.error() :
            !reservationCompletion ? reservationCompletion.error() :
            !plannedWait ? plannedWait.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance cannot reserve its global service window"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, serviceStart, group.completionDeadline,
        plannedWait.value(), serviceDuration.value(),
        static_cast<std::size_t>(wire.value()),
        nextCommittedWireBytes,
        reservationCompletion.value(), group.selectedWireBytesPerSecond,
        true));
}

::media::Status MediaTsDatagramEmissionSchedule::completeMaintenanceGroup()
{
    if (!m_state || !m_state->activeMaintenanceGroup ||
        m_state->pendingRevision ||
        m_state->activeMaintenanceGroup->committedWireBytes !=
            m_state->activeMaintenanceGroup->totalWireBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance group cannot complete"));
    }
    auto& group = *m_state->activeMaintenanceGroup;
    if (group.scheduledReservation && m_state->activeAccessUnit) {
        m_state->activeAccessUnit->timelineAvailableAt =
            group.timelineAvailableAt;
        m_state->activeAccessUnit->selectedWireBytesPerSecond =
            group.selectedWireBytesPerSecond;
    } else if (group.scheduledReservation) {
        m_state->availableAt = group.timelineAvailableAt;
        m_state->transportWireBytesPerSecond =
            group.selectedWireBytesPerSecond;
    }
    if (group.scheduledReservation) {
        m_state->maintenanceWireBytesPerSecond =
            group.proposedMaintenanceWireBytesPerSecond;
    }
    m_state->activeMaintenanceGroup.reset();
    return ::media::Status::success();
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
    if (prepared.m_maintenanceReservation) {
        if (!m_state->activeMaintenanceGroup ||
            prepared.m_nextCommittedWireBytes <=
                m_state->activeMaintenanceGroup->committedWireBytes ||
            prepared.m_nextCommittedWireBytes >
                m_state->activeMaintenanceGroup->totalWireBytes ||
            prepared.m_reservationCompletion <
                m_state->activeMaintenanceGroup->timelineAvailableAt ||
            (m_state->activeMaintenanceGroup->scheduledReservation &&
             prepared.m_selectedWireBytesPerSecond <= 0)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS maintenance reservation conflicts with the global timeline"));
        }
        m_state->activeMaintenanceGroup->committedWireBytes =
            prepared.m_nextCommittedWireBytes;
        m_state->activeMaintenanceGroup->timelineAvailableAt =
            prepared.m_reservationCompletion;
    } else if (m_state->activeAccessUnit) {
        if (prepared.m_selectedWireBytesPerSecond <= 0 ||
            prepared.m_reservationCompletion <
                m_state->activeAccessUnit->timelineAvailableAt) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS access-unit reservation conflicts with the global timeline"));
        }
        m_state->activeAccessUnit->selectedWireBytesPerSecond =
            prepared.m_selectedWireBytesPerSecond;
        m_state->activeAccessUnit->timelineAvailableAt = (std::max)(
            prepared.m_reservationCompletion, actualEmissionTime);
        m_state->activeAccessUnit->committedWireBytes =
            prepared.m_nextCommittedWireBytes;
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
    m_state->transportWireBytesPerSecond =
        active.selectedWireBytesPerSecond;
    m_state->availableAt = active.timelineAvailableAt;
    m_state->activeAccessUnit.reset();
    return ::media::Status::success();
}

const MediaTsDatagramEmissionPlan&
MediaTsDatagramEmissionSchedule::plan() const noexcept
{
    return m_state->plan;
}

} // namespace media::ffmpeg::graph
