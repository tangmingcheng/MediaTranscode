#include "internal/graph/nodes/sync/MediaAvContinuousAggregateNode.h"

#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
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
}

MediaAvContinuousAggregateNode::MediaAvContinuousAggregateNode(
    MediaNodeId node, MediaAvAggregateRuntimeDependencies dependencies)
    : FFmpegNodeRuntime(node, staticKind(), "MediaAvContinuousAggregateNode"),
      m_dependencies(std::move(dependencies))
{
    if (m_dependencies.aggregatePlan) {
        m_sources.resize(plan().sources.size());
        for (auto& source : m_sources)
            source.purge = std::make_shared<MediaOwnerThreadGenerationPurge>();
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
    if (!m_dependencies.aggregatePlan || !m_dependencies.output ||
        m_dependencies.output->key() != plan().outputGroupKey ||
        m_sources.empty() || m_sources.size() != m_dependencies.sources.size() ||
        plan().audioSource >= m_sources.size() ||
        plan().canvas.tiles.size() != m_sources.size() ||
        plan().videoFrameRate.num <= 0 || plan().videoFrameRate.den <= 0 ||
        plan().audio.sampleRate() <= 0 || plan().audio.codecFrameSamples() <= 0 ||
        plan().initialGeneration == 0 || plan().initialAudioSample < 0 ||
        plan().maximumAudioCandidates == 0 || plan().maximumAudioCandidateSamples <= 0 ||
        plan().maximumAudioContributions == 0 || plan().maximumMetadataBytes == 0 ||
        plan().preparationLead.nanoseconds() <= 0)
        return invalid("Continuous aggregate requires its complete planner product");
    m_nextAudioSample = plan().initialAudioSample;
    m_summaryPublished = false;
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        if (!m_dependencies.sources[i].group ||
            m_dependencies.sources[i].group->key() != plan().sources[i].groupKey ||
            plan().sources[i].groupKey == plan().outputGroupKey ||
            m_dependencies.sources[i].group->clock() != m_dependencies.output->clock() ||
            plan().sources[i].maximumVideoCandidates == 0 ||
            (plan().sources[i].discardedAudioPort &&
             (i == plan().audioSource || plan().sources[i].discardedAudioPort->empty())))
            return invalid("Continuous aggregate source clock or capacity differs from its plan");
        for (std::size_t previous = 0; previous < i; ++previous)
            if (plan().sources[previous].groupKey == plan().sources[i].groupKey)
                return invalid("Aggregate source domains must be distinct");
    }
    for (auto& source : m_sources) {
        if (auto status = source.purge->start(context.sharedNodeWakeup(nodeId())); !status) {
            clear();
            return status;
        }
    }
    auto status = FFmpegNodeRuntime::start(context);
    if (!status) clear();
    return status;
}

void MediaAvContinuousAggregateNode::clear() noexcept
{
    for (auto& source : m_sources) {
        source.purge->stop();
        source.video.clear();
        source.minimumGeneration = 0;
        source.committedRealFrames = 0;
        source.committedBlackFrames = 0;
        source.committedGeneration = 0;
        source.videoEnded = false;
        source.epochEnded = false;
        source.discardedAudioEnded = false;
        source.prepared = false;
        source.lastInputEnd.reset();
    }
    m_audio.clear();
    m_pending.reset();
    m_videoCodec.reset();
    m_audioCodec.reset();
    m_canvas = MediaVideoCanvasProducer{};
    m_canvasPrepared = false;
    m_audioCandidateSamples = 0;
    m_committedAudioFrames = 0;
    m_committedRealSamples = 0;
    m_committedSilenceSamples = 0;
    m_audioEnded = false;
    m_epoch.reset();
    m_audioOrigin.reset();
    m_lastInputEnd.reset();
    m_nextVideoFrame = 0;
    m_nextAudioSample = 0;
    m_videoEndPublished = false;
    m_audioEndPublished = false;
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
        source.minimumGeneration = request->nextGeneration;
        source.prepared = false;
        source.lastInputEnd.reset();
        if (i == plan().audioSource) {
            m_audio.clear();
            m_audioCandidateSamples = 0;
        }
        // No new generation can publish before this source barrier acknowledges.
        auto* video = context.findInputChannel(nodeId(), plan().sources[i].videoPort);
        if (!video) return invalid("Aggregate purge lost its source input");
        video->clear();
        if (plan().sources[i].discardedAudioPort) {
            auto* discarded = context.findInputChannel(nodeId(), *plan().sources[i].discardedAudioPort);
            if (!discarded) return invalid("Aggregate purge lost its discarded audio input");
            discarded->clear();
        }
        if (i == plan().audioSource) {
            auto* audio = context.findInputChannel(nodeId(), plan().audioPort);
            if (!audio) return invalid("Aggregate purge lost its audio input");
            audio->clear();
        }
        if (auto status = source.purge->complete(*request, ::media::Status::success()); !status)
            return status;
    }
    m_lastInputEnd.reset();
    for (const auto& source : m_sources)
        if (source.lastInputEnd && (!m_lastInputEnd || *source.lastInputEnd > *m_lastInputEnd))
            m_lastInputEnd = source.lastInputEnd;
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaAvContinuousAggregateNode::process(MediaGraphExecutionContext& context)
{
    if (auto status = servicePurges(context); !status)
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    return FFmpegNodeRuntime::process(context);
}

::media::Status MediaAvContinuousAggregateNode::bindCodecs(MediaGraphExecutionContext& context)
{
    for (auto [port, retained] : {std::pair{"video_codec", &m_videoCodec},
                                 std::pair{"audio_codec", &m_audioCodec}}) {
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
        if (!range.encoderInput || codec->width != plan().canvas.width ||
            codec->height != plan().canvas.height || !codec->hw_frames_ctx ||
            codec->color_range != range.encoderInput->colorRange ||
            codec->pix_fmt != range.encoderInput->pixelFormat ||
            codec->sw_pix_fmt != range.encoderInput->surfacePixelFormat)
            return invalid("Canvas facts differ from the production encoder readback");
        if (auto status = m_canvas.prepare(plan().canvas, codec->hw_frames_ctx,
                context.sharedNodeWakeup(nodeId())); !status) return status;
        m_canvasPrepared = true;
    }
    if (m_audioCodec) {
        const auto* codec = static_cast<const FFmpegCodecContextBuffer*>(m_audioCodec.get())->context();
        if (codec->sample_rate != plan().audio.sampleRate() ||
            codec->frame_size != plan().audio.codecFrameSamples() ||
            codec->ch_layout.nb_channels != plan().audio.channels() ||
            codec->sample_fmt != av_get_sample_fmt(plan().audio.sampleFormat().c_str()))
            return invalid("Aggregate audio differs from the prepared encoder sample grid");
    }
    return ::media::Status::success();
}

::media::Status MediaAvContinuousAggregateNode::consumeInputs(MediaGraphExecutionContext& context)
{
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
                if ((*input.value())->isEof()) { source.discardedAudioEnded = true; break; }
                if ((*input.value())->isFlush()) continue;
                const auto* audio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(input.value()->get());
                if (!audio || !audio->media() || !audio->media()->lineage() ||
                    !std::holds_alternative<MediaSourceAccessUnitIdentity>(audio->media()->lineage()->identity))
                    return invalid("Aggregate discarded audio requires bound source samples");
                // This declared non-selected stream has no output contribution.
                // Releasing its sample lease still drives its source A/V owner.
            }
        }
        const auto capacity = plan().sources[i].maximumVideoCandidates;
        for (std::size_t consumed = 0; consumed < capacity && !source.videoEnded && source.video.size() < capacity; ++consumed) {
            auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), plan().sources[i].videoPort),
                name(), plan().sources[i].videoPort);
            if (!input) return ::media::Status::failure(input.error());
            if (!input.value()) break;
            if ((*input.value())->isEof()) { source.videoEnded = true; break; }
            if ((*input.value())->isFlush()) continue;
            auto lineage = FFmpegFrameView::canonicalLineage(*input.value());
            if (!lineage || !std::holds_alternative<MediaSourceAccessUnitIdentity>(lineage->identity) ||
                !FFmpegFrameView::frame(*input.value()) || lineage->duration.nanoseconds() <= 0)
                return invalid("Aggregate requires source video lineage with an explicit interval");
            if (lineage->generation < source.minimumGeneration) continue;
            if (auto valid = validateMediaCanonicalLineage(*lineage); !valid) return valid;
            if (!source.video.empty()) {
                const auto previous = FFmpegFrameView::canonicalLineage(source.video.back());
                if (previous->generation == lineage->generation && previous->presentation > lineage->presentation)
                    return invalid("Aggregate video candidates are not in presentation order");
            }
            auto epoch = m_dependencies.sources[i].group->playbackEpoch();
            if (!epoch) return ::media::Status::failure(epoch.error());
            if (epoch.value().generation != lineage->generation) continue;
            auto relative = lineage->presentation.checkedSubtract(epoch.value().sourceStart);
            if (!relative) return ::media::Status::failure(relative.error());
            auto begin = epoch.value().masterRelease.checkedAdd(relative.value());
            if (!begin) return ::media::Status::failure(begin.error());
            auto end = begin.value().checkedAdd(lineage->duration);
            if (!end) return ::media::Status::failure(end.error());
            observeInputEnd(i, end.value());
            source.video.push_back(std::move(*input.value()));
            source.prepared = true;
        }
    }
    for (std::size_t consumed = 0; consumed < plan().maximumAudioCandidates && !m_audioEnded &&
        m_audio.size() < plan().maximumAudioCandidates; ++consumed) {
        auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), plan().audioPort), name(), plan().audioPort);
        if (!input) return ::media::Status::failure(input.error());
        if (!input.value()) break;
        if ((*input.value())->isEof()) { m_audioEnded = true; break; }
        if ((*input.value())->isFlush()) continue;
        auto audio = std::dynamic_pointer_cast<MediaBoundCanonicalAudioBuffer>(*input.value());
        if (!audio || audio->audioOrigin().outputSampleRate != plan().audio.sampleRate())
            return invalid("Aggregate requires normalized bound audio samples");
        if (audio->audioOrigin().generation < m_sources[plan().audioSource].minimumGeneration) continue;
        const auto count = audio->media()->interval().sampleCount();
        if (!count || *count > plan().maximumAudioCandidateSamples - m_audioCandidateSamples)
            return invalid("Aggregate audio candidates exceed their planner sample bound");
        const auto& origin = audio->audioOrigin();
        const auto interval = audio->media()->interval();
        if (interval.begin < origin.epochOutputSampleIndex ||
            interval.end < origin.epochOutputSampleIndex)
            return invalid("Aggregate audio precedes its released source epoch");
        if (!m_audio.empty() && m_audio.back()->audioOrigin().generation == origin.generation &&
            m_audio.back()->media()->interval().end > interval.begin)
            return invalid("Aggregate audio source candidates overlap or run backwards");
        auto relativeEnd = MediaRunningTime::checkedFromTicks(
            interval.end - origin.epochOutputSampleIndex, 1, origin.outputSampleRate);
        if (!relativeEnd) return ::media::Status::failure(relativeEnd.error());
        auto masterEnd = origin.masterRelease.checkedAdd(relativeEnd.value());
        if (!masterEnd) return ::media::Status::failure(masterEnd.error());
        observeInputEnd(plan().audioSource, masterEnd.value());
        m_audioCandidateSamples += *count;
        m_audio.push_back(std::move(audio));
    }
    return validateMetadataBound();
}

::media::Result<bool> MediaAvContinuousAggregateNode::activate(MediaGraphExecutionContext& context)
{
    if (allSourcesEnded() && (!m_lastInputEnd || m_audio.empty() ||
        std::any_of(m_sources.begin(), m_sources.end(), [](const auto& source) { return !source.prepared; })))
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Composition sources ended before mandatory preparation completed"));
    if (!m_canvasPrepared || !m_audioCodec || m_audio.empty())
        return ::media::Result<bool>::success(false);
    auto now = m_dependencies.output->clock()->now();
    if (!now) return ::media::Result<bool>::failure(now.error());
    auto release = now.value().checkedAdd(plan().preparationLead);
    if (!release) return ::media::Result<bool>::failure(release.error());
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        if (!m_sources[i].prepared || m_sources[i].video.empty() ||
            m_dependencies.sources[i].group->lifecycleState() != MediaAvSyncGroupRuntime::LifecycleState::Active)
            return ::media::Result<bool>::success(false);
        auto epoch = m_dependencies.sources[i].group->playbackEpoch();
        if (!epoch) return ::media::Result<bool>::failure(epoch.error());
        if (epoch.value().masterRelease > release.value()) release =
            ::media::Result<MediaRunningTime>::success(epoch.value().masterRelease);
    }
    const MediaPlaybackEpoch epoch{MediaRunningTime::fromNanoseconds(0), release.value(), plan().initialGeneration};
    const MediaAudioPlaybackOrigin origin{epoch.generation, epoch.sourceStart,
        epoch.masterRelease, plan().initialAudioSample, plan().audio.sampleRate()};
    auto event = MediaPlaybackEpochActivatedBuffer::create(plan().outputGroupKey, epoch, origin, std::nullopt);
    if (!event) return ::media::Result<bool>::failure(event.error());
    const auto* port = context.graph()->findOutputPort(nodeId(), "activated");
    if (!port) return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument("Aggregate activation output is missing"));
    std::vector<MediaAtomicOutputBatch> batches;
    for (auto* channel : context.outputChannels(nodeId()))
        if (channel && channel->binding().from.portId == port->id)
            batches.push_back({channel, std::span(&event.value(), 1)});
    if (batches.empty()) return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument("Aggregate activation has no consumers"));
    auto publication = MediaAtomicOutputTransaction::acquire("Continuous output activation", batches);
    if (!publication) return ::media::Result<bool>::failure(publication.error());
    if (!publication.value()) return ::media::Result<bool>::success(false);
    if (auto status = m_dependencies.activation.activateInitial(epoch, origin); !status)
        return ::media::Result<bool>::failure(status.error());
    publication.value()->commitReserved();
    m_epoch = epoch;
    m_audioOrigin = origin;
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaRunningTime> MediaAvContinuousAggregateNode::videoTime(std::int64_t frame) const
{
    return MediaRunningTime::checkedFromTicks(frame, plan().videoFrameRate.den, plan().videoFrameRate.num);
}

::media::Result<MediaRunningTime> MediaAvContinuousAggregateNode::audioTime(std::int64_t sample) const
{
    if (sample < std::numeric_limits<std::int64_t>::min() + plan().initialAudioSample)
        return ::media::Result<MediaRunningTime>::failure(::media::ErrorInfo::invalidArgument(
            "Aggregate output sample offset is not representable"));
    return MediaRunningTime::checkedFromTicks(sample - plan().initialAudioSample, 1, plan().audio.sampleRate());
}

::media::Result<MediaRunningTime> MediaAvContinuousAggregateNode::presentationMaster(MediaRunningTime presentation) const
{
    return m_epoch->masterRelease.checkedAdd(presentation);
}

bool MediaAvContinuousAggregateNode::allSourcesEnded() const noexcept
{
    if (!m_audioEnded) return false;
    for (std::size_t i = 0; i < m_sources.size(); ++i) {
        if (!m_sources[i].videoEnded ||
            (plan().sources[i].discardedAudioPort && !m_sources[i].discardedAudioEnded)) return false;
    }
    return true;
}

::media::Result<MediaOutputCommitReservation>
MediaAvContinuousAggregateNode::reserveOutputCommit(const MediaBufferRef&) const
{
    std::vector<MediaAvOutputPermitCommitReservation> permits;
    if (m_pending) {
        permits.reserve(m_sources.size() + 1);
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
    case PendingKind::Video:
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            auto& source = m_sources[i];
            source.committedGeneration = m_pending->sourceGenerations[i];
            if (source.committedGeneration != 0) ++source.committedRealFrames;
            else ++source.committedBlackFrames;
        }
        ++m_nextVideoFrame;
        break;
    case PendingKind::Audio: {
        const auto* audio = static_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
        for (const auto& fragment : audio->media()->fragments()) {
            const auto samples = static_cast<std::uint64_t>(*fragment.interval.sampleCount());
            if (std::holds_alternative<MediaCanonicalAudioRealSource>(fragment.contribution.origin))
                m_committedRealSamples += samples;
            else m_committedSilenceSamples += samples;
        }
        ++m_committedAudioFrames;
        m_nextAudioSample += plan().audio.codecFrameSamples();
        break;
    }
    case PendingKind::VideoEnd: m_videoEndPublished = true; break;
    case PendingKind::AudioEnd: m_audioEndPublished = true; break;
    }
    m_pending.reset();
    return ::media::Status::success();
}

void MediaAvContinuousAggregateNode::logSummary(const char* reason) noexcept
{
    if (m_summaryPublished || !m_dependencies.aggregatePlan) return;
    m_summaryPublished = true;
    // Diagnostics must not throw out of the runtime's noexcept abort path.
    try {
        std::ostringstream out;
        out << "continuous_aggregate.summary reason=" << reason
            << " output_group=" << plan().outputGroupKey.value()
            << " output_generation=" << plan().initialGeneration
            << " video_commits=" << m_nextVideoFrame
            << " audio_commits=" << m_committedAudioFrames
            << " real_samples=" << m_committedRealSamples
            << " silence_samples=" << m_committedSilenceSamples;
        for (std::size_t i = 0; i < m_sources.size(); ++i) {
            const auto& source = m_sources[i];
            out << " source[" << i << "].group=" << plan().sources[i].groupKey.value()
                << " source[" << i << "].real_frames=" << source.committedRealFrames
                << " source[" << i << "].black_frames=" << source.committedBlackFrames
                << " source[" << i << "].contribution_generation=" << source.committedGeneration;
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

::media::Status MediaAvContinuousAggregateNode::observeInputEnd(std::size_t source, MediaRunningTime end)
{
    auto& sourceEnd = m_sources[source].lastInputEnd;
    if (!sourceEnd || end > *sourceEnd) sourceEnd = end;
    if (!m_lastInputEnd || end > *m_lastInputEnd) m_lastInputEnd = end;
    return ::media::Status::success();
}

::media::Status MediaAvContinuousAggregateNode::validateMetadataBound() const
{
    std::uint64_t remaining = plan().maximumMetadataBytes;
    const auto consume = [&](std::uint64_t bytes) {
        if (bytes > remaining) return false;
        remaining -= bytes;
        return true;
    };
    const auto lineageBytes = [&](const MediaCanonicalLineage& lineage) {
        if (!consume(sizeof(lineage))) return false;
        const auto& identity = lineage.identity;
        const auto stringBytes = std::holds_alternative<MediaSourceAccessUnitIdentity>(identity)
            ? std::get<MediaSourceAccessUnitIdentity>(identity).sourceIdentity.capacity()
            : std::get<MediaOutputAccessUnitIdentity>(identity).outputIdentity.capacity();
        if (!consume(stringBytes) || !consume(1)) return false;
        const auto unusedVideo = lineage.videoContributions.capacity() - lineage.videoContributions.size();
        if (unusedVideo > remaining / sizeof(MediaCanonicalVideoContribution) ||
            !consume(unusedVideo * sizeof(MediaCanonicalVideoContribution))) return false;
        for (const auto& contribution : lineage.videoContributions) {
            if (!consume(sizeof(contribution))) return false;
            if (const auto* source = std::get_if<MediaCanonicalSourceStamp>(&contribution.origin))
                if (!consume(source->identity.sourceIdentity.capacity()) || !consume(1)) return false;
        }
        return true;
    };
    const auto audioBytes = [&](const MediaBoundCanonicalAudioBuffer& audio) {
        if (!consume(sizeof(audio)) || !consume(sizeof(MediaCanonicalAudioSamplesBuffer))) return false;
        const auto unusedFragments = audio.media()->fragments().capacity() - audio.media()->fragments().size();
        if (unusedFragments > remaining / sizeof(MediaAudioIntervalFragment) ||
            !consume(unusedFragments * sizeof(MediaAudioIntervalFragment))) return false;
        for (const auto& fragment : audio.media()->fragments()) {
            // Counting a shared lineage for every fragment is conservative and
            // keeps the charge independent of allocator/address identities.
            if (!consume(sizeof(fragment)) || !lineageBytes(*fragment.lineage)) return false;
            if (const auto* real = std::get_if<MediaCanonicalAudioRealSource>(&fragment.contribution.origin))
                if (!consume(real->source.identity.sourceIdentity.capacity()) || !consume(1)) return false;
        }
        return true;
    };
    bool valid = consume(sizeof(*this));
    const auto unusedSources = m_sources.capacity() - m_sources.size();
    valid = valid && unusedSources <= remaining / sizeof(SourceState) && consume(unusedSources * sizeof(SourceState));
    for (const auto& source : m_sources) {
        valid = valid && consume(sizeof(source));
        for (const auto& video : source.video)
            valid = valid && consume(sizeof(MediaBufferRef)) && consume(sizeof(MediaCanonicalVideoFrameBuffer)) &&
                lineageBytes(*FFmpegFrameView::canonicalLineage(video));
    }
    for (const auto& audio : m_audio)
        valid = valid && consume(sizeof(audio)) && audioBytes(*audio);
    if (m_pending) {
        valid = valid && consume(sizeof(Pending));
        for (const auto generation : m_pending->sourceGenerations)
            valid = valid && consume(sizeof(generation));
        if (const auto video = FFmpegFrameView::canonicalLineage(m_pending->buffer))
            valid = valid && consume(sizeof(MediaCanonicalVideoFrameBuffer)) && lineageBytes(*video);
        if (const auto* audio = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(m_pending->buffer.get()))
            valid = valid && audioBytes(*audio);
    }
    return valid ? ::media::Status::success() : invalid(
        "Aggregate retained metadata exceeds its planner bound");
}

::media::Result<MediaNodeProcessResult> MediaAvContinuousAggregateNode::onProcess(MediaGraphExecutionContext& context)
{
    if (auto status = bindCodecs(context); !status) return processProgress(status);
    if (auto status = consumeInputs(context); !status) return processProgress(status);
    if (!m_epoch) {
        auto activated = activate(context);
        if (!activated) return ::media::Result<MediaNodeProcessResult>::failure(activated.error());
        return activated.value() ? processProgress() : processWaiting();
    }
    if (m_pending) {
        const auto output = m_pending->buffer;
        const auto port = m_pending->kind == PendingKind::Video || m_pending->kind == PendingKind::VideoEnd ? "video" : "audio";
        return processProgress(emitOutput(context, port, output));
    }
    if (m_videoEndPublished && m_audioEndPublished) return processFinished();
    if (m_nextVideoFrame == std::numeric_limits<std::int64_t>::max() ||
        m_nextAudioSample > std::numeric_limits<std::int64_t>::max() - plan().audio.codecFrameSamples())
        return processProgress(invalid("Aggregate output sample or frame counter exhausted"));
    auto video = videoTime(m_nextVideoFrame);
    auto audio = audioTime(m_nextAudioSample);
    if (!video || !audio) return ::media::Result<MediaNodeProcessResult>::failure(!video ? video.error() : audio.error());
    const bool chooseVideo = !m_videoEndPublished && (m_audioEndPublished || video.value() <= audio.value());
    auto target = presentationMaster(chooseVideo ? video.value() : audio.value());
    if (!target) return ::media::Result<MediaNodeProcessResult>::failure(target.error());
    auto deadline = target.value().checkedSubtract(plan().preparationLead);
    if (!deadline) return ::media::Result<MediaNodeProcessResult>::failure(deadline.error());
    auto now = m_dependencies.output->clock()->now();
    if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    if (now.value() < deadline.value()) return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::waitingUntilInputOrDeadline(plan().outputGroupKey, deadline.value()));
    if (allSourcesEnded() && (!m_lastInputEnd || target.value() >= *m_lastInputEnd)) {
        if (m_videoEndPublished && m_audioEndPublished) return processFinished();
        auto eof = FFmpegBufferFactory::makeEof(chooseVideo ? MediaStreamKind::Video : MediaStreamKind::Audio);
        if (!eof) return ::media::Result<MediaNodeProcessResult>::failure(eof.error());
        m_pending = Pending{chooseVideo ? PendingKind::VideoEnd : PendingKind::AudioEnd,
            std::move(eof).value(), std::vector<std::uint64_t>(m_sources.size())};
    } else {
        auto buffer = chooseVideo ? buildVideo(context) : buildAudio(context);
        if (!buffer) return processProgress(::media::Status::failure(buffer.error()));
        // Builders set the pending contribution generations before publication.
        m_pending->buffer = std::move(buffer).value();
    }
    const auto output = m_pending->buffer;
    return processProgress(emitOutput(context, chooseVideo ? "video" : "audio", output));
}

} // namespace media::ffmpeg::graph
