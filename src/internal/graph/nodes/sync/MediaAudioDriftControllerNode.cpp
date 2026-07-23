#include "internal/graph/nodes/sync/MediaAudioDriftControllerNode.h"

#include "internal/graph/runtime/buffer/MediaAudioCorrectionBuffer.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaAudioDriftControllerState.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

#include <array>
#include <span>

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
        MediaAudioDriftMeasurement{phase.value(), sourceEndOnMaster,
                                   origin.generation, sequence,
                                   projectedOutput.begin,
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
    if (!sourceEndMaster) {
        return ::media::Status::failure(sourceEndMaster.error());
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
        m_state->nextSequence);
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

::media::Result<bool> MediaAudioDriftControllerNode::commitIfReady(
    MediaGraphExecutionContext& context)
{
    MediaChannel* audio = context.findOutputChannel(nodeId(), "audio");
    MediaChannel* correction = context.findOutputChannel(nodeId(), "correction");
    if (!audio || !correction) {
        return ::media::Result<bool>::failure(::media::ErrorInfo::notInitialized(
            "Audio drift controller requires explicit transactional outputs"));
    }
    const std::array<MediaBufferRef, 1> audioValues{m_state->pending->audio};
    const std::span<const MediaBufferRef> correctionValues =
        m_state->pending->correction
        ? std::span<const MediaBufferRef>(&m_state->pending->correction, 1)
        : std::span<const MediaBufferRef>{};
    const std::array<MediaAtomicOutputBatch, 2> batches{
        MediaAtomicOutputBatch{correction, correctionValues},
        MediaAtomicOutputBatch{audio, audioValues}};
    auto acquired = MediaAtomicOutputTransaction::acquire(
        "Audio drift controller", batches);
    if (!acquired) {
        return ::media::Result<bool>::failure(acquired.error());
    }
    if (!acquired.value()) return ::media::Result<bool>::success(false);
    if (auto committed = acquired.value()->commit(); !committed) {
        return ::media::Result<bool>::failure(committed.error());
    }
    m_state->servo = std::move(m_state->pending->servo);
    m_state->projection = std::move(m_state->pending->projection);
    m_state->origin = m_state->pending->origin;
    m_state->nextSequence = m_state->pending->nextSequence;
    m_state->pending.reset();
    return ::media::Result<bool>::success(true);
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
    auto committed = commitIfReady(context);
    if (!committed) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            committed.error());
    }
    return committed.value() ? processProgress() : processWaiting();
}

} // namespace media::ffmpeg::graph
