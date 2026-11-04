#include "internal/graph/sync/MediaAvAggregateCoordination.h"
#include "internal/graph/runtime/storage/MediaAvAggregateMetadataBudget.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"
#include "internal/graph/sync/MediaAvAggregateTimeline.h"

#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/MediaPreparedVideoCanvas.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/MediaAudioSampleGrid.h"

#include <algorithm>
#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* text)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(text));
}
void appendObservedFrame(std::ostream& out, const std::string& prefix,
    const std::optional<MediaAggregateFrameObservation>& frame)
{
    out << ' ' << prefix << "source_sequence="
        << (frame ? std::to_string(frame->sourceSequence) : "unavailable")
        << ' ' << prefix << "presentation_ns="
        << (frame ? std::to_string(frame->presentationNs) : "unavailable")
        << ' ' << prefix << "duration_ns="
        << (frame && frame->durationNs ? std::to_string(*frame->durationNs) : "unavailable")
        << ' ' << prefix << "decoded_sequence="
        << (frame && frame->decodedSequence ? std::to_string(*frame->decodedSequence) : "unavailable");
}
void logFirstSourceFrame(MediaNodeId node, const std::string& identity,
    std::uint64_t generation, const char* stage, std::int64_t steady,
    const std::optional<MediaAggregateFrameObservation>& frame) noexcept
{
    if (!frame) return;
    try {
        std::ostringstream out;
        out << "source_video_trace node_id=" << node.value
            << " node=MediaAvContinuousAggregateNode stage=" << stage
            << " source_identity=" << identity << " generation=" << generation
            << " steady_ns=" << steady;
        appendObservedFrame(out, "", frame);
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {
        // Observation must not alter contribution selection or publication.
    }
}
}

MediaAvContinuousAggregateNode::MediaAvContinuousAggregateNode(
    MediaNodeId node, MediaAvAggregateRuntimeDependencies dependencies)
    : FFmpegNodeRuntime(node, staticKind(), "MediaAvContinuousAggregateNode"),
      m_dependencies(std::move(dependencies))
{
    if (m_dependencies.aggregatePlan) {
        m_sources.resize(plan().sources.size());
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            m_sources[i].purge = std::make_shared<MediaOwnerThreadGenerationPurge>();
            m_sources[i].video = MediaFixedCapacityStorage<MediaBufferRef>(plan().sources[i].maximumVideoCandidates);
        }
    }
}

const MediaAvContinuousAggregatePlan& MediaAvContinuousAggregateNode::plan() const noexcept
{
    return *m_dependencies.aggregatePlan;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaAvContinuousAggregateNode::sourcePurgeTarget(const MediaAvSyncGroupKey& group) const noexcept
{
    for (std::size_t i = 0; i < m_sources.size(); ++i)
        if (plan().sources[i].groupKey == group) return m_sources[i].purge;
    return nullptr;
}

::media::Status MediaAvContinuousAggregateNode::start(MediaGraphExecutionContext& context)
{
    if (!m_dependencies.aggregatePlan || !m_dependencies.output || !m_dependencies.preparedCanvas || !m_dependencies.coordination ||
        m_dependencies.output->key() != plan().outputGroupKey ||
        m_sources.size() != plan().sources.size() || m_sources.size() != m_dependencies.sources.size())
        return invalid("Continuous aggregate requires its runtime dependencies");
    if (auto status = MediaAvContinuousAggregatePlanValidator::validate(plan()); !status) return status;
    m_summaryPublished = false;
    m_executionObservation = {};
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        if (!m_dependencies.sources[i].group ||
            m_dependencies.sources[i].group->key() != plan().sources[i].groupKey ||
            m_dependencies.sources[i].group->clock() != m_dependencies.output->clock() ||
            m_dependencies.sources[i].group->plan().startup.videoReadiness !=
                (plan().sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint
                    ? MediaVideoStartupReadiness::DecodedPresentationSpan : MediaVideoStartupReadiness::IntervalCoverage))
            return invalid("Continuous aggregate source clock differs from its plan");
    }
    for (auto& source : m_sources) {
        if (auto status = source.purge->start(context.sharedNodeWakeup(nodeId())); !status) {
            clear();
            return status;
        }
    }
    if (auto status = m_dependencies.coordination->bindWakeup(MediaStreamKind::Video,
        context.sharedNodeWakeup(nodeId())); !status) { clear(); return status; }
    auto status = FFmpegNodeRuntime::start(context);
    if (!status) clear();
    return status;
}

void MediaAvContinuousAggregateNode::clear() noexcept
{
    if (m_dependencies.coordination) m_dependencies.coordination->stop();
    for (auto& source : m_sources) {
        source.purge->stop();
        source.video.clear();
        source.displaySealed = false;
        source.lastDisplaySequence = 0;
        source.lastDisplayPresentation.reset();
        source.lastDisplayCommit.reset();
        source.minimumGeneration = 0;
        source.committedRealFrames = 0;
        source.committedBlackFrames = 0;
        source.committedGeneration = 0;
        source.firstIntervalMiss.reset();
        source.generationObservation = {};
        source.videoEnded = false;
        source.epochEnded = false;
        source.discardedAudioEnded = false;
        source.prepared = false;
        source.lastInputEnd.reset();
    }
    m_pending.reset();
    m_videoCodec.reset();
    m_canvas.reset();
    m_canvasPrepared = false;
    m_epoch.reset();
    m_nextVideoFrame = 0;
    m_videoEndPublished = false;
    m_rejectedPending = false;
}

::media::Status MediaAvContinuousAggregateNode::stop(MediaGraphExecutionContext& context)
{
    logSummary("stop");
    auto status = FFmpegNodeRuntime::stop(context);
    clear();
    return status;
}

void MediaAvContinuousAggregateNode::abort(MediaGraphExecutionContext& context) noexcept
{
    logSummary("abort");
    FFmpegNodeRuntime::abort(context);
    clear();
}

::media::Status MediaAvContinuousAggregateNode::servicePurges(MediaGraphExecutionContext& context)
{
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        const auto request = source.purge->pending();
        if (!request) continue;
        if (m_pending && m_pending->sourceGenerations[i] != 0 &&
            m_pending->sourceGenerations[i] <= request->oldGeneration) {
            cancelPendingOutputTransfer();
            m_pending.reset();
        }
        source.video.clear();
        source.displaySealed = false;
        source.lastDisplaySequence = 0;
        source.lastDisplayPresentation.reset();
        source.lastDisplayCommit.reset();
        source.minimumGeneration = request->nextGeneration;
        source.generationObservation = {};
        source.generationObservation.enter(request->nextGeneration);
        if (plan().sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint) {
            // An unsealed EOF belongs to the purged generation, not its successor.
            source.videoEnded = false;
        }
        source.prepared = false;
        source.lastInputEnd.reset();
        // No new generation can publish before this source barrier acknowledges.
        auto* video = context.findInputChannel(nodeId(), plan().sources[i].videoPort);
        if (!video) return invalid("Aggregate purge lost its source input");
        video->clear();
        if (plan().sources[i].discardedAudioPort) {
            auto* discarded = context.findInputChannel(nodeId(), *plan().sources[i].discardedAudioPort);
            if (!discarded) return invalid("Aggregate purge lost its discarded audio input");
            discarded->clear();
        }
        if (auto status = m_dependencies.coordination->purgeInput(i, MediaStreamKind::Video,
            request->oldGeneration, request->nextGeneration); !status) return status;
        if (plan().sources[i].discardedAudioPort)
            if (auto status = m_dependencies.coordination->purgeInput(i, MediaStreamKind::Audio,
                request->oldGeneration, request->nextGeneration); !status) return status;
        if (auto status = source.purge->complete(*request, ::media::Status::success()); !status)
            return status;
    }
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaAvContinuousAggregateNode::process(MediaGraphExecutionContext& context)
{
    return m_executionObservation.process({m_nextVideoFrame, std::nullopt},
        pendingOutputBufferCount() != 0, [&] {
            auto status = m_executionObservation.measure(ExecutionStage::Purge,
                {m_nextVideoFrame, std::nullopt}, [&] { return servicePurges(context); });
            if (!status) return ::media::Result<MediaNodeProcessResult>::failure(status.error());
            return FFmpegNodeRuntime::process(context);
        });
}

::media::Status MediaAvContinuousAggregateNode::bindCodecs(MediaGraphExecutionContext& context)
{
    for (auto [port, retained] : {std::pair{"video_codec", &m_videoCodec}}) {
        if (*retained) continue;
        auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), port), name(), port);
        if (!input) return ::media::Status::failure(input.error());
        if (!input.value()) continue;
        const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(input.value()->get());
        if (!codec || !codec->context()) return invalid("Aggregate requires prepared encoder contexts");
        *retained = std::move(*input.value());
    }
    if (m_videoCodec && !m_canvasPrepared) {
        const auto* codec = static_cast<const FFmpegCodecContextBuffer*>(m_videoCodec.get())->context();
        const auto& range = plan().canvas.effectiveColorRange;
        if (!range.encoderInput || !range.color ||
            codec->color_primaries != range.color->primaries || codec->color_trc != range.color->transfer ||
            codec->colorspace != range.color->matrix ||
            codec->sample_aspect_ratio.num != plan().canvas.sampleAspectRatio.num ||
            codec->sample_aspect_ratio.den != plan().canvas.sampleAspectRatio.den || codec->width != plan().canvas.width ||
            codec->height != plan().canvas.height || !codec->hw_frames_ctx ||
            codec->color_range != range.encoderInput->colorRange ||
            codec->pix_fmt != range.encoderInput->pixelFormat ||
            codec->sw_pix_fmt != range.encoderInput->surfacePixelFormat)
            return invalid("Canvas facts differ from the production encoder readback");
        if (!m_dependencies.preparedCanvas) return invalid("Aggregate requires its prepared canvas resources");
        auto canvas = m_dependencies.preparedCanvas->claim({plan().canvas, plan().canvasStorage}, codec->hw_frames_ctx,
            context.sharedNodeWakeup(nodeId()));
        if (!canvas) return ::media::Status::failure(canvas.error());
        m_canvas = std::move(canvas.value());
        m_canvasPrepared = true;
    }
    return ::media::Status::success();
}

::media::Status MediaAvContinuousAggregateNode::consumeInputs(
    MediaGraphExecutionContext& context, CandidatePreparation& preparation)
{
    preparation = CandidatePreparation::Ready;
    std::optional<MediaRunningTime> target;
    if (m_epoch && !m_pending) {
        auto presentation = MediaAvAggregateTimeline::videoPresentation(plan(), m_nextVideoFrame);
        if (!presentation) return ::media::Status::failure(presentation.error());
        auto mapped = MediaAvAggregateTimeline::presentationMaster(*m_epoch, presentation.value());
        if (!mapped) return ::media::Status::failure(mapped.error());
        target = mapped.value();
    }
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto& source = m_sources[i];
        const auto epochPort = "source_epoch_" + std::to_string(i);
        auto event = source.epochEnded
            ? ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt)
            : tryReadRequiredInput(context.findInputChannel(nodeId(), epochPort), name(), epochPort);
        if (!event) return ::media::Status::failure(event.error());
        if (event.value()) {
            const auto* activated = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(event.value()->get());
            if (activated) {
                if (activated->groupKey() != plan().sources[i].groupKey)
                    return invalid("Aggregate received an epoch for another source");
            } else if ((*event.value())->isEof()) source.epochEnded = true;
            else return invalid("Aggregate source epoch input is not an activation or trusted end");
        }
        if (plan().sources[i].discardedAudioPort && !source.discardedAudioEnded) {
            for (std::size_t consumed = 0; consumed < plan().maximumAudioCandidates; ++consumed) {
                auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), *plan().sources[i].discardedAudioPort),
                    name(), *plan().sources[i].discardedAudioPort);
                if (!input) return ::media::Status::failure(input.error());
                if (!input.value()) break;
                if ((*input.value())->isEof()) { source.discardedAudioEnded = true;
                    if (auto status = m_dependencies.coordination->sealInput(i, MediaStreamKind::Audio); !status) return status;
                    break; }
                if ((*input.value())->isFlush()) continue;
                const auto* audio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(input.value()->get());
                if (!audio || !audio->media() || !audio->media()->lineage() ||
                    !std::holds_alternative<MediaSourceAccessUnitIdentity>(audio->media()->lineage()->identity))
                    return invalid("Aggregate discarded audio requires bound source samples");
                const auto& origin = audio->audioOrigin();
                const auto endSample = audio->media()->interval().end;
                if (endSample < origin.epochOutputSampleIndex)
                    return invalid("Discarded audio end precedes its source epoch");
                auto relative = MediaRunningTime::checkedFromTicks(endSample - origin.epochOutputSampleIndex,
                    1, origin.outputSampleRate);
                if (!relative) return ::media::Status::failure(relative.error());
                auto end = origin.masterRelease.checkedAdd(relative.value());
                if (!end) return ::media::Status::failure(end.error());
                if (auto status = m_dependencies.coordination->observeInputEnd(i, MediaStreamKind::Audio,
                    origin.generation, end.value()); !status) return status;
                // This declared non-selected stream has no output contribution.
                // Releasing its sample lease still drives its source A/V owner.
            }
        }
        const bool displayProduct = plan().sources[i].videoProduct == MediaVideoFrameLineageProduct::SourceDisplayPoint;
        if (!displayProduct && target) {
            const auto snapshot = m_dependencies.sources[i].group->epochTransitionSnapshot();
            if (const auto epoch = snapshot.playbackEpoch(); snapshot.outputPermitted && epoch) {
                source.generationObservation.enter(epoch->generation);
                while (!source.video.empty()) {
                    const auto lineage = FFmpegFrameView::canonicalLineage(source.video.front());
                    if (lineage->generation != epoch->generation) {
                        if (lineage->generation < epoch->generation)
                            source.generationObservation.count(source.generationObservation.prepareOldGeneration);
                        source.video.pop_front();
                        continue;
                    }
                    auto interval = MediaAvAggregateTimeline::sourceInterval(
                        *epoch, lineage->presentation, lineage->duration);
                    if (!interval) return ::media::Status::failure(interval.error());
                    source.generationObservation.lastPrepareBeginNs = interval.value().begin.nanoseconds();
                    source.generationObservation.lastPrepareEndNs = interval.value().end.nanoseconds();
                    if (interval.value().end > *target) break;
                    source.generationObservation.count(source.generationObservation.prepareExpired);
                    source.video.pop_front();
                }
            }
        }
        bool observedEmpty = false;
        std::size_t consumed = 0;
        const auto capacity = plan().sources[i].maximumVideoCandidates;
        for (; consumed < capacity && !source.videoEnded && source.video.size() < capacity; ++consumed) {
            auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), plan().sources[i].videoPort),
                name(), plan().sources[i].videoPort);
            if (!input) return ::media::Status::failure(input.error());
            if (!input.value()) { observedEmpty = true; break; }
            if ((*input.value())->isEof()) { source.videoEnded = true;
                if (!displayProduct)
                    if (auto status = m_dependencies.coordination->sealInput(i, MediaStreamKind::Video); !status) return status;
                break; }
            if ((*input.value())->isFlush()) continue;
            if (displayProduct) {
                const auto* decoded = dynamic_cast<const MediaVideoDecodeBuffer*>(input.value()->get());
                const auto display = decoded ? decoded->displayLineage() : nullptr;
                if (!display || !display->accessUnit || !FFmpegFrameView::frame(*input.value()) ||
                    display->accessUnit->identity.sourceIdentity.view() != plan().sources[i].videoIdentity ||
                    !display->decodedSequence || display->accessUnit->mappingConfidence != MediaTimeMappingConfidence::Locked)
                    return invalid("Aggregate requires a real decoded display point for its planned source");
                const auto generation = display->accessUnit->generation;
                if (generation < source.minimumGeneration) continue;
                auto epoch = m_dependencies.sources[i].group->playbackEpoch();
                if (!epoch) return ::media::Status::failure(epoch.error());
                if (generation != epoch.value().generation) continue;
                if (display->decodedSequence <= source.lastDisplaySequence ||
                    (source.lastDisplayPresentation && display->presentation < *source.lastDisplayPresentation))
                    return invalid("Aggregate display points must advance decoded sequence in presentation order");
                source.lastDisplaySequence = display->decodedSequence;
                source.lastDisplayPresentation = display->presentation;
                source.video.push_back(std::move(*input.value()));
                if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
                    source.generationObservation.enter(generation);
                    if (!source.generationObservation.firstCandidateSteadyNs) {
                        source.generationObservation.firstCandidateSteadyNs = mediaSteadyClockNowNs();
                        source.generationObservation.firstCandidateFrame = MediaAggregateFrameObservation{
                            display->accessUnit->identity.sourceSequence.value(), display->presentation.nanoseconds(),
                            std::nullopt, display->decodedSequence};
                        logFirstSourceFrame(nodeId(), plan().sources[i].videoIdentity, generation,
                            "aggregate_candidate", *source.generationObservation.firstCandidateSteadyNs,
                            source.generationObservation.firstCandidateFrame);
                    }
                }
                source.prepared = true;
                continue;
            }
            auto lineage = FFmpegFrameView::canonicalLineage(*input.value());
            if (!lineage || !std::holds_alternative<MediaSourceAccessUnitIdentity>(lineage->identity) ||
                !FFmpegFrameView::frame(*input.value()) || lineage->duration.nanoseconds() <= 0)
                return invalid("Aggregate requires source video lineage with an explicit interval");
            if (std::get<MediaSourceAccessUnitIdentity>(lineage->identity).sourceIdentity.view() !=
                    plan().sources[i].videoIdentity || !lineage->videoContributions.empty())
                return invalid("Aggregate source video metadata differs from prepared identity");
            if (lineage->generation < source.minimumGeneration) {
                source.generationObservation.enter(source.minimumGeneration);
                source.generationObservation.count(source.generationObservation.prepareOldGeneration);
                continue;
            }
            if (auto valid = validateMediaCanonicalLineage(*lineage); !valid) return valid;
            if (!source.video.empty()) {
                const auto previous = FFmpegFrameView::canonicalLineage(source.video.back());
                if (previous->generation == lineage->generation && previous->presentation > lineage->presentation)
                    return invalid("Aggregate video candidates are not in presentation order");
            }
            auto epoch = m_dependencies.sources[i].group->playbackEpoch();
            if (!epoch) return ::media::Status::failure(epoch.error());
            source.generationObservation.enter(epoch.value().generation);
            if (epoch.value().generation != lineage->generation) {
                if (lineage->generation < epoch.value().generation)
                    source.generationObservation.count(source.generationObservation.prepareOldGeneration);
                continue;
            }
            auto interval = MediaAvAggregateTimeline::sourceInterval(
                epoch.value(), lineage->presentation, lineage->duration);
            if (!interval) return ::media::Status::failure(interval.error());
            if (auto status = m_dependencies.coordination->observeInputEnd(i, MediaStreamKind::Video,
                lineage->generation, interval.value().end); !status) return status;
            source.generationObservation.lastPrepareBeginNs = interval.value().begin.nanoseconds();
            source.generationObservation.lastPrepareEndNs = interval.value().end.nanoseconds();
            if (target && interval.value().end <= *target) {
                source.generationObservation.count(source.generationObservation.prepareExpired);
                continue;
            }
            source.video.push_back(std::move(*input.value()));
            if (!source.generationObservation.firstCandidateSteadyNs &&
                mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
                source.generationObservation.firstCandidateSteadyNs = mediaSteadyClockNowNs();
                source.generationObservation.firstCandidateBeginNs = interval.value().begin.nanoseconds();
                source.generationObservation.firstCandidateEndNs = interval.value().end.nanoseconds();
                source.generationObservation.firstCandidateFrame = MediaAggregateFrameObservation{
                    std::get<MediaSourceAccessUnitIdentity>(lineage->identity).sourceSequence.value(),
                    lineage->presentation.nanoseconds(), lineage->duration.nanoseconds(), std::nullopt};
                logFirstSourceFrame(nodeId(), plan().sources[i].videoIdentity, lineage->generation,
                    "aggregate_candidate", *source.generationObservation.firstCandidateSteadyNs,
                    source.generationObservation.firstCandidateFrame);
                if (source.firstIntervalMiss && source.firstIntervalMiss->generation == lineage->generation &&
                    !source.firstIntervalMiss->firstCandidateSteadyNs) {
                    source.firstIntervalMiss->firstCandidateSteadyNs = source.generationObservation.firstCandidateSteadyNs;
                    source.firstIntervalMiss->firstCandidateBeginNs = source.generationObservation.firstCandidateBeginNs;
                    source.firstIntervalMiss->firstCandidateEndNs = source.generationObservation.firstCandidateEndNs;
                    source.firstIntervalMiss->firstCandidateFrame = source.generationObservation.firstCandidateFrame;
                }
            }
            source.prepared = true;
        }
        if (!displayProduct && target && source.video.empty() && !source.videoEnded &&
            !observedEmpty && consumed == capacity)
            preparation = CandidatePreparation::RetryProgress;
        if (displayProduct && source.videoEnded && !source.displaySealed) {
            if (source.video.size() == 1 && source.lastDisplayCommit) {
                const auto* last = dynamic_cast<const MediaVideoDecodeBuffer*>(source.video.front().get());
                if (last && last->decodedSequence() == source.lastDisplayCommit->decodedSequence)
                    source.video.pop_front();
            }
            if (source.video.empty()) {
                auto epoch = m_dependencies.sources[i].group->playbackEpoch();
                if (!epoch) return ::media::Status::failure(epoch.error());
                if (auto status = m_dependencies.coordination->sealDisplayInput(i, epoch.value().generation,
                        source.lastDisplayCommit); !status) return status;
                source.displaySealed = true;
            }
        }
    }
    return MediaAvAggregateMetadataBudget::validate(plan(), m_sources.view(), {}, m_pending ? m_pending->buffer : MediaBufferRef{});
}

::media::Result<bool> MediaAvContinuousAggregateNode::activate(MediaGraphExecutionContext& context)
{
    const auto preparation = m_dependencies.coordination->snapshot();
    if (preparation.allInputsSealed && (!preparation.finalOutputEnd || !preparation.audioReady ||
        std::any_of(m_sources.begin(), m_sources.end(), [](const auto& source) { return !source.prepared; })))
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Composition sources ended before mandatory preparation completed"));
    if (!m_canvasPrepared || !preparation.audioReady)
        return ::media::Result<bool>::success(false);
    auto now = m_dependencies.output->clock()->now();
    if (!now) return ::media::Result<bool>::failure(now.error());
    auto release = now.value().checkedAdd(plan().preparationLead);
    if (!release) return ::media::Result<bool>::failure(release.error());
    MediaFixedCapacityStorage<std::uint64_t> generations(m_sources.size());
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        if (!m_sources[i].prepared || m_sources[i].video.empty() ||
            m_dependencies.sources[i].group->lifecycleState() != MediaAvSyncGroupRuntime::LifecycleState::Active)
            return ::media::Result<bool>::success(false);
        auto epoch = m_dependencies.sources[i].group->playbackEpoch();
        if (!epoch) return ::media::Result<bool>::failure(epoch.error());
        if (i == plan().audioSource && preparation.audioReady->origin.generation != epoch.value().generation)
            return ::media::Result<bool>::success(false);
        generations.push_back(epoch.value().generation);
        if (epoch.value().masterRelease > release.value()) release =
            ::media::Result<MediaRunningTime>::success(epoch.value().masterRelease);
    }
    const MediaPlaybackEpoch epoch{MediaRunningTime::fromNanoseconds(0), release.value(), plan().initialGeneration};
    const MediaAudioPlaybackOrigin origin{epoch.generation, epoch.sourceStart,
        epoch.masterRelease, plan().initialAudioSample, plan().audio.sampleRate()};
    if (!m_dependencies.output || !m_dependencies.output->plan().members)
        return ::media::Result<bool>::failure(::media::ErrorInfo::notInitialized(
            "Aggregate activation requires planned output members"));
    auto activation = MediaPlaybackActivation::create(
        *m_dependencies.output->plan().members, epoch, origin);
    if (!activation) return ::media::Result<bool>::failure(activation.error());
    auto event = MediaPlaybackEpochActivatedBuffer::create(
        plan().outputGroupKey, activation.value(), std::nullopt);
    if (!event) return ::media::Result<bool>::failure(event.error());
    const auto* port = context.graph()->findOutputPort(nodeId(), "activated");
    if (!port) return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument("Aggregate activation output is missing"));
    std::vector<MediaAtomicOutputBatch> batches;
    for (auto* channel : context.outputChannels(nodeId()))
        if (channel && channel->binding().from.portId == port->id)
            batches.push_back({channel, std::span(&event.value(), 1)});
    if (batches.empty()) return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument("Aggregate activation has no consumers"));
    MediaFixedCapacityStorage<MediaAvOutputPermitCommitReservation> sourcePermits(m_sources.size());
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        auto permit = m_dependencies.sources[i].group->reserveOutputCommit(generations[i]);
        if (!permit) {
            if (permit.error().code == ::media::ErrorCode::Cancelled) return ::media::Result<bool>::success(false);
            return ::media::Result<bool>::failure(permit.error());
        }
        sourcePermits.push_back(std::move(permit).value());
    }
    auto publication = MediaAtomicOutputTransaction::acquire("Continuous output activation", batches);
    if (!publication) return ::media::Result<bool>::failure(publication.error());
    if (!publication.value()) return ::media::Result<bool>::success(false);
    if (auto status = m_dependencies.activation.activateInitial(activation.value()); !status)
        return ::media::Result<bool>::failure(status.error());
    publication.value()->commitReserved();
    m_epoch = epoch;
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaOutputCommitReservation>
MediaAvContinuousAggregateNode::reserveOutputCommit(const MediaBufferRef&) const
{
    MediaFixedCapacityStorage<MediaAvOutputPermitCommitReservation> permits(m_sources.size() + 1);
    if (m_pending) {
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            if (m_pending->sourceGenerations[i] == 0) continue;
            auto sourcePermit = m_dependencies.sources[i].group->reserveOutputCommit(
                m_pending->sourceGenerations[i]);
            if (!sourcePermit) {
                m_rejectedPending = true;
                return ::media::Result<MediaOutputCommitReservation>::failure(sourcePermit.error());
            }
            permits.push_back(std::move(sourcePermit).value());
        }
    }
    auto permit = m_dependencies.output->reserveOutputCommit(plan().initialGeneration);
    if (!permit) return ::media::Result<MediaOutputCommitReservation>::failure(permit.error());
    permits.push_back(std::move(permit).value());
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(std::move(permits)));
}

::media::Status MediaAvContinuousAggregateNode::commitReservedOutput(const MediaBufferRef& buffer)
{
    if (!m_pending || m_pending->buffer != buffer) return invalid("Aggregate output commit has no matching transaction");
    switch (m_pending->kind) {
    case PendingKind::Video: {
        const auto lineage = FFmpegFrameView::canonicalLineage(buffer);
        if (!lineage || !m_epoch || lineage->videoContributions.size() != m_sources.size())
            return invalid("Aggregate commit requires its output contribution evidence");
        auto relativeEnd = lineage->presentation.checkedAdd(lineage->duration);
        if (!relativeEnd) return ::media::Status::failure(relativeEnd.error());
        auto outputEnd = MediaAvAggregateTimeline::presentationMaster(*m_epoch, relativeEnd.value());
        if (!outputEnd) return ::media::Status::failure(outputEnd.error());
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            auto& source = m_sources[i];
            if (const auto* display = std::get_if<MediaVideoDisplaySourceStamp>(&lineage->videoContributions[i].origin))
                source.lastDisplayCommit = MediaAvAggregateDisplayCommit{display->decodedSequence, outputEnd.value()};
            source.committedGeneration = m_pending->sourceGenerations[i];
            if (source.committedGeneration != 0) ++source.committedRealFrames;
            else ++source.committedBlackFrames;
            if (mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
                if (source.committedGeneration) {
                    source.generationObservation.enter(source.committedGeneration);
                    if (!source.generationObservation.firstRealCursor) {
                        source.generationObservation.firstRealCursor = m_nextVideoFrame;
                        source.generationObservation.firstRealSteadyNs = mediaSteadyClockNowNs();
                        const auto& origin = lineage->videoContributions[i].origin;
                        if (const auto* canonical = std::get_if<MediaCanonicalSourceStamp>(&origin))
                            source.generationObservation.firstRealFrame = MediaAggregateFrameObservation{
                                canonical->identity.sourceSequence.value(), canonical->presentation.nanoseconds(),
                                canonical->duration.nanoseconds(), std::nullopt};
                        else if (const auto* display = std::get_if<MediaVideoDisplaySourceStamp>(&origin))
                            source.generationObservation.firstRealFrame = MediaAggregateFrameObservation{
                                display->identity.sourceSequence.value(), display->presentation.nanoseconds(),
                                std::nullopt, display->decodedSequence};
                        logFirstSourceFrame(nodeId(), plan().sources[i].videoIdentity, source.committedGeneration,
                            "aggregate_contribution", *source.generationObservation.firstRealSteadyNs,
                            source.generationObservation.firstRealFrame);
                    }
                    if (source.firstIntervalMiss && source.firstIntervalMiss->generation == source.committedGeneration &&
                        !source.firstIntervalMiss->firstRealCursor) {
                        source.firstIntervalMiss->firstRealCursor = source.generationObservation.firstRealCursor;
                        source.firstIntervalMiss->firstRealSteadyNs = source.generationObservation.firstRealSteadyNs;
                        source.firstIntervalMiss->firstRealFrame = source.generationObservation.firstRealFrame;
                    }
                }
                if (source.firstIntervalMiss && source.firstIntervalMiss->cursor == m_nextVideoFrame &&
                    !source.firstIntervalMiss->committedGeneration) {
                    source.firstIntervalMiss->committedGeneration = source.committedGeneration;
                    source.firstIntervalMiss->committedSteadyNs = mediaSteadyClockNowNs();
                }
            }
        }
        ++m_nextVideoFrame;
        break;
    }
    case PendingKind::VideoEnd: m_videoEndPublished = true; break;
    default: return invalid("Video aggregate has a non-video pending output");
    }
    m_pending.reset();
    return ::media::Status::success();
}

void MediaAvContinuousAggregateNode::logSummary(const char* reason) noexcept
{
    if (m_summaryPublished || !m_dependencies.aggregatePlan) return;
    m_summaryPublished = true;
    m_executionObservation.log(nodeId().value, plan().initialGeneration);
    // Diagnostics must not throw out of the runtime's noexcept abort path.
    try {
        std::ostringstream out;
        out << "continuous_aggregate.summary reason=" << reason
            << " output_group=" << plan().outputGroupKey.value()
            << " output_generation=" << plan().initialGeneration
            << " video_commits=" << m_nextVideoFrame;
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            const auto& source = m_sources[i];
            out << " source[" << i << "].group=" << plan().sources[i].groupKey.value()
                << " source[" << i << "].identity=" << plan().sources[i].videoIdentity
                << " source[" << i << "].real_frames=" << source.committedRealFrames
                << " source[" << i << "].black_frames=" << source.committedBlackFrames
                << " source[" << i << "].contribution_generation=" << source.committedGeneration;
            if (source.firstIntervalMiss) {
                const auto& miss = *source.firstIntervalMiss;
                const std::string prefix = " source[" + std::to_string(i) + "].first_interval_miss_";
                const auto appendFrame = [&](const char* stage,
                    const std::optional<MediaAggregateFrameObservation>& frame) {
                    appendObservedFrame(out, prefix + stage + "_", frame);
                };
                appendFrame("first_candidate_at_miss", miss.generationAtMiss.firstCandidateFrame);
                appendFrame("first_real_at_miss", miss.generationAtMiss.firstRealFrame);
                appendFrame("first_candidate_final", miss.firstCandidateFrame);
                appendFrame("first_real_final", miss.firstRealFrame);
                out << prefix << "cursor=" << miss.cursor
                    << prefix << "target_ns=" << miss.targetNs
                    << prefix << "steady_ns=" << miss.steadyNs
                    << prefix << "generation=" << miss.generation
                    << prefix << "capacity=" << plan().sources[i].maximumVideoCandidates
                    << prefix << "before=" << miss.candidatesBefore
                    << prefix << "after=" << miss.candidatesAfter
                    << prefix << "expired=" << miss.expiredCandidates
                    << prefix << "channel_depth=" << (miss.channelDepth ? std::to_string(*miss.channelDepth) : "unavailable")
                    << prefix << "examined_begin_ns=" << (miss.lastExaminedBeginNs ? std::to_string(*miss.lastExaminedBeginNs) : "unavailable")
                    << prefix << "examined_end_ns=" << (miss.lastExaminedEndNs ? std::to_string(*miss.lastExaminedEndNs) : "unavailable")
                    << prefix << "epoch_source_start_ns=" << miss.epochSourceStartNs
                    << prefix << "epoch_master_release_ns=" << miss.epochMasterReleaseNs
                    << prefix << "prepare_expired=" << miss.generationAtMiss.prepareExpired
                    << prefix << "prepare_old_generation=" << miss.generationAtMiss.prepareOldGeneration
                    << prefix << "prepare_incomplete=" << miss.generationAtMiss.incomplete;
                out << prefix << "first_real_cursor_at_miss=" << (miss.generationAtMiss.firstRealCursor ? std::to_string(*miss.generationAtMiss.firstRealCursor) : "unavailable");
                out << prefix << "first_real_steady_ns_at_miss=" << (miss.generationAtMiss.firstRealSteadyNs ? std::to_string(*miss.generationAtMiss.firstRealSteadyNs) : "unavailable");
                out << prefix << "first_candidate_steady_ns_at_miss=" << (miss.generationAtMiss.firstCandidateSteadyNs ? std::to_string(*miss.generationAtMiss.firstCandidateSteadyNs) : "unavailable");
                out << prefix << "first_candidate_begin_ns_at_miss=" << (miss.generationAtMiss.firstCandidateBeginNs ? std::to_string(*miss.generationAtMiss.firstCandidateBeginNs) : "unavailable");
                out << prefix << "first_candidate_end_ns_at_miss=" << (miss.generationAtMiss.firstCandidateEndNs ? std::to_string(*miss.generationAtMiss.firstCandidateEndNs) : "unavailable");
                out << prefix << "prepare_last_begin_ns=" << (miss.generationAtMiss.lastPrepareBeginNs ? std::to_string(*miss.generationAtMiss.lastPrepareBeginNs) : "unavailable");
                out << prefix << "prepare_last_end_ns=" << (miss.generationAtMiss.lastPrepareEndNs ? std::to_string(*miss.generationAtMiss.lastPrepareEndNs) : "unavailable");
                out << prefix << "first_candidate_steady_ns_final=" << (miss.firstCandidateSteadyNs ? std::to_string(*miss.firstCandidateSteadyNs) : "unavailable");
                out << prefix << "first_candidate_begin_ns_final=" << (miss.firstCandidateBeginNs ? std::to_string(*miss.firstCandidateBeginNs) : "unavailable");
                out << prefix << "first_candidate_end_ns_final=" << (miss.firstCandidateEndNs ? std::to_string(*miss.firstCandidateEndNs) : "unavailable");
                out << prefix << "first_real_cursor=" << (miss.firstRealCursor ? std::to_string(*miss.firstRealCursor) : "unavailable");
                out << prefix << "first_real_steady_ns=" << (miss.firstRealSteadyNs ? std::to_string(*miss.firstRealSteadyNs) : "unavailable");
                out << prefix << "cursor_committed_generation=" << (miss.committedGeneration ? std::to_string(*miss.committedGeneration) : "unavailable");
                out << prefix << "cursor_committed_steady_ns=" << (miss.committedSteadyNs ? std::to_string(*miss.committedSteadyNs) : "unavailable");
            }
        }
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {
        // Preserve runtime teardown if diagnostic string allocation fails.
    }
}

::media::Status MediaAvContinuousAggregateNode::cancelReservedOutput(const MediaBufferRef&)
{
    // Publication failure under backpressure keeps the transaction. Only a
    // revoked source permit invalidates its uncommitted contribution set.
    if (m_rejectedPending) {
        m_pending.reset();
        m_rejectedPending = false;
    }
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaAvContinuousAggregateNode::onProcess(MediaGraphExecutionContext& context)
{
    const auto drain = m_dependencies.coordination->drainSnapshot();
    if (drain.requested && m_videoEndPublished) {
        if (!drain.videoClosed) {
            // onProcess runs only after deferred publication has committed and
            // its channel/permit reservations have been released.
            closeOutputChannels(context);
            if (auto status = m_dependencies.coordination->publishOutputClosed(MediaStreamKind::Video); !status)
                return processProgress(status);
        }
        return processWaiting(); // Keep servicing source purge on this owner.
    }
    if (drain.requested && !m_epoch)
        return processProgress(invalid("Aggregate drain requires an activated output"));
    if (!drain.requested) {
        m_executionObservation.enteredOnProcess();
        if (auto status = m_executionObservation.measure(ExecutionStage::Bind,
            {m_nextVideoFrame, std::nullopt}, [&] { return bindCodecs(context); }); !status)
            return processProgress(status);
        CandidatePreparation preparation;
        if (auto status = m_executionObservation.measure(ExecutionStage::Consume,
            {m_nextVideoFrame, std::nullopt}, [&] { return consumeInputs(context, preparation); }); !status)
            return processProgress(status);
        if (preparation == CandidatePreparation::RetryProgress) return processProgress();
        if (!m_epoch) {
            auto activated = m_executionObservation.measure(ExecutionStage::Activate,
                {m_nextVideoFrame, std::nullopt}, [&] { return activate(context); });
            if (!activated) return ::media::Result<MediaNodeProcessResult>::failure(activated.error());
            return activated.value() ? processProgress() : processWaiting();
        }
    }
    if (m_pending) return processProgress(emitOutput(context, "video", m_pending->buffer));
    if (m_videoEndPublished) return processFinished();
    bool endOfOutput = drain.requested;
    if (!endOfOutput) {
        if (m_nextVideoFrame == std::numeric_limits<std::int64_t>::max())
            return processProgress(invalid("Aggregate output frame counter exhausted"));
        auto presentation = MediaAvAggregateTimeline::videoPresentation(plan(), m_nextVideoFrame);
        if (!presentation) return ::media::Result<MediaNodeProcessResult>::failure(presentation.error());
        auto target = MediaAvAggregateTimeline::presentationMaster(*m_epoch, presentation.value());
        if (!target) return ::media::Result<MediaNodeProcessResult>::failure(target.error());
        auto deadline = target.value().checkedSubtract(plan().preparationLead);
        auto now = m_dependencies.output->clock()->now();
        if (!deadline || !now) return ::media::Result<MediaNodeProcessResult>::failure(!deadline ? deadline.error() : now.error());
        if (now.value() < deadline.value()) return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntilInputOrDeadline(plan().outputGroupKey, deadline.value()));
        const auto completion = m_dependencies.coordination->snapshot();
        endOfOutput = completion.allInputsSealed &&
            (!completion.finalOutputEnd || target.value() >= *completion.finalOutputEnd);
    }
    if (endOfOutput) {
        auto eof = makeMediaBufferRef<MediaControlBuffer>(
            MediaControlBufferKind::Eof, m_epoch->generation);
        eof->setStreamKind(MediaStreamKind::Video);
        MediaFixedCapacityStorage<std::uint64_t> generations;
        generations.resize(m_sources.size());
        m_pending = Pending{PendingKind::VideoEnd, std::move(eof), std::move(generations)};
    } else {
        auto buffer = m_executionObservation.measure(ExecutionStage::Video,
            {m_nextVideoFrame, std::nullopt}, [&] { return buildVideo(context); });
        if (!buffer) return processProgress(::media::Status::failure(buffer.error()));
        m_pending->buffer = std::move(buffer).value();
    }
    return processProgress(m_executionObservation.measure(ExecutionStage::Emit,
        {m_nextVideoFrame, std::nullopt}, [&] { return emitOutput(context, "video", m_pending->buffer); }));
}

} // namespace media::ffmpeg::graph
