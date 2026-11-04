#include "internal/graph/runtime/storage/MediaAvAggregateMetadataBudget.h"
#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"
#include "internal/graph/sync/MediaAvAggregateTimeline.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaAvContinuousAggregateNode::buildVideo(
    MediaGraphExecutionContext& context)
{
    auto presentation = MediaAvAggregateTimeline::videoPresentation(plan(), m_nextVideoFrame);
    auto next = MediaAvAggregateTimeline::videoPresentation(plan(), m_nextVideoFrame + 1);
    if (!presentation || !next) return ::media::Result<MediaBufferRef>::failure(
        !presentation ? presentation.error() : next.error());
    auto duration = next.value().checkedSubtract(presentation.value());
    auto target = MediaAvAggregateTimeline::presentationMaster(*m_epoch, presentation.value());
    if (!duration || !target) return ::media::Result<MediaBufferRef>::failure(
        !duration ? duration.error() : target.error());
    MediaFixedCapacityStorage<MediaVideoCanvasTile> tiles;
    tiles.resize(m_sources.size());
    MediaFixedCapacityStorage<MediaCanonicalVideoContribution> contributions(m_sources.size());
    MediaFixedCapacityStorage<std::uint64_t> generations;
    generations.resize(m_sources.size());
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        contributions.push_back({i, plan().canvas.tiles[i], MediaCanonicalVideoGeneratedBlack{}});
        const auto snapshot = m_dependencies.sources[i].group->epochTransitionSnapshot();
        if (!snapshot.outputPermitted || !snapshot.playbackEpoch()) continue;
        const auto& epoch = *snapshot.playbackEpoch();
        if (plan().sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint) {
            while (!source.video.empty()) {
                const auto* current = dynamic_cast<const MediaVideoDecodeBuffer*>(source.video.front().get());
                const auto display = current ? current->displayLineage() : nullptr;
                if (!display || !display->accessUnit)
                    return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("Missing aggregate display evidence"));
                if (display->accessUnit->generation != epoch.generation) { source.video.pop_front(); continue; }
                auto relative = display->presentation.checkedSubtract(epoch.sourceStart);
                if (!relative) return ::media::Result<MediaBufferRef>::failure(relative.error());
                auto begin = epoch.masterRelease.checkedAdd(relative.value());
                if (!begin) return ::media::Result<MediaBufferRef>::failure(begin.error());
                if (begin.value() > target.value()) break;
                if (source.video.size() > 1) {
                    const auto* nextFrame = dynamic_cast<const MediaVideoDecodeBuffer*>(source.video[1].get());
                    const auto following = nextFrame ? nextFrame->displayLineage() : nullptr;
                    if (!following || !following->accessUnit || following->accessUnit->generation != epoch.generation)
                        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument("Aggregate display queue crosses generations"));
                    auto nextRelative = following->presentation.checkedSubtract(epoch.sourceStart);
                    if (!nextRelative) return ::media::Result<MediaBufferRef>::failure(nextRelative.error());
                    auto nextBegin = epoch.masterRelease.checkedAdd(nextRelative.value());
                    if (!nextBegin) return ::media::Result<MediaBufferRef>::failure(nextBegin.error());
                    if (nextBegin.value() <= target.value()) { source.video.pop_front(); continue; }
                }
                tiles[i] = {FFmpegFrameView::frame(source.video.front()),
                    FFmpegFrameView::readiness(source.video.front())};
                generations[i] = epoch.generation;
                contributions.back().origin = MediaVideoDisplaySourceStamp{display->accessUnit->identity,
                    display->decodedSequence, epoch.generation, display->accessUnit->mappingConfidence, display->presentation};
                break;
            }
            continue;
        }
        const auto candidatesBefore = source.video.size();
        std::size_t expiredCandidates = 0;
        std::optional<std::int64_t> examinedBegin;
        std::optional<std::int64_t> examinedEnd;
        while (!source.video.empty()) {
            const auto lineage = FFmpegFrameView::canonicalLineage(source.video.front());
            if (lineage->generation != epoch.generation) {
                source.video.pop_front();
                continue;
            }
            auto interval = MediaAvAggregateTimeline::sourceInterval(
                epoch, lineage->presentation, lineage->duration);
            if (!interval) return ::media::Result<MediaBufferRef>::failure(interval.error());
            examinedBegin = interval.value().begin.nanoseconds();
            examinedEnd = interval.value().end.nanoseconds();
            // A frame may cover multiple output ticks only within its declared
            // half-open interval. Never extend its lifetime across a real gap.
            if (interval.value().end <= target.value()) {
                ++expiredCandidates;
                source.video.pop_front();
                continue;
            }
            if (interval.value().begin <= target.value()) {
                tiles[i] = {FFmpegFrameView::frame(source.video.front()),
                    FFmpegFrameView::readiness(source.video.front())};
                generations[i] = lineage->generation;
                contributions.back().origin = MediaCanonicalSourceStamp{
                    std::get<MediaSourceAccessUnitIdentity>(lineage->identity),
                    lineage->generation, lineage->mappingConfidence,
                    lineage->presentation, lineage->duration};
            }
            break;
        }
        if (!generations[i] && source.committedRealFrames && !source.firstIntervalMiss &&
            mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
            const auto* channel = context.findInputChannel(nodeId(), plan().sources[i].videoPort);
            source.firstIntervalMiss = MediaAggregateIntervalMissObservation{
                m_nextVideoFrame, target.value().nanoseconds(), mediaSteadyClockNowNs(),
                epoch.generation, candidatesBefore, source.video.size(), expiredCandidates,
                channel ? std::optional<std::size_t>(channel->size()) : std::nullopt,
                examinedBegin, examinedEnd};
            source.firstIntervalMiss->epochSourceStartNs = epoch.sourceStart.nanoseconds();
            source.firstIntervalMiss->epochMasterReleaseNs = epoch.masterRelease.nanoseconds();
            source.generationObservation.enter(epoch.generation);
            source.firstIntervalMiss->generationAtMiss = source.generationObservation;
            source.firstIntervalMiss->firstRealCursor = source.generationObservation.firstRealCursor;
            source.firstIntervalMiss->firstRealSteadyNs = source.generationObservation.firstRealSteadyNs;
            source.firstIntervalMiss->firstCandidateSteadyNs = source.generationObservation.firstCandidateSteadyNs;
            source.firstIntervalMiss->firstCandidateBeginNs = source.generationObservation.firstCandidateBeginNs;
            source.firstIntervalMiss->firstCandidateEndNs = source.generationObservation.firstCandidateEndNs;
            source.firstIntervalMiss->firstCandidateFrame = source.generationObservation.firstCandidateFrame;
            source.firstIntervalMiss->firstRealFrame = source.generationObservation.firstRealFrame;
        }
    }
    auto reservation = context.reservePayload(nodeId(), MediaStreamKind::Video, MediaPayloadKind::Frame);
    if (!reservation) return ::media::Result<MediaBufferRef>::failure(reservation.error());
    auto frame = m_executionObservation.measure(ExecutionStage::Canvas,
        {m_nextVideoFrame, std::nullopt}, [&] { return m_canvas->compose(tiles.view()); });
    if (!frame) return ::media::Result<MediaBufferRef>::failure(frame.error());
    frame.value()->pts = m_nextVideoFrame;
    frame.value()->duration = 1;
    frame.value()->time_base = {plan().videoFrameRate.den, plan().videoFrameRate.num};
    auto footprint = MediaFramePayloadFootprint::logicalBytes(*frame.value(), MediaStreamKind::Video);
    if (!footprint) return ::media::Result<MediaBufferRef>::failure(footprint.error());
    if (auto status = reservation.value().shrinkToActual(footprint.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(frame).value(), MediaStreamKind::Video, MediaRuntimeLineageAllocation{});
    if (!wrapped) return wrapped;
    MediaTimeDescriptor time;
    time.timeBase = {plan().videoFrameRate.den, plan().videoFrameRate.num};
    wrapped.value()->setTimeDescriptor(time);
    if (auto status = reservation.value().attachTo(*wrapped.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto lineage = createMediaCanonicalOutputLineage(presentation.value(), std::nullopt,
        duration.value(), MediaDecodeOrderMode::PresentationOrderNoReorder,
        plan().outputGroupKey.value(), MediaScheduledStream::Video,
         MediaOutputAccessUnitSequence(static_cast<std::uint64_t>(m_nextVideoFrame) + 1),
        MediaTimeMappingConfidence::Locked, plan().initialGeneration, contributions.view());
    if (!lineage) return ::media::Result<MediaBufferRef>::failure(lineage.error());
    auto output = MediaCanonicalVideoFrameBuffer::create(std::move(wrapped).value(), std::move(lineage).value(), MediaVideoPresentationProgress{next.value()});
    if (!output) return output;
    m_pending = Pending{PendingKind::Video, output.value(), std::move(generations)};
    if (auto status = MediaAvAggregateMetadataBudget::validate(plan(), m_sources.view(), {}, m_pending ? m_pending->buffer : MediaBufferRef{}); !status) {
        m_pending.reset();
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    return output;
}

} // namespace media::ffmpeg::graph
