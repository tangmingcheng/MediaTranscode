#include "internal/graph/protocol/mpegts/MediaTsDatagramEmissionSchedule.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
class MediaTsDatagramEmissionScheduleState final {
public:
    struct ActiveAccessUnit final {
        MediaScheduledStream stream;
        MediaRunningTime emitOnMaster;
        MediaRunningTime completionDeadline;
        MediaRunningTime reservationStart;
        MediaRunningTime timelineAvailableAt;
        std::uint64_t totalWireBytes;
        std::uint64_t committedWireBytes;
    };
    struct ActiveMaintenanceGroup final {
        MediaRunningTime notBefore;
        MediaRunningTime completionDeadline;
        MediaRunningTime reservationStart;
        MediaRunningTime timelineAvailableAt;
        std::uint64_t totalWireBytes;
        std::uint64_t committedWireBytes;
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

struct AccessUnitReservation final {
    MediaRunningTime notBefore;
    MediaRunningTime completion;
    MediaRunningTime plannedWait;
    std::uint64_t wireBytes;
    std::uint64_t nextCommittedWireBytes;
};

MediaRunningTime currentTimelineAvailableAt(
    const MediaTsDatagramEmissionScheduleState& state) noexcept
{
    if (state.activeMaintenanceGroup) {
        return state.activeMaintenanceGroup->timelineAvailableAt;
    }
    if (state.activeAccessUnit) {
        return state.activeAccessUnit->timelineAvailableAt;
    }
    return state.availableAt;
}

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
    const std::uint64_t nextCommittedWireBytes =
        active.committedWireBytes + wire.value();
    auto plannedWait = active.timelineAvailableAt.checkedSubtract(
        active.emitOnMaster);
    if (!plannedWait ||
        active.timelineAvailableAt > active.completionDeadline) {
        return Result::failure(
            !plannedWait ? plannedWait.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit datagram reservation is not representable"));
    }
    return Result::success(AccessUnitReservation{
        active.timelineAvailableAt, active.timelineAvailableAt,
        plannedWait.value(), wire.value(), nextCommittedWireBytes});
}

} // namespace

MediaTsPreparedDatagramEmission::MediaTsPreparedDatagramEmission(
    std::shared_ptr<MediaTsDatagramEmissionScheduleState> state,
    std::uint64_t revision,
    MediaRunningTime deadline,
    MediaRunningTime latestEmissionTime,
    MediaRunningTime plannedWait,
    std::size_t wireBytes,
    std::uint64_t nextCommittedWireBytes,
    MediaRunningTime reservationCompletion,
    bool maintenanceReservation) noexcept
    : m_state(std::move(state)),
      m_revision(revision),
      m_deadline(deadline),
      m_latestEmissionTime(latestEmissionTime),
      m_plannedWait(plannedWait),
      m_wireBytes(wireBytes),
      m_nextCommittedWireBytes(nextCommittedWireBytes),
      m_reservationCompletion(reservationCompletion),
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
      m_wireBytes(other.m_wireBytes),
      m_nextCommittedWireBytes(other.m_nextCommittedWireBytes),
      m_reservationCompletion(other.m_reservationCompletion),
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
    m_wireBytes = other.m_wireBytes;
    m_nextCommittedWireBytes = other.m_nextCommittedWireBytes;
    m_reservationCompletion = other.m_reservationCompletion;
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
    (void)actualMasterNow;
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
    if (totalWire.value() > m_state->plan.maximumQueuedBytes()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access-unit exceeds the planned emission byte bound"));
    }
    const MediaRunningTime serviceStart =
        (std::max)(emitOnMaster, m_state->availableAt);
    auto remainingWindow = dispatchOnMaster.checkedSubtract(serviceStart);
    const std::optional<MediaRunningTime> plannedServiceWindow =
        stream == MediaScheduledStream::Video
            ? std::optional<MediaRunningTime>(
                  m_state->plan.videoInitialServiceWindow())
            : m_state->plan.audioInitialServiceWindow();
    if (!remainingWindow) return Result::failure(remainingWindow.error());
    if (remainingWindow.value().nanoseconds() <= 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access unit has no transport service window"));
    }
    if (!plannedServiceWindow) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS access unit has no planned service window"));
    }
    const MediaRunningTime serviceWindow = (std::min)(
        remainingWindow.value(), *plannedServiceWindow);
    (void)serviceWindow;
    auto debt = serviceStart.checkedSubtract(emitOnMaster);
    if (!debt || serviceStart > dispatchOnMaster) {
        return Result::failure(
            !debt ? debt.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access-unit exceeds its transport dispatch deadline"));
    }
    m_state->activeAccessUnit.emplace(
        MediaTsDatagramEmissionScheduleState::ActiveAccessUnit{
            stream, emitOnMaster, dispatchOnMaster, serviceStart,
            serviceStart,
            totalWire.value(), 0});
    return Result::success(MediaTsAccessUnitEmissionDecision{
        debt.value(), dispatchOnMaster});
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
        m_state->activeAccessUnit->completionDeadline,
        reservation.value().plannedWait,
        static_cast<std::size_t>(reservation.value().wireBytes),
        reservation.value().nextCommittedWireBytes,
        reservation.value().completion, false));
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
    const MediaRunningTime parentAvailableAt = m_state->activeAccessUnit
        ? m_state->activeAccessUnit->timelineAvailableAt
        : m_state->availableAt;
    const MediaRunningTime serviceStart =
        (std::max)(notBefore, parentAvailableAt);
    if (serviceStart > completionDeadline) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance exceeds its canonical deadline"));
    }
    m_state->activeMaintenanceGroup.emplace(
        MediaTsDatagramEmissionScheduleState::ActiveMaintenanceGroup{
            notBefore, completionDeadline, serviceStart, serviceStart,
            totalWire.value(), 0});
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
    auto plannedWait = serviceStart.checkedSubtract(group.notBefore);
    if (!wire || wire.value() >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()) ||
        !plannedWait ||
        serviceStart > group.completionDeadline ||
        m_state->nextRevision ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        if (wire && plannedWait &&
            serviceStart > group.completionDeadline) {
            std::ostringstream message;
            message << "MPEG-TS maintenance cannot reserve its global service window"
                    << " group_not_before_ns=" << group.notBefore.nanoseconds()
                    << " group_deadline_ns="
                    << group.completionDeadline.nanoseconds()
                    << " service_start_ns=" << serviceStart.nanoseconds()
                    << " reservation_completion_ns="
                    << serviceStart.nanoseconds()
                    << " datagram_wire_bytes=" << wire.value()
                    << " committed_wire_bytes=" << group.committedWireBytes
                    << " total_wire_bytes=" << group.totalWireBytes;
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(message.str()));
        }
        return Result::failure(
            !wire ? wire.error() :
            !plannedWait ? plannedWait.error() :
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS maintenance cannot reserve its global service window"));
    }
    const std::uint64_t revision = m_state->nextRevision++;
    m_state->pendingRevision = revision;
    return Result::success(MediaTsPreparedDatagramEmission(
        m_state, revision, serviceStart, group.completionDeadline,
        plannedWait.value(),
        static_cast<std::size_t>(wire.value()),
        nextCommittedWireBytes,
        serviceStart, true));
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
    if (m_state->activeAccessUnit) {
        m_state->activeAccessUnit->timelineAvailableAt =
            group.timelineAvailableAt;
    } else {
        m_state->availableAt = group.timelineAvailableAt;
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
    const MediaRunningTime timelineAvailableAt =
        currentTimelineAvailableAt(*m_state);
    if (prepared.m_deadline < timelineAvailableAt) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS emission commit would move the continuous timeline backward"));
    }
    if (prepared.m_maintenanceReservation) {
        if (!m_state->activeMaintenanceGroup ||
            prepared.m_nextCommittedWireBytes <=
                m_state->activeMaintenanceGroup->committedWireBytes ||
            prepared.m_nextCommittedWireBytes >
                m_state->activeMaintenanceGroup->totalWireBytes ||
            prepared.m_reservationCompletion <
                m_state->activeMaintenanceGroup->timelineAvailableAt) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS maintenance reservation conflicts with the global timeline"));
        }
        m_state->activeMaintenanceGroup->committedWireBytes =
            prepared.m_nextCommittedWireBytes;
        m_state->activeMaintenanceGroup->timelineAvailableAt =
            prepared.m_reservationCompletion;
    } else if (m_state->activeAccessUnit) {
        if (prepared.m_reservationCompletion <
                m_state->activeAccessUnit->timelineAvailableAt) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS access-unit reservation conflicts with the global timeline"));
        }
        m_state->activeAccessUnit->timelineAvailableAt =
            prepared.m_reservationCompletion;
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
