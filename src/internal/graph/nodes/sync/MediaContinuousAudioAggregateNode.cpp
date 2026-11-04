#include "internal/graph/runtime/storage/MediaAvAggregateMetadataBudget.h"
#include "internal/graph/nodes/sync/MediaContinuousAudioAggregateNode.h"
#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlanValidator.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaAvAggregateTimeline.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}

MediaContinuousAudioAggregateNode::MediaContinuousAudioAggregateNode(
    MediaNodeId node, MediaContinuousAudioAggregateDependencies dependencies)
    : FFmpegNodeRuntime(node, staticKind(), "MediaContinuousAudioAggregateNode"),
      m_dependencies(std::move(dependencies)), m_purge(std::make_shared<MediaOwnerThreadGenerationPurge>())
{
    if (m_dependencies.aggregatePlan)
        m_audio = MediaFixedCapacityStorage<std::shared_ptr<MediaBoundCanonicalAudioBuffer>>(plan().maximumAudioCandidates);
}

::media::Status MediaContinuousAudioAggregateNode::start(MediaGraphExecutionContext& context)
{
    if (!m_dependencies.aggregatePlan || !m_dependencies.source || !m_dependencies.output ||
        !m_dependencies.coordination) return invalid("Continuous audio aggregate lacks dependencies");
    if (auto status = MediaAvContinuousAggregatePlanValidator::validate(plan()); !status) return status;
    if (m_dependencies.source->key() != plan().sources[plan().audioSource].groupKey ||
        m_dependencies.output->key() != plan().outputGroupKey ||
        m_dependencies.source->clock() != m_dependencies.output->clock())
        return invalid("Continuous audio aggregate source or clock differs from plan");
    m_summaryPublished = false;
    m_audioBuiltObservation = {};
    m_audioCommitObservation = {};
    m_minimumGeneration = plan().initialGeneration;
    m_nextAudioSample = plan().initialAudioSample;
    if (auto status = m_purge->start(context.sharedNodeWakeup(nodeId())); !status) return status;
    if (auto status = m_dependencies.coordination->bindWakeup(MediaStreamKind::Audio,
        context.sharedNodeWakeup(nodeId())); !status) { clear(); return status; }
    auto status = FFmpegNodeRuntime::start(context);
    if (!status) clear();
    return status;
}

void MediaContinuousAudioAggregateNode::clear() noexcept
{
    if (m_dependencies.coordination) m_dependencies.coordination->stop();
    m_purge->stop();
    m_audio.clear();
    m_audioCodec.reset();
    m_pending.reset();
    m_epoch.reset();
    m_audioOrigin.reset();
    m_audioCandidateSamples = 0;
    m_audioEnded = false;
    m_audioEndPublished = false;
    m_rejectedPending = false;
    m_nextAudioSample = 0;
    m_minimumGeneration = 0;
    m_committedAudioFrames = 0;
    m_committedRealSamples = 0;
    m_committedSilenceSamples = 0;
}

::media::Status MediaContinuousAudioAggregateNode::stop(MediaGraphExecutionContext& context)
{
    logSummary("stop");
    auto status = FFmpegNodeRuntime::stop(context);
    clear();
    return status;
}

void MediaContinuousAudioAggregateNode::abort(MediaGraphExecutionContext& context) noexcept
{
    logSummary("abort");
    FFmpegNodeRuntime::abort(context);
    clear();
}

::media::Status MediaContinuousAudioAggregateNode::servicePurge(MediaGraphExecutionContext& context)
{
    const auto request = m_purge->pending();
    if (!request) return ::media::Status::success();
    if (m_pending && m_pending->sourceGenerations[plan().audioSource] != 0 &&
        m_pending->sourceGenerations[plan().audioSource] <= request->oldGeneration) {
        cancelPendingOutputTransfer();
        m_pending.reset();
    }
    m_audio.clear();
    m_audioCandidateSamples = 0;
    m_minimumGeneration = request->nextGeneration;
    auto* channel = context.findInputChannel(nodeId(), plan().audioPort);
    if (!channel) return invalid("Continuous audio purge lost its source input");
    channel->clear();
    if (auto status = m_dependencies.coordination->purgeInput(plan().audioSource, MediaStreamKind::Audio,
        request->oldGeneration, request->nextGeneration); !status) return status;
    return m_purge->complete(*request, ::media::Status::success());
}

::media::Result<MediaNodeProcessResult> MediaContinuousAudioAggregateNode::process(MediaGraphExecutionContext& context)
{
    if (auto status = servicePurge(context); !status)
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult> MediaContinuousAudioAggregateNode::onProcess(MediaGraphExecutionContext& context)
{
    const auto drain = m_dependencies.coordination->drainSnapshot();
    if (drain.requested && m_audioEndPublished) {
        if (!drain.audioClosed) {
            // onProcess runs only after deferred publication has committed and
            // its channel/permit reservations have been released.
            closeOutputChannels(context);
            if (auto status = m_dependencies.coordination->publishOutputClosed(MediaStreamKind::Audio); !status)
                return processProgress(status);
        }
        return processWaiting(); // Keep servicing source purge on this owner.
    }
    if (drain.requested && !m_epoch)
        return processProgress(invalid("Aggregate drain requires an activated output"));
    if (!drain.requested) {
        if (!m_audioCodec) {
            auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), "audio_codec"), name(), "audio_codec");
            if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
            if (input.value()) {
                const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(input.value()->get());
                if (!codec || !codec->context()) return processProgress(invalid("Audio aggregate requires prepared encoder context"));
                const auto* prepared = codec->context();
                if (prepared->sample_rate != plan().audio.sampleRate() ||
                    prepared->frame_size != plan().audio.codecFrameSamples() ||
                    prepared->ch_layout.nb_channels != plan().audio.channels() ||
                    prepared->sample_fmt != av_get_sample_fmt(plan().audio.sampleFormat().c_str()))
                    return processProgress(invalid("Aggregate audio differs from the prepared encoder sample grid"));
                m_audioCodec = std::move(*input.value());
            }
        }
        if (auto status = consumeInputs(context); !status) return processProgress(status);
        if (!m_epoch) {
            if (m_audioCodec && !m_audio.empty() && !m_dependencies.coordination->snapshot().audioReady) {
                if (auto status = m_dependencies.coordination->publishAudioReady(
                    {m_audio.front()->audioOrigin(), m_audio.front()->media()->interval()}); !status)
                    return processProgress(status);
            }
            auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), "activated"), name(), "activated");
            if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
            if (!input.value()) return processWaiting();
            const auto* event = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(input.value()->get());
            if (!event || event->groupKey() != plan().outputGroupKey || !event->activation().audioOrigin() ||
                event->epoch().generation != plan().initialGeneration ||
                event->activation().audioOrigin()->outputSampleRate != plan().audio.sampleRate() ||
                event->activation().audioOrigin()->epochOutputSampleIndex != plan().initialAudioSample || !m_audioCodec)
                return processProgress(invalid("Audio aggregate activation differs from its prepared output"));
            m_epoch = event->epoch();
            m_audioOrigin = *event->activation().audioOrigin();
            return processProgress();
        }
    }
    if (m_pending) return processProgress(emitOutput(context, "audio", m_pending->buffer));
    if (m_audioEndPublished) return processFinished();
    bool endOfOutput = drain.requested;
    if (!endOfOutput) {
        if (m_nextAudioSample > std::numeric_limits<std::int64_t>::max() - plan().audio.codecFrameSamples())
            return processProgress(invalid("Aggregate output sample counter exhausted"));
        auto presentation = MediaAvAggregateTimeline::audioPresentation(plan(), m_nextAudioSample);
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
        eof->setStreamKind(MediaStreamKind::Audio);
        MediaFixedCapacityStorage<std::uint64_t> generations;
        generations.resize(plan().sources.size());
        m_pending = Pending{PendingKind::AudioEnd, std::move(eof), std::move(generations)};
    } else {
        auto output = buildAudio(context);
        if (!output) return processProgress(::media::Status::failure(output.error()));
        m_pending->buffer = std::move(output).value();
    }
    return processProgress(emitOutput(context, "audio", m_pending->buffer));
}

::media::Result<MediaOutputCommitReservation>
MediaContinuousAudioAggregateNode::reserveOutputCommit(const MediaBufferRef&) const
{
    MediaFixedCapacityStorage<MediaAvOutputPermitCommitReservation> permits(2);
    if (m_pending && m_pending->sourceGenerations[plan().audioSource]) {
        auto source = m_dependencies.source->reserveOutputCommit(m_pending->sourceGenerations[plan().audioSource]);
        if (!source) {
            m_rejectedPending = true;
            return ::media::Result<MediaOutputCommitReservation>::failure(source.error());
        }
        permits.push_back(std::move(source).value());
    }
    auto output = m_dependencies.output->reserveOutputCommit(plan().initialGeneration);
    if (!output) return ::media::Result<MediaOutputCommitReservation>::failure(output.error());
    permits.push_back(std::move(output).value());
    return ::media::Result<MediaOutputCommitReservation>::success(MediaOutputCommitReservation::hold(std::move(permits)));
}

::media::Status MediaContinuousAudioAggregateNode::cancelReservedOutput(const MediaBufferRef&)
{
    if (m_rejectedPending) { m_pending.reset(); m_rejectedPending = false; }
    return ::media::Status::success();
}

::media::Status MediaContinuousAudioAggregateNode::commitReservedOutput(const MediaBufferRef& buffer)
{
    if (!m_pending || m_pending->buffer != buffer) return invalid("Audio aggregate commit lacks its transaction");
    if (m_pending->kind == PendingKind::AudioEnd) m_audioEndPublished = true;
    else if (m_pending->kind == PendingKind::Audio) {
        const auto* audio = static_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
        m_audioCommitObservation.observe(audio->audioOrigin().generation, audio->media()->interval());
        for (const auto& fragment : audio->media()->fragments()) {
            const auto count = static_cast<std::uint64_t>(*fragment.interval.sampleCount());
            if (std::holds_alternative<MediaCanonicalAudioRealSource>(fragment.contribution.origin)) m_committedRealSamples += count;
            else m_committedSilenceSamples += count;
        }
        ++m_committedAudioFrames;
        m_nextAudioSample += plan().audio.codecFrameSamples();
    } else return invalid("Audio aggregate has a non-audio pending output");
    m_pending.reset();
    return ::media::Status::success();
}

void MediaContinuousAudioAggregateNode::logSummary(const char* reason) noexcept
{
    if (m_summaryPublished || !m_dependencies.aggregatePlan) return;
    m_summaryPublished = true;
    m_audioBuiltObservation.log(nodeId().value, "aggregate_built");
    m_audioCommitObservation.log(nodeId().value, "aggregate_committed");
    try {
        std::ostringstream out;
        out << "continuous_audio_aggregate.summary reason=" << reason << " output_group=" << plan().outputGroupKey.value()
            << " audio_commits=" << m_committedAudioFrames << " real_samples=" << m_committedRealSamples
            << " silence_samples=" << m_committedSilenceSamples;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode, out.str());
    } catch (...) {}
}


::media::Status MediaContinuousAudioAggregateNode::consumeInputs(MediaGraphExecutionContext& context)
{
    for (std::size_t consumed = 0; consumed < plan().maximumAudioCandidates && !m_audioEnded &&
        m_audio.size() < plan().maximumAudioCandidates; ++consumed) {
        auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), plan().audioPort), name(), plan().audioPort);
        if (!input) return ::media::Status::failure(input.error());
        if (!input.value()) break;
        if ((*input.value())->isEof()) { m_audioEnded = true; if (auto status = m_dependencies.coordination->sealInput(plan().audioSource, MediaStreamKind::Audio); !status) return status; break; }
        if ((*input.value())->isFlush()) continue;
        auto audio = std::dynamic_pointer_cast<MediaBoundCanonicalAudioBuffer>(*input.value());
        if (!audio || audio->audioOrigin().outputSampleRate != plan().audio.sampleRate())
            return invalid("Aggregate requires normalized bound audio samples");
        for (const auto& fragment : audio->media()->fragments()) {
            const auto* identity = fragment.lineage
                ? std::get_if<MediaSourceAccessUnitIdentity>(&fragment.lineage->identity) : nullptr;
            const auto* real = std::get_if<MediaCanonicalAudioRealSource>(&fragment.contribution.origin);
            if (!identity || !real || !fragment.lineage->videoContributions.empty() ||
                !plan().sources[plan().audioSource].audioIdentity ||
                identity->sourceIdentity.view() != *plan().sources[plan().audioSource].audioIdentity ||
                real->source.identity.sourceIdentity.view() != *plan().sources[plan().audioSource].audioIdentity)
                return invalid("Aggregate source audio metadata differs from prepared identity");
        }
        if (audio->audioOrigin().generation < m_minimumGeneration) continue;
        const auto count = audio->media()->interval().sampleCount();
        if (!count || *count > plan().maximumAudioCandidateSamples - m_audioCandidateSamples) {
            std::ostringstream detail;
            detail << "Aggregate audio candidates exceed their planner sample bound"
                   << " generation=" << audio->audioOrigin().generation
                   << " interval_begin=" << audio->media()->interval().begin
                   << " interval_end=" << audio->media()->interval().end
                   << " incoming_samples=";
            if (count) detail << *count; else detail << "invalid";
            detail << " retained_samples=" << m_audioCandidateSamples
                   << " retained_candidates=" << m_audio.size()
                   << " maximum_candidates=" << plan().maximumAudioCandidates
                   << " maximum_samples=" << plan().maximumAudioCandidateSamples;
            return invalid(detail.str().c_str());
        }
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
        if (auto status = m_dependencies.coordination->observeInputEnd(plan().audioSource, MediaStreamKind::Audio, origin.generation, masterEnd.value()); !status) return status;
        m_audioCandidateSamples += *count;
        m_audio.push_back(std::move(audio));
    }
    return MediaAvAggregateMetadataBudget::validate(plan(), {}, m_audio, m_pending ? m_pending->buffer : MediaBufferRef{});
}

} // namespace media::ffmpeg::graph
