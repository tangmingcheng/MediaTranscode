#include "internal/graph/diagnostics/MediaTsEmissionDiagnostics.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

template <typename Value>
void saturatingAdd(Value& target, Value added) noexcept
{
    const Value maximum = (std::numeric_limits<Value>::max)();
    target = target > maximum - added ? maximum : target + added;
}

} // namespace

void MediaTsEmissionDiagnostics::recordCommittedDatagram(
    std::size_t wireBytes,
    MediaRunningTime plannedWait,
    MediaRunningTime deadline,
    MediaRunningTime actualEmission) noexcept
{
    saturatingAdd(m_snapshot.datagrams, std::uint64_t{1});
    saturatingAdd(
        m_snapshot.wireBytes, static_cast<std::uint64_t>(wireBytes));
    const std::int64_t waitNanoseconds =
        (std::max)(plannedWait.nanoseconds(), std::int64_t{0});
    if (waitNanoseconds == 0) {
        saturatingAdd(m_snapshot.immediateDeadlines, std::uint64_t{1});
    } else {
        saturatingAdd(m_snapshot.deferredDeadlines, std::uint64_t{1});
        saturatingAdd(m_snapshot.plannedWaitNanoseconds, waitNanoseconds);
    }
    if (actualEmission > deadline) {
        const auto lateness = actualEmission.checkedSubtract(deadline);
        if (lateness) {
            saturatingAdd(m_snapshot.lateDatagrams, std::uint64_t{1});
            m_snapshot.maximumLatenessNanoseconds = (std::max)(
                m_snapshot.maximumLatenessNanoseconds,
                lateness.value().nanoseconds());
        }
    }
}

void MediaTsEmissionDiagnostics::recordPendingBytes(
    std::size_t bytes) noexcept
{
    m_snapshot.pendingBytes = bytes;
    m_snapshot.peakPendingBytes =
        (std::max)(m_snapshot.peakPendingBytes, bytes);
}

void MediaTsEmissionDiagnostics::recordPressureFailure() noexcept
{
    saturatingAdd(m_snapshot.pressureFailures, std::uint64_t{1});
}

void MediaTsEmissionDiagnostics::recordAccessUnitDecision(
    const MediaTsAccessUnitEmissionDecision& decision) noexcept
{
    saturatingAdd(m_snapshot.accessUnits, std::uint64_t{1});
    m_snapshot.currentSchedulingDebtNanoseconds =
        (std::max)(decision.schedulingDebt.nanoseconds(), std::int64_t{0});
    m_snapshot.maximumSchedulingDebtNanoseconds = (std::max)(
        m_snapshot.maximumSchedulingDebtNanoseconds,
        m_snapshot.currentSchedulingDebtNanoseconds);
    m_snapshot.selectedWireBytesPerSecond =
        decision.selectedWireBytesPerSecond;
    m_snapshot.maximumSelectedWireBytesPerSecond = (std::max)(
        m_snapshot.maximumSelectedWireBytesPerSecond,
        decision.selectedWireBytesPerSecond);
}

void MediaTsEmissionDiagnostics::recordAccessUnitCompleted() noexcept
{
    m_snapshot.currentSchedulingDebtNanoseconds = 0;
}

const MediaTsEmissionSnapshot&
MediaTsEmissionDiagnostics::snapshot() const noexcept
{
    return m_snapshot;
}

void MediaTsEmissionDiagnostics::logPlan(
    const MediaTsDatagramEmissionPlan& plan,
    std::uint64_t generation) const
{
    std::ostringstream out;
    out << "mpegts_emission_plan generation=" << generation
        << " mode=canonical_access_unit_window"
        << " access_unit_window_ns="
        << plan.accessUnitWindow().nanoseconds()
        << " video_initial_service_window_ns="
        << plan.videoInitialServiceWindow().nanoseconds()
        << " audio_initial_service_window_ns="
        << (plan.audioInitialServiceWindow()
                ? plan.audioInitialServiceWindow()->nanoseconds()
                : 0)
        << " packet_size_bytes=" << plan.packetSizeBytes()
        << " maximum_payload_bytes=" << plan.maximumPayloadBytes()
        << " per_datagram_overhead_bytes="
        << plan.perDatagramOverheadBytes();
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        out.str());
}

void MediaTsEmissionDiagnostics::logSnapshot(
    std::string_view stage,
    std::string_view exitReason,
    std::uint64_t generation) const
{
    std::ostringstream out;
    out << "mpegts_emission stage=" << stage
        << " generation=" << generation
        << " datagrams=" << m_snapshot.datagrams
        << " wire_bytes=" << m_snapshot.wireBytes
        << " immediate_deadlines=" << m_snapshot.immediateDeadlines
        << " deferred_deadlines=" << m_snapshot.deferredDeadlines
        << " planned_wait_ns=" << m_snapshot.plannedWaitNanoseconds
        << " late_datagrams=" << m_snapshot.lateDatagrams
        << " maximum_lateness_ns="
        << m_snapshot.maximumLatenessNanoseconds
        << " pending_bytes=" << m_snapshot.pendingBytes
        << " peak_pending_bytes=" << m_snapshot.peakPendingBytes
        << " pressure_failures=" << m_snapshot.pressureFailures
        << " access_units=" << m_snapshot.accessUnits
        << " current_scheduling_debt_ns="
        << m_snapshot.currentSchedulingDebtNanoseconds
        << " maximum_scheduling_debt_ns="
        << m_snapshot.maximumSchedulingDebtNanoseconds
        << " selected_wire_bytes_per_second="
        << m_snapshot.selectedWireBytesPerSecond
        << " maximum_selected_wire_bytes_per_second="
        << m_snapshot.maximumSelectedWireBytesPerSecond
        << " exit_reason=" << exitReason;
    mediaGraphDiagnosticLog(
        stage == "final"
            ? MediaGraphDiagnosticLevel::Summary
            : MediaGraphDiagnosticLevel::Flow,
        MediaGraphDiagnosticPhase::RuntimeNode,
        out.str());
}

} // namespace media::ffmpeg::graph
