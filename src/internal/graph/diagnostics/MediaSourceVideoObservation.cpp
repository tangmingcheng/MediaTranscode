#include "internal/graph/diagnostics/MediaSourceVideoObservation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaVideoDisplayLineage.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {
constexpr std::array<const char*, static_cast<std::size_t>(MediaSourceVideoObservationStage::Count)> stages{
    "input", "accepted", "would_block", "resolved", "output_attempt", "published",
    "release", "release_committed", "activation", "release_published"};
template<class T> void append(std::ostream& out, const char* label, const std::optional<T>& value)
{ out << ' ' << label << '='; if (value) out << *value; else out << "unavailable"; }
}
std::optional<MediaSourceVideoObservationFacts> mediaSourceVideoObservationFacts(const MediaBufferRef& buffer) noexcept
{
    if (!buffer || buffer->streamKind() != MediaStreamKind::Video) return std::nullopt;
    const auto* frame = FFmpegFrameView::frame(buffer);
    const auto* packet = frame ? nullptr : FFmpegPacketView::packet(buffer);
    if (!frame && !packet) return std::nullopt;
    const auto pts = frame ? frame->pts : packet->pts;
    const auto time = buffer->timeDescriptor().timeBase;
    const auto actualFrameTime = frame ? std::optional{MediaRational{frame->time_base.num, frame->time_base.den}} : std::nullopt;
    if (const auto* source = dynamic_cast<const MediaVideoDecodeBuffer*>(buffer.get())) {
        const auto& au = source->lineage();
        if (!au) return std::nullopt;
        const auto display = FFmpegFrameView::displayLineage(buffer);
        return MediaSourceVideoObservationFacts{au->identity.sourceIdentity.view(), au->generation,
            au->identity.sourceSequence.value(), display ? display->presentation.nanoseconds() : au->presentation.nanoseconds(),
            !display && au->duration ? std::optional{au->duration->nanoseconds()} : std::nullopt,
            source->decodedSequence(), pts, time, actualFrameTime};
    }
    const auto canonical = frame ? FFmpegFrameView::canonicalLineage(buffer) : FFmpegPacketView::canonicalLineage(buffer);
    if (!canonical) return std::nullopt;
    const auto* identity = std::get_if<MediaSourceAccessUnitIdentity>(&canonical->identity);
    if (!identity) return std::nullopt;
    return MediaSourceVideoObservationFacts{identity->sourceIdentity.view(), canonical->generation,
        identity->sourceSequence.value(), canonical->presentation.nanoseconds(), canonical->duration.nanoseconds(),
        std::nullopt, pts, time, actualFrameTime};
}
void MediaSourceVideoObservation::observeRelease(MediaNodeId node, const char* nodeName,
    MediaSourceVideoObservationStage stage, const MediaAvStartupReleaseBuffer& release,
    std::optional<std::uint64_t> releaseIdentity) noexcept
{
    if (release.video().empty()) return;
    const auto first = mediaSourceVideoObservationFacts(release.video().front().media);
    if (!first) { unresolved(); return; }
    MediaSourceVideoObservationContext context;
    context.group = release.groupKey().value();
    context.releaseIdentity = releaseIdentity;
    context.transitionSequence = release.completedTransitionSequence();
    context.epochSourceStartNs = release.epoch().sourceStart.nanoseconds();
    context.epochMasterReleaseNs = release.epoch().masterRelease.nanoseconds();
    context.videoCount = release.video().size();
    const auto index = static_cast<std::size_t>(stage);
    if (first->generation != m_generation || (index < m_counts.size() && !m_counts[index])) {
        bool completeEnd = true;
        for (const auto& unit : release.video()) {
            const auto facts = mediaSourceVideoObservationFacts(unit.media);
            if (!facts || facts->generation != first->generation || facts->sourceIdentity != first->sourceIdentity) {
                m_incomplete = true; completeEnd = false; continue;
            }
            if (!context.minimumPresentationNs || facts->presentationNs < *context.minimumPresentationNs)
                context.minimumPresentationNs = facts->presentationNs;
            if (!facts->durationNs || *facts->durationNs < 0 ||
                facts->presentationNs > (std::numeric_limits<std::int64_t>::max)() - *facts->durationNs) {
                completeEnd = false; continue;
            }
            const auto end = facts->presentationNs + *facts->durationNs;
            if (!context.maximumEndNs || end > *context.maximumEndNs) context.maximumEndNs = end;
        }
        if (!completeEnd) context.maximumEndNs.reset();
    }
    observe(node, nodeName, stage, *first, context);
}
void MediaSourceVideoObservation::increment(std::uint64_t& count) noexcept
{ if (count == (std::numeric_limits<std::uint64_t>::max)()) m_incomplete = true; else ++count; }
void MediaSourceVideoObservation::unresolved() noexcept { increment(m_unknownLineage); }
void MediaSourceVideoObservation::droppedStale() noexcept { increment(m_stale); }
void MediaSourceVideoObservation::observe(MediaNodeId node, const char* nodeName,
    MediaSourceVideoObservationStage stage, const MediaBufferRef& buffer,
    const MediaSourceVideoObservationContext& context) noexcept
{
    if (auto facts = mediaSourceVideoObservationFacts(buffer)) observe(node, nodeName, stage, *facts, context);
}
void MediaSourceVideoObservation::observe(MediaNodeId node, const char* nodeName,
    MediaSourceVideoObservationStage stage, const MediaSourceVideoObservationFacts& facts,
    const MediaSourceVideoObservationContext& context) noexcept
{
    if (!facts.generation || (facts.sourceIdentity.empty() && !context.identityFromInput) || !facts.sourceSequence) return;
    if (facts.generation < m_generation) { increment(m_stale); return; }
    if (facts.generation != m_generation) {
        if (m_generation) summary(node, nodeName, "generation_change");
        m_generation = facts.generation;
        m_counts = {};
    }
    const auto index = static_cast<std::size_t>(stage);
    if (index >= m_counts.size()) { m_incomplete = true; return; }
    increment(m_counts[index]);
    if (m_counts[index] != 1 || !mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
    const auto steady = mediaSteadyClockNowNs();
    try {
        std::ostringstream out;
        out << "source_video_trace node_id=" << node.value << " node=" << nodeName << " stage=" << stages[index]
            << " source_identity=" << (facts.sourceIdentity.empty() ? std::string_view{"unavailable"} : facts.sourceIdentity)
            << " identity_link=" << (context.identityFromInput ? "node_generation_input_source_sequence" : "direct")
            << " generation=" << facts.generation
            << " source_sequence=" << facts.sourceSequence << " presentation_ns=" << facts.presentationNs;
        append(out, "duration_ns", facts.durationNs); append(out, "decoded_sequence", facts.decodedSequence);
        out << " pts=" << facts.pts << " descriptor_time_base=" << facts.timeBase.num << '/' << facts.timeBase.den
            << " frame_time_base=";
        if (facts.frameTimeBase) out << facts.frameTimeBase->num << '/' << facts.frameTimeBase->den;
        else out << "unavailable";
        out << " steady_ns=" << steady;
        if (!context.group.empty()) out << " group=" << context.group;
        append(out, "release_identity", context.releaseIdentity); append(out, "transition_sequence", context.transitionSequence);
        append(out, "epoch_source_start_ns", context.epochSourceStartNs); append(out, "epoch_master_release_ns", context.epochMasterReleaseNs);
        append(out, "video_count", context.videoCount); append(out, "min_presentation_ns", context.minimumPresentationNs);
        append(out, "max_end_ns", context.maximumEndNs); append(out, "preparation_phase", context.preparationPhase);
        for (std::size_t i = 0; i != m_counts.size(); ++i) out << ' ' << stages[i] << "_count=" << m_counts[i];
        out << " unknown_lineage_node_total=" << m_unknownLineage << " stale_node_total=" << m_stale << " incomplete=" << m_incomplete;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) { m_incomplete = true; }
}
void MediaSourceVideoObservation::summary(MediaNodeId node, const char* nodeName, const char* reason) const noexcept
{
    if ((!m_generation && !m_unknownLineage && !m_stale) || !mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
    try {
        std::ostringstream out;
        out << "source_video_trace_summary node_id=" << node.value << " node=" << nodeName
            << " generation=" << m_generation << " reason=" << reason << " steady_ns=" << mediaSteadyClockNowNs();
        for (std::size_t i = 0; i != m_counts.size(); ++i) out << ' ' << stages[i] << "_count=" << m_counts[i];
        out << " unknown_lineage_node_total=" << m_unknownLineage << " stale_node_total=" << m_stale << " incomplete=" << m_incomplete;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {}
}
void MediaSourceVideoObservation::finish(MediaNodeId node, const char* nodeName, const char* reason) noexcept
{ summary(node, nodeName, reason); reset(); }
} // namespace media::ffmpeg::graph
