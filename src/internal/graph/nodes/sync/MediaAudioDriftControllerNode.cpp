#include "internal/graph/nodes/sync/MediaAudioDriftControllerNode.h"

#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaAudioDriftControllerState.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

namespace media::ffmpeg::graph {

namespace {

::media::Result<MediaRunningTime> outputEndOnMaster(
    const MediaAudioPlaybackOrigin& origin,
    std::int64_t outputEnd)
{
    if (outputEnd < origin.epochOutputSampleIndex) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio drift output position precedes playback epoch"));
    }
    auto offset = MediaRunningTime::checkedFromTicks(
        outputEnd - origin.epochOutputSampleIndex, 1, origin.outputSampleRate);
    if (!offset) return offset;
    return origin.masterRelease.checkedAdd(offset.value());
}

} // namespace

MediaAudioDriftControllerNode::MediaAudioDriftControllerNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAudioDriftControllerNode")
    , m_groupKey(std::move(groupKey))
    , m_state(std::make_shared<MediaAudioDriftControllerState>())
{
}

std::string_view MediaAudioDriftControllerNode::generationPurgeIdentity() noexcept
{
    return MediaAudioCorrectionGenerationIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaAudioDriftControllerNode::generationPurgeTarget() const noexcept
{
    return m_state;
}

MediaAudioDriftControllerNode::~MediaAudioDriftControllerNode() = default;

MediaNodeKind MediaAudioDriftControllerNode::staticKind() noexcept
{
    return MediaNodeKind::AudioDriftController;
}

::media::Result<MediaAudioDriftMeasurement>
MediaAudioDriftControllerNode::measureCanonicalPosition(
    const MediaAudioPlaybackOrigin& origin,
    MediaRunningTime sourceEndOnMaster,
    MediaCanonicalAudioSampleInterval projectedOutput,
    MediaRunningTime observedAt,
    std::uint64_t sequence)
{
    if (sequence == 0 || origin.generation == 0 ||
        origin.outputSampleRate <= 0 ||
        projectedOutput.sampleRate != origin.outputSampleRate ||
        projectedOutput.end <= projectedOutput.begin) {
        return ::media::Result<MediaAudioDriftMeasurement>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio drift measurement requires exact active lineage"));
    }
    auto outputEnd = outputEndOnMaster(origin, projectedOutput.end);
    if (!outputEnd) {
        return ::media::Result<MediaAudioDriftMeasurement>::failure(
            outputEnd.error());
    }
    auto phase = sourceEndOnMaster.checkedSubtract(outputEnd.value());
    if (!phase) {
        return ::media::Result<MediaAudioDriftMeasurement>::failure(phase.error());
    }
    return ::media::Result<MediaAudioDriftMeasurement>::success(
        MediaAudioDriftMeasurement{phase.value(), observedAt, origin.generation,
                                   sequence, projectedOutput.begin,
                                   origin.outputSampleRate});
}

::media::Status MediaAudioDriftControllerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAudioDriftControllerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAudioDriftControllerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAudioDriftControllerNode::resetState() noexcept
{
    m_group.reset();
    m_state->resetForLifecycle();
}

::media::Status MediaAudioDriftControllerNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_group) return ::media::Status::success();
    auto runtime = context.findAvSyncGroup(m_groupKey);
    if (!runtime) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Audio drift controller requires its registered sync group"));
    }
    m_group = std::move(runtime);
    return ::media::Status::success();
}

bool MediaAudioDriftControllerNode::pendingOutputIsCurrent(
    const MediaBufferRef& buffer) const noexcept
{
    return m_state->pendingOutputIsCurrent(buffer, std::nullopt);
}

::media::Status MediaAudioDriftControllerNode::stage(const MediaBufferRef& audio)
{
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(
        audio.get());
    if (!bound || !m_group) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio drift controller requires trimmed bound canonical audio"));
    }
    const auto snapshot = m_group->epochTransitionSnapshot();
    if (m_group->lifecycleState() !=
            MediaAvSyncGroupRuntime::LifecycleState::Active ||
        !snapshot.audioOrigin || *snapshot.audioOrigin != bound->audioOrigin()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Audio drift controller requires the exact active playback origin"));
    }
    if (auto observed = m_state->observe(bound->audioOrigin().generation);
        !observed) {
        return observed;
    }
    const auto& fragments = bound->media()->fragments();
    if (fragments.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Audio drift controller requires non-empty canonical fragments"));
    }
    auto sourceEnd = MediaRunningTime::checkedFromTicks(
        fragments.back().interval.end, 1,
        fragments.back().interval.sampleRate);
    if (!sourceEnd) return ::media::Status::failure(sourceEnd.error());
    auto sourceEndMaster = m_group->mapCanonicalToMaster(sourceEnd.value());
    auto observedAt = m_group->clock()->now();
    if (!sourceEndMaster || !observedAt) {
        return ::media::Status::failure(
            !sourceEndMaster ? sourceEndMaster.error() : observedAt.error());
    }

    auto candidateProjection = m_state->projection;
    if (!candidateProjection) {
        auto created = MediaAudioSampleProjection::create(
            bound->audioOrigin().epochOutputSampleIndex,
            fragments.front().interval.sampleRate,
            bound->audioOrigin().outputSampleRate);
        if (!created) return ::media::Status::failure(created.error());
        candidateProjection = std::move(created).value();
    }
    MediaCanonicalAudioSampleInterval projected{};
    for (const auto& fragment : fragments) {
        if (!fragment.lineage ||
            fragment.lineage->generation != bound->audioOrigin().generation ||
            fragment.interval.sampleRate != candidateProjection->sourceSampleRate()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Audio drift controller rejects changed lineage or source rate"));
        }
        auto interval = candidateProjection->append(
            fragment.interval.end - fragment.interval.begin);
        if (!interval) return ::media::Status::failure(interval.error());
        projected = interval.value();
    }
    auto measurement = measureCanonicalPosition(
        bound->audioOrigin(), sourceEndMaster.value(), projected,
        observedAt.value(), m_state->nextSequence);
    if (!measurement) return ::media::Status::failure(measurement.error());

    auto candidateServo = m_state->servo;
    if (!candidateServo) {
        const auto& plan = m_group->plan();
        if (!plan.topology || !plan.recovery.hardDiscontinuityThresholdNs) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Audio drift controller requires complete planner servo policy"));
        }
        auto created = MediaAudioDriftServo::create(
            *plan.topology, plan.audioServo,
            *plan.recovery.hardDiscontinuityThresholdNs,
            bound->audioOrigin().generation);
        if (!created) {
            return ::media::Status::failure(
                created.error().toErrorInfo());
        }
        candidateServo = std::move(created).value();
    }
    auto decision = candidateServo->update(measurement.value());
    if (!decision) {
        return ::media::Status::failure(
            decision.error().toErrorInfo());
    }
    MediaBufferRef correction;
    if (decision.value().kind() == MediaAudioServoDecisionKind::Apply) {
        correction = makeMediaBufferRef<MediaAudioCorrectionBuffer>(
            *decision.value().command());
    } else if (decision.value().kind() != MediaAudioServoDecisionKind::None) {
        const auto reason = decision.value().kind() ==
                MediaAudioServoDecisionKind::DropOldGeneration
            ? MediaAvReacquisitionReason::FutureGeneration
            : MediaAvReacquisitionReason::HardDiscontinuity;
        m_group->requestReacquisition(
            {decision.value().generation(), reason});
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Audio drift controller requested epoch reacquisition"));
    }
    m_state->pending = MediaAudioDriftControllerState::PendingTransaction{
        audio, std::move(correction), std::move(*candidateServo),
        std::move(*candidateProjection), bound->audioOrigin(),
        m_state->nextSequence + 1};
    return ::media::Status::success();
}

::media::Result<bool> MediaAudioDriftControllerNode::outputsReady(
    MediaGraphExecutionContext& context) const
{
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    MediaChannel* correction = context.findOutputChannel(nodeId(), "correction");
    if (!audio || !correction || audio->closed() || correction->closed() ||
        audio->aborted() || correction->aborted()) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::cancelled(
            "Audio drift controller output is unavailable"));
    }
    if (audio->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer ||
        correction->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::invalidArgument(
            "Audio drift controller requires blocking transactional outputs"));
    }
    return ::media::Result<bool>::success(
        audio->size() < audio->capacity() &&
        (!m_state->pending->correction ||
         correction->size() < correction->capacity()));
}

::media::Status MediaAudioDriftControllerNode::commit(
    MediaGraphExecutionContext& context)
{
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    MediaChannel* correction = context.findOutputChannel(nodeId(), "correction");
    if (m_state->pending->correction &&
        correction->pushOutcome(m_state->pending->correction) !=
            MediaQueuePushOutcome::Accepted) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "Audio correction commit diverged after preflight"));
    }
    if (audio->pushOutcome(m_state->pending->audio) !=
        MediaQueuePushOutcome::Accepted) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "Audio media commit diverged after preflight"));
    }
    m_state->servo = std::move(m_state->pending->servo);
    m_state->projection = std::move(m_state->pending->projection);
    m_state->origin = m_state->pending->origin;
    m_state->nextSequence = m_state->pending->nextSequence;
    m_state->pending.reset();
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaAudioDriftControllerNode::onProcess(MediaGraphExecutionContext& context)
{
    if (auto configured = configure(context); !configured) {
        return processProgress(configured);
    }
    auto stateLock = m_state->lock();
    if (!m_state->pending) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "audio"),
            "Audio drift controller", "audio");
        if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
        if (!input.value()) return processWaiting();
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
                input.value()->get())) {
            if (control->controlKind() == MediaControlBufferKind::Unknown) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Audio drift controller rejects unknown control"));
            }
            if (auto authorized = m_state->authorizeRetainedControl(
                    *input.value()); !authorized) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    authorized.error());
            }
            auto status = emitOutput(context, "audio", *input.value());
            return control->controlKind() == MediaControlBufferKind::Eof ||
                           control->controlKind() == MediaControlBufferKind::Abort
                ? processFinished(std::move(status))
                : processProgress(std::move(status));
        }
        if (auto staged = stage(*input.value()); !staged) {
            return ::media::Result<MediaNodeProcessResult>::failure(staged.error());
        }
    }
    auto ready = outputsReady(context);
    if (!ready) return ::media::Result<MediaNodeProcessResult>::failure(ready.error());
    if (!ready.value()) return processWaiting();
    return processProgress(commit(context));
}

} // namespace media::ffmpeg::graph
