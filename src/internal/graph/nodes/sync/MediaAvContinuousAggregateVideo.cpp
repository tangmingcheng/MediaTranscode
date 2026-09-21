#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/MediaFramePayloadFootprint.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaAvContinuousAggregateNode::buildVideo(
    MediaGraphExecutionContext& context)
{
    auto presentation = videoTime(m_nextVideoFrame);
    auto next = videoTime(m_nextVideoFrame + 1);
    if (!presentation || !next) return ::media::Result<MediaBufferRef>::failure(
        !presentation ? presentation.error() : next.error());
    auto duration = next.value().checkedSubtract(presentation.value());
    auto target = presentationMaster(presentation.value());
    if (!duration || !target) return ::media::Result<MediaBufferRef>::failure(
        !duration ? duration.error() : target.error());
    std::vector<const AVFrame*> tiles(m_sources.size(), nullptr);
    std::vector<MediaCanonicalVideoContribution> contributions;
    contributions.reserve(m_sources.size());
    std::vector<std::uint64_t> generations(m_sources.size());
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        contributions.push_back({i, plan().canvas.tiles[i], MediaCanonicalVideoGeneratedBlack{}});
        const auto snapshot = m_dependencies.sources[i].group->epochTransitionSnapshot();
        if (!snapshot.outputPermitted || !snapshot.playbackEpoch) continue;
        const auto& epoch = *snapshot.playbackEpoch;
        while (!source.video.empty()) {
            const auto lineage = FFmpegFrameView::canonicalLineage(source.video.front());
            if (lineage->generation != epoch.generation) {
                source.video.pop_front();
                continue;
            }
            auto relative = lineage->presentation.checkedSubtract(epoch.sourceStart);
            if (!relative) return ::media::Result<MediaBufferRef>::failure(relative.error());
            auto begin = epoch.masterRelease.checkedAdd(relative.value());
            if (!begin) return ::media::Result<MediaBufferRef>::failure(begin.error());
            auto end = begin.value().checkedAdd(lineage->duration);
            if (!end) return ::media::Result<MediaBufferRef>::failure(end.error());
            observeInputEnd(i, end.value());
            // A frame may cover multiple output ticks only within its declared
            // half-open interval. Never extend its lifetime across a real gap.
            if (end.value() <= target.value()) {
                source.video.pop_front();
                continue;
            }
            if (begin.value() <= target.value()) {
                tiles[i] = FFmpegFrameView::frame(source.video.front());
                generations[i] = lineage->generation;
                contributions.back().origin = MediaCanonicalSourceStamp{
                    std::get<MediaSourceAccessUnitIdentity>(lineage->identity),
                    lineage->generation, lineage->mappingConfidence,
                    lineage->presentation, lineage->duration};
            }
            break;
        }
    }
    auto reservation = context.reservePayload(nodeId(), MediaStreamKind::Video, MediaPayloadKind::Frame);
    if (!reservation) return ::media::Result<MediaBufferRef>::failure(reservation.error());
    auto frame = m_canvas.compose(tiles);
    if (!frame) return ::media::Result<MediaBufferRef>::failure(frame.error());
    frame.value()->pts = m_nextVideoFrame;
    frame.value()->duration = 1;
    frame.value()->time_base = {plan().videoFrameRate.den, plan().videoFrameRate.num};
    auto footprint = MediaFramePayloadFootprint::logicalBytes(*frame.value(), MediaStreamKind::Video);
    if (!footprint) return ::media::Result<MediaBufferRef>::failure(footprint.error());
    if (auto status = reservation.value().shrinkToActual(footprint.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto wrapped = FFmpegBufferFactory::wrapFrame(std::move(frame).value(), MediaStreamKind::Video);
    if (!wrapped) return wrapped;
    MediaTimeDescriptor time;
    time.timeBase = {plan().videoFrameRate.den, plan().videoFrameRate.num};
    wrapped.value()->setTimeDescriptor(time);
    if (auto status = reservation.value().attachTo(*wrapped.value()); !status)
        return ::media::Result<MediaBufferRef>::failure(status.error());
    auto lineage = createMediaCanonicalOutputLineage(presentation.value(), std::nullopt,
        duration.value(), MediaDecodeOrderMode::PresentationOrderNoReorder,
        {plan().outputGroupKey.value(), MediaScheduledStream::Video,
         MediaOutputAccessUnitSequence(static_cast<std::uint64_t>(m_nextVideoFrame) + 1)},
        MediaTimeMappingConfidence::Locked, plan().initialGeneration, std::move(contributions));
    if (!lineage) return ::media::Result<MediaBufferRef>::failure(lineage.error());
    auto output = MediaCanonicalVideoFrameBuffer::create(std::move(wrapped).value(), std::move(lineage).value());
    if (!output) return output;
    m_pending = Pending{PendingKind::Video, output.value(), std::move(generations)};
    if (auto status = validateMetadataBound(); !status) {
        m_pending.reset();
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    return output;
}

} // namespace media::ffmpeg::graph
