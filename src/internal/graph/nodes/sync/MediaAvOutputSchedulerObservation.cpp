#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include <sstream>

namespace media::ffmpeg::graph {
void MediaAvOutputSchedulerNode::observeMissingMediaWait()
{
    if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State) || !m_group) return;
    const auto now = m_group->clock()->now();
    if (!now) return;
    const auto observeHead = [&](auto& held, const auto& other, bool otherEof) {
        if (!held || !held->canonical()) return;
        const auto* restored = other ? other->canonical() : nullptr;
        held->waitObservation().observe(now.value().nanoseconds(), !other && !otherEof,
            restored ? std::optional<std::uint64_t>(restored->generation()) : std::nullopt,
            restored ? std::optional<std::uint64_t>(restored->canonicalSequence().value()) : std::nullopt);
    };
    observeHead(m_generationData->videoHead, m_generationData->audioHead, m_generationData->audioEof);
    observeHead(m_generationData->audioHead, m_generationData->videoHead, m_generationData->videoEof);
    const auto steady = mediaSteadyClockNowNs();
    if (m_currentWait) {
        const auto& restored = m_currentWait->missing == Input::Video
            ? m_generationData->videoHead : m_generationData->audioHead;
        const auto& held = m_currentWait->missing == Input::Video
            ? m_generationData->audioHead : m_generationData->videoHead;
        const auto* unit = held ? held->canonical() : nullptr;
        const bool same = unit && unit->generation() == m_currentWait->generation &&
            unit->canonicalSequence().value() == m_currentWait->sequence;
        if (same) {
            m_currentWait->end = steady;
            m_currentWait->endMaster = now.value().nanoseconds();
            if (restored && restored->canonical()) {
                const auto dispatch = restored->canonicalDispatchTime();
                if (dispatch) {
                    const auto mapped = m_group->mapCanonicalToMaster(dispatch.value());
                    if (mapped) m_currentWait->resumedDispatch = mapped.value().nanoseconds();
                }
            }
            if (!m_maximumWait || m_currentWait->end - m_currentWait->begin >
                m_maximumWait->end - m_maximumWait->begin) m_maximumWait = m_currentWait;
        }
        const bool eof = m_currentWait->missing == Input::Video ? m_generationData->videoEof : m_generationData->audioEof;
        if (!same || restored || eof) m_currentWait.reset();
        else return;
    }
    const std::optional<Input> missing = !m_generationData->videoHead && !m_generationData->videoEof
        ? std::optional<Input>(Input::Video) : !m_generationData->audioHead && !m_generationData->audioEof
        ? std::optional<Input>(Input::Audio) : std::nullopt;
    if (!missing) return;
    const auto& held = *missing == Input::Video ? m_generationData->audioHead : m_generationData->videoHead;
    const auto* unit = held ? held->canonical() : nullptr;
    if (!unit) return;
    const auto dispatch = unit->canonicalDispatch();
    if (!dispatch) return;
    const auto mapped = m_group->mapCanonicalToMaster(dispatch.value());
    if (!mapped) return;
    m_currentWait = MissingWaitObservation{unit->generation(), unit->canonicalSequence().value(), *missing,
        steady, steady, mapped.value().nanoseconds(), now.value().nanoseconds(), now.value().nanoseconds(), std::nullopt};
}

void MediaAvOutputSchedulerNode::logCorrelatedWait() const noexcept
{
    if (!m_maximumWait) return;
    try {
        const auto& record = *m_maximumWait;
        std::ostringstream out;
        out << "av_scheduler_correlated_wait generation=" << record.generation
            << " canonical_sequence=" << record.sequence
            << " missing=" << (record.missing == Input::Video ? "video" : "audio")
            << " begin_steady_ns=" << record.begin << " end_steady_ns=" << record.end
            << " observed_wait_ns=" << record.end - record.begin
            << " held_dispatch_master_ns=" << record.dispatch
            << " begin_master_ns=" << record.beginMaster << " end_master_ns=" << record.endMaster
            << " dispatch_remaining_at_end_ns=" << record.dispatch - record.endMaster
            << " resumed_dispatch_master_ns=";
        if (record.resumedDispatch) out << *record.resumedDispatch; else out << "unavailable";
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {}
}
} // namespace media::ffmpeg::graph
