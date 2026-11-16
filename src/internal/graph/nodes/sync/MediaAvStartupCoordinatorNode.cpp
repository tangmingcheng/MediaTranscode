#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"

#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool hasTerminalHead(const std::optional<MediaBufferRef>& pending) noexcept
{
    return pending &&
           dynamic_cast<const MediaControlBuffer*>(pending->get()) != nullptr;
}

} // namespace

MediaAvStartupCoordinatorNode::MediaAvStartupCoordinatorNode(
    MediaNodeId nodeId,
    MediaAvStartupCoordinatorNodePreparation preparation)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvStartupCoordinatorNode")
    , m_allocation(std::move(preparation.m_allocation))
    , m_releaseStorage(std::move(preparation.m_releaseStorage))
    , m_ownerPurge(std::make_shared<MediaOwnerThreadGenerationPurge>())
    , m_coordinator(std::move(preparation.m_coordinator))
    , m_generationState(std::move(preparation.m_generationState))
{
}

MediaNodeKind MediaAvStartupCoordinatorNode::staticKind() noexcept
{
    return MediaNodeKind::AvStartupCoordinator;
}

std::string_view MediaAvStartupCoordinatorNode::generationPurgeIdentity() noexcept
{
    return MediaAvStartupGenerationState::plannedIdentity();
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaAvStartupCoordinatorNode::generationPurgeTarget() const noexcept
{
    return m_ownerPurge;
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::process(
    MediaGraphExecutionContext& context)
try
{
    if (const auto purge = m_ownerPurge->pending()) {
        cancelPendingOutputTransfer();
        auto retired = m_coordinator->retireGeneration(*purge);
        auto status = retired ? m_generationState->purge(*purge)
            : ::media::Status::failure(retired.error().toErrorInfo());
        if (status) {
            clearTransientState();
            m_lastReleasedGeneration = purge->publishedGeneration;
        }
        auto completed = m_ownerPurge->complete(*purge, status);
        if (!completed) return processProgress(completed);
        return processProgress(status);
    }
    return FFmpegNodeRuntime::process(context);
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::allocationFailed("Startup coordinator runtime allocation failed"));
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::onProcess(
    MediaGraphExecutionContext& context)
{
    bool activatedClockBarrier = false;
    if (m_deferredTerminalError) {
        auto error = std::move(*m_deferredTerminalError);
        m_deferredTerminalError.reset();
        return ::media::Result<MediaNodeProcessResult>::failure(std::move(error));
    }
    auto deadlines = m_coordinator->deadlines();
    if (!deadlines) return ::media::Result<MediaNodeProcessResult>::failure(
        deadlines.error().toErrorInfo());
    const auto deadline = deadlines.value()
        ? std::optional<MediaRunningTime>(deadlines.value()->wakeAt()) : std::nullopt;
    if (m_terminalBarrierActive) {
        auto video = fillSnapshotBarrierMedia(context, "video", m_pendingVideo,
                                              m_videoTerminalBarrierRemaining);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        if (m_coordinator->audioConfig()) {
            auto audio = fillSnapshotBarrierMedia(context, "audio", m_pendingAudio,
                                                  m_audioTerminalBarrierRemaining);
            if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
        }
        auto clock = fillTerminalBarrierClock(context);
        if (!clock) return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    } else if (!m_clockBarrierActive) {
        auto video = fillPendingMedia(context, "video", m_pendingVideo);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        if (m_coordinator->audioConfig()) {
            auto audio = fillPendingMedia(context, "audio", m_pendingAudio);
            if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
        }
    } else {
        if (!m_clockBarrierSnapshotSealed) {
            if (auto status = sealClockBarrier(context); !status) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    status.error());
            }
        }
        auto video = fillSnapshotBarrierMedia(context, "video", m_pendingVideo,
                                              m_videoClockBarrierRemaining);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        if (m_coordinator->audioConfig()) {
            auto audio = fillSnapshotBarrierMedia(context, "audio", m_pendingAudio,
                                                  m_audioClockBarrierRemaining);
            if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
        }
    }
    if (!m_terminalBarrierActive) {
        auto clock = fillPendingClock(context);
        if (!clock) return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (!m_pendingClock && !m_terminalBarrierActive && deadline &&
        !hasTerminalHead(m_pendingVideo) && !hasTerminalHead(m_pendingAudio)) {
        const auto group = context.findAvSyncGroup(m_generationState->groupKey());
        if (!group || !group->clock())
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized("Startup deadline requires its planned master clock"));
        auto now = group->clock()->now();
        if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
        if (now.value() >= *deadline) {
            m_pendingClock = makeMediaBufferRef<MediaAvStartupClockBuffer>(now.value());
            m_pendingClockIsDeadline = true;
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "av_startup_deadline deadline_ns=" + std::to_string(deadline->nanoseconds()) +
                    " observed_ns=" + std::to_string(now.value().nanoseconds()));
        }
    }
    if (m_pendingClock && !m_clockBarrierActive && !m_terminalBarrierActive) {
        if (auto status = activateClockBarrier(context); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        activatedClockBarrier = true;
    }
    if (!m_terminalBarrierActive &&
        (hasTerminalHead(m_pendingVideo) || hasTerminalHead(m_pendingAudio))) {
        if (auto status = activateTerminalBarrier(context); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    if (activatedClockBarrier) return processProgress();
    auto selected = selectPending();
    if (!selected) return ::media::Result<MediaNodeProcessResult>::failure(selected.error());
    if (!selected.value()) {
        return deadline && !m_terminalBarrierActive
            ? ::media::Result<MediaNodeProcessResult>::success(
                  MediaNodeProcessResult::waitingUntilInputOrDeadline(
                      m_generationState->groupKey(), *deadline))
            : processWaiting();
    }
    return *selected.value() == PendingInput::Clock
        ? processClock(context)
        : processOne(context, *selected.value());
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillPendingMedia(
    MediaGraphExecutionContext& context,
    const char* portName,
    std::optional<MediaBufferRef>& pending)
{
    if (pending) return ::media::Result<bool>::success(false);
    auto input = tryPopInputOptional(context, portName);
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    pending = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillPendingClock(
    MediaGraphExecutionContext& context)
{
    if (m_pendingClock) return ::media::Result<bool>::success(false);
    auto input = tryPopInputOptional(context, "clock");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    m_pendingClock = std::move(*input.value());
    m_pendingClockIsDeadline = false;
    return ::media::Result<bool>::success(true);
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillTerminalBarrierClock(
    MediaGraphExecutionContext& context)
{
    if (m_pendingClock || m_clockTerminalBarrierRemaining == 0) {
        return ::media::Result<bool>::success(false);
    }
    auto input = tryPopInputOptional(context, "clock");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::internalError(
                "MediaAvStartupCoordinatorNode lost a terminal clock snapshot input"));
    }
    --m_clockTerminalBarrierRemaining;
    m_pendingClock = std::move(*input.value());
    m_pendingClockIsDeadline = false;
    return ::media::Result<bool>::success(true);
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillSnapshotBarrierMedia(
    MediaGraphExecutionContext& context,
    const char* portName,
    std::optional<MediaBufferRef>& pending,
    std::size_t& remaining)
{
    if (pending || remaining == 0) {
        return ::media::Result<bool>::success(false);
    }
    auto input = tryPopInputOptional(context, portName);
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::internalError(
                "MediaAvStartupCoordinatorNode lost a snapshot-barrier input"));
    }
    --remaining;
    pending = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Status MediaAvStartupCoordinatorNode::activateClockBarrier(
    MediaGraphExecutionContext& context)
{
    (void)context;
    m_videoClockBarrierRemaining = 0;
    m_audioClockBarrierRemaining = 0;
    m_clockBarrierSnapshotSealed = false;
    m_clockBarrierActive = true;
    return ::media::Status::success();
}

::media::Status MediaAvStartupCoordinatorNode::sealClockBarrier(
    MediaGraphExecutionContext& context)
{
    const auto snapshotSize = [&](const char* portName) -> ::media::Result<std::size_t> {
        MediaChannel* channel = context.findInputChannel(nodeId(), portName);
        if (!channel) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaAvStartupCoordinatorNode is missing a media input"));
        }
        return ::media::Result<std::size_t>::success(channel->size());
    };
    auto video = snapshotSize("video");
    if (!video) return ::media::Status::failure(video.error());
    m_videoClockBarrierRemaining = video.value();
    if (m_coordinator->audioConfig()) {
        auto audio = snapshotSize("audio");
        if (!audio) return ::media::Status::failure(audio.error());
        m_audioClockBarrierRemaining = audio.value();
    }
    m_clockBarrierSnapshotSealed = true;
    return ::media::Status::success();
}

::media::Status MediaAvStartupCoordinatorNode::activateTerminalBarrier(
    MediaGraphExecutionContext& context)
{
    if (m_pendingClockIsDeadline) resetClockBarrier();
    const auto snapshotSize = [&](const char* portName,
                                  bool terminalHead) -> ::media::Result<std::size_t> {
        if (terminalHead) return ::media::Result<std::size_t>::success(0);
        MediaChannel* channel = context.findInputChannel(nodeId(), portName);
        if (!channel) {
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaAvStartupCoordinatorNode is missing a media input"));
        }
        return ::media::Result<std::size_t>::success(channel->size());
    };
    auto video = snapshotSize("video", hasTerminalHead(m_pendingVideo));
    if (!video) return ::media::Status::failure(video.error());
    if (m_coordinator->audioConfig()) {
        auto audio = snapshotSize("audio", hasTerminalHead(m_pendingAudio));
        if (!audio) return ::media::Status::failure(audio.error());
        m_audioTerminalBarrierRemaining = audio.value();
    }
    MediaChannel* clock = context.findInputChannel(nodeId(), "clock");
    if (!clock) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvStartupCoordinatorNode is missing the clock input"));
    }
    m_videoTerminalBarrierRemaining = video.value();
    m_clockTerminalBarrierRemaining = clock->size();
    m_terminalBarrierActive = true;
    return ::media::Status::success();
}

::media::Result<std::optional<MediaAvStartupCoordinatorNode::PendingInput>>
MediaAvStartupCoordinatorNode::selectPending() const
{
    std::optional<PendingInput> selected;
    std::optional<MediaRunningTime> selectedTime;
    const auto consider = [&](PendingInput input,
                              const MediaBufferRef& pending,
                              MediaRunningTime eventTime) {
        if (!pending) return;
        if (!selectedTime || eventTime < *selectedTime) {
            selected = input;
            selectedTime = eventTime;
        }
    };
    if (m_pendingAudio && !hasTerminalHead(m_pendingAudio)) {
        const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
            m_pendingAudio->get());
        if (!envelope) return ::media::Result<std::optional<PendingInput>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode audio input requires a common canonical envelope"));
        consider(PendingInput::Audio, *m_pendingAudio, envelope->observedAt());
    }
    if (m_pendingVideo && !hasTerminalHead(m_pendingVideo)) {
        const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
            m_pendingVideo->get());
        if (!envelope) return ::media::Result<std::optional<PendingInput>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode video input requires a common canonical envelope"));
        consider(PendingInput::Video, *m_pendingVideo, envelope->observedAt());
    }
    const bool clockBarrierMediaDrained =
        !m_clockBarrierActive ||
        (m_clockBarrierSnapshotSealed &&
         !m_pendingVideo && !m_pendingAudio &&
         m_videoClockBarrierRemaining == 0 &&
         m_audioClockBarrierRemaining == 0);
    if (m_pendingClock && clockBarrierMediaDrained) {
        const auto* tick = dynamic_cast<const MediaAvStartupClockBuffer*>(m_pendingClock.get());
        if (!tick) return ::media::Result<std::optional<PendingInput>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode clock input requires a master clock tick"));
        consider(PendingInput::Clock, m_pendingClock, tick->masterNow());
    }
    if (!selected && hasTerminalHead(m_pendingAudio)) selected = PendingInput::Audio;
    if (!selected && hasTerminalHead(m_pendingVideo)) selected = PendingInput::Video;
    return ::media::Result<std::optional<PendingInput>>::success(selected);
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::processClock(MediaGraphExecutionContext& context)
{
    const auto* tick = dynamic_cast<const MediaAvStartupClockBuffer*>(m_pendingClock.get());
    if (!tick) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::internalError("Selected startup clock head is invalid"));
    if (!m_pendingClockIsDeadline && m_lastClock && tick->masterNow() < *m_lastClock) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode rejects master clock regression"));
    }
    const auto observedAt = tick->masterNow();
    // External notifications and the local timer are independent producers.
    // Only external FIFO samples participate in that stream's regression check.
    if (!m_pendingClockIsDeadline) m_lastClock = observedAt;
    resetClockBarrier();
    auto outcome = m_coordinator->poll(observedAt);
    if (!outcome) return ::media::Result<MediaNodeProcessResult>::failure(
        outcome.error().toErrorInfo());
    if (!outcome.value()) return processProgress();
    const auto& expired = *outcome.value();
    erasePurged(expired.purged);
    const auto group = context.findAvSyncGroup(m_generationState->groupKey());
    if (!group || !group->preservesActivatedOutput())
        return ::media::Result<MediaNodeProcessResult>::failure(expired.error.toErrorInfo());
    const auto evidence = group->clockEvidence();
    if (!evidence || evidence->generation != expired.generation)
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("Expired source attempt lost its clock evidence"));
    m_retiredClockEvidenceRevision = evidence->revision;
    m_pendingVideo.reset();
    m_pendingAudio.reset();
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "av_source_attempt state=waiting_for_evidence generation=" +
            std::to_string(expired.generation) + " revision=" + std::to_string(evidence->revision));
    return processProgress();
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::processOne(
    MediaGraphExecutionContext& context,
    PendingInput input)
{
    auto& pending = input == PendingInput::Video ? m_pendingVideo : m_pendingAudio;
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            pending->get())) {
        return processControl(context, input, *control);
    }
    const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
        pending->get());
    if (!envelope || !envelope->media()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode requires a common canonical envelope"));
    }
    const auto& unit = envelope->unit();
    if (unit.stream != (input == PendingInput::Video
            ? MediaAvStartupStream::Video : MediaAvStartupStream::Audio) ||
        (unit.stream == MediaAvStartupStream::Audio && !m_coordinator->audioConfig())) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode stream does not match its input port"));
    }
    auto& lastObservedAt = unit.stream == MediaAvStartupStream::Video
        ? m_lastVideoObservedAt
        : m_lastAudioObservedAt;
    if (lastObservedAt && envelope->observedAt() < *lastObservedAt) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode rejects per-stream event-time regression"));
    }
    lastObservedAt = envelope->observedAt();
    if (unit.stream == MediaAvStartupStream::Video && unit.keyFrame &&
        !m_keyTraceEmitted) {
        m_keyTraceEmitted = true;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("av_startup_trace stage=coordinator_key sequence=") +
                std::to_string(unit.sequence) + " generation=" +
                std::to_string(unit.generation));
    }
    if (m_coordinator->state() == MediaAvSyncState::WaitingForEvidence) {
        const auto group = context.findAvSyncGroup(m_generationState->groupKey());
        const auto evidence = group ? group->clockEvidence() : std::nullopt;
        if (!m_retiredClockEvidenceRevision || !evidence ||
            evidence->revision <= *m_retiredClockEvidenceRevision ||
            evidence->generation != unit.generation || envelope->observedAt() < evidence->acceptedAt) {
            pending.reset();
            return processProgress();
        }
        auto resumed = m_coordinator->resumeAfterEvidence(envelope->observedAt());
        if (!resumed) return ::media::Result<MediaNodeProcessResult>::failure(resumed.error().toErrorInfo());
        m_retiredClockEvidenceRevision.reset();
    }
    auto decision = m_coordinator->submit(unit, envelope->observedAt());
    if (!decision) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            decision.error().toErrorInfo());
    }
    const bool retainsPayload =
        decision.value().release.has_value() ||
        decision.value().disposition == MediaAvStartupDisposition::Buffered;
    if (retainsPayload &&
        unit.readiness == MediaSourceClockReadiness::Locked &&
        m_generationState) {
        auto stored = m_generationState->store(
            m_generationState->groupKey(), unit, envelope->media());
        if (!stored) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                stored.error());
        }
    }
    erasePurged(decision.value().purged);
    auto output = prepareOutput(context, decision.value(), *envelope);
    if (!output) return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    pending.reset();
    if (output.value()) {
        auto status = emitOutput(context, "release", *output.value());
        if (!status) return processProgress(status);
    }
    return processProgress();
}

::media::Result<std::optional<MediaBufferRef>>
MediaAvStartupCoordinatorNode::prepareOutput(
    MediaGraphExecutionContext& context,
    const MediaAvStartupDecision& decision,
    const MediaAvStartupEnvelopeBuffer& envelope)
try {
    const bool passThrough = decision.disposition == MediaAvStartupDisposition::PassThrough;
    const auto videoCount = decision.release ? decision.release->video.size()
        : std::size_t(passThrough && envelope.unit().stream == MediaAvStartupStream::Video);
    const auto audioCount = decision.release ? decision.release->audio.size()
        : std::size_t(passThrough && envelope.unit().stream == MediaAvStartupStream::Audio);
    auto* resource = MediaLineageAllocator<MediaAvReleasedUnit>{m_allocation}.resource();
    if (!resource) return ::media::Result<std::optional<MediaBufferRef>>::failure(
        ::media::ErrorInfo::invalidArgument("Startup release requires an owned allocation resource"));
    MediaAvReleasedUnits video(videoCount, *resource);
    MediaAvReleasedUnits audio(audioCount, *resource);
    std::optional<MediaPlaybackEpoch> epoch;
    MediaAvStartupReleaseKind releaseKind =
        MediaAvStartupReleaseKind::ActiveEpochPassThrough;
    std::optional<std::uint64_t> completedTransitionSequence;
    if (decision.release) {
        epoch = decision.release->epoch;
        if (m_lastReleasedGeneration) {
            auto group = context.findAvSyncGroup(
                m_generationState->groupKey());
            const auto reacquisition = group
                ? group->reacquisitionSnapshot()
                : MediaAvReacquisitionSnapshot{
                      MediaAvReacquisitionPhase::Inactive,
                      std::nullopt,
                      std::nullopt};
            if (!group ||
                reacquisition.phase !=
                    MediaAvReacquisitionPhase::Acquiring ||
                !reacquisition.transition ||
                reacquisition.transition->publishedGeneration != *m_lastReleasedGeneration ||
                reacquisition.transition->nextGeneration !=
                    epoch->generation) {
                return ::media::Result<
                    std::optional<MediaBufferRef>>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "A/V startup next release requires the exact completed group transition"));
            }
            releaseKind = MediaAvStartupReleaseKind::NextAtomicRelease;
            completedTransitionSequence =
                reacquisition.transition->transitionSequence;
        } else {
            releaseKind =
                MediaAvStartupReleaseKind::InitialAtomicRelease;
        }
        for (const auto& selected : decision.release->video) {
            auto found = m_generationState->take(selected.id);
            if (!found) return ::media::Result<std::optional<MediaBufferRef>>::failure(found.error());
            video.push_back({std::move(found).value(), selected.trimLeadingSamples});
        }
        for (const auto& selected : decision.release->audio) {
            auto found = m_generationState->take(selected.id);
            if (!found) return ::media::Result<std::optional<MediaBufferRef>>::failure(found.error());
            audio.push_back({std::move(found).value(), selected.trimLeadingSamples});
        }
    } else if (decision.disposition == MediaAvStartupDisposition::PassThrough) {
        epoch = m_coordinator->playbackEpoch();
        if (!epoch) return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::internalError("Running startup coordinator has no epoch"));
        if (envelope.unit().stream == MediaAvStartupStream::Video) {
            video.push_back({envelope.media(), 0});
        } else {
            audio.push_back({envelope.media(), 0});
        }
    }
    if (!epoch) {
        return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
    }
    const auto plannedGroup = context.findAvSyncGroup(m_generationState->groupKey());
    if (!plannedGroup || plannedGroup->plan().members != m_coordinator->members())
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::notInitialized("Startup release requires its frozen planned members"));
    std::optional<MediaAudioPlaybackOrigin> audioOrigin;
    if (const auto* config = m_coordinator->audioConfig())
        audioOrigin = MediaAudioPlaybackOrigin{epoch->generation, epoch->sourceStart,
            epoch->masterRelease, 0, config->outputSampleRate};
    auto activation = MediaPlaybackActivation::create(
        m_coordinator->members(), *epoch, std::move(audioOrigin));
    if (!activation) return ::media::Result<std::optional<MediaBufferRef>>::failure(activation.error());
    auto release = MediaAvStartupReleaseBuffer::create(
        m_releaseStorage, m_generationState->groupKey(), releaseKind, std::move(activation).value(),
        std::move(video), std::move(audio),
        completedTransitionSequence);
    if (!release) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            release.error());
    }
    if (releaseKind == MediaAvStartupReleaseKind::NextAtomicRelease) {
        auto group = context.findAvSyncGroup(
            m_generationState->groupKey());
        if (!group) {
            return ::media::Result<std::optional<MediaBufferRef>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V startup next release lost its sync group"));
        }
        if (auto ready = group->markReacquisitionReadyForActivation(
                epoch->generation,
                *completedTransitionSequence); !ready) {
            return ::media::Result<std::optional<MediaBufferRef>>::failure(
                ready.error());
        }
    }
    if (decision.release) {
        m_lastReleasedGeneration = epoch->generation;
    }
    return ::media::Result<std::optional<MediaBufferRef>>::success(
        std::move(release).value());
} catch (const std::bad_alloc&) {
    return ::media::Result<std::optional<MediaBufferRef>>::failure(
        ::media::ErrorInfo::allocationFailed("Startup release backing allocation failed"));
} catch (const std::length_error&) {
    return ::media::Result<std::optional<MediaBufferRef>>::failure(
        ::media::ErrorInfo::allocationFailed("Startup release backing size is not representable"));
}

void MediaAvStartupCoordinatorNode::erasePurged(
    const MediaAvStartupIds& purged) noexcept
{
    if (m_generationState) m_generationState->erase(purged);
}

::media::Result<MediaNodeProcessResult>
MediaAvStartupCoordinatorNode::processControl(
    MediaGraphExecutionContext& context,
    PendingInput input,
    const MediaControlBuffer& control)
{
    auto& pending = input == PendingInput::Video ? m_pendingVideo : m_pendingAudio;
    const auto stream = input == PendingInput::Video
        ? MediaAvStartupStream::Video
        : MediaAvStartupStream::Audio;
    MediaBufferRef output = *pending;
    MediaAvSyncStatus terminalStatus = MediaAvSyncStatus::success();
    bool publishTerminal = false;
    const bool endGeneration = control.controlKind() == MediaControlBufferKind::EndGeneration;
    if (endGeneration) {
        auto classified = mediaEndGeneration(output);
        if (!classified) return processProgress(::media::Status::failure(classified.error()));
        const auto group = context.findAvSyncGroup(m_generationState->groupKey());
        const auto closing = group ? group->sourceDrainSnapshot() : std::nullopt;
        const auto& sourceGeneration = m_coordinator->generation();
        const auto& epoch = m_coordinator->playbackEpoch();
        const auto* member = closing ? (input == PendingInput::Video ? &closing->video :
            closing->audio ? &*closing->audio : nullptr) : nullptr;
        // tryRelease constructs this epoch from the source generation. Validate that
        // frozen binding, never borrow a later activation to identify this control.
        if (!group || !group->preservesActivatedOutput() || !closing || !member ||
            !member->acceptedFrontier || !sourceGeneration ||
            classified.value() != sourceGeneration || !epoch ||
            epoch->generation != *sourceGeneration ||
            epoch->generation != closing->request.playbackGeneration ||
            m_generationState->hasRetainedMedia())
            return processProgress(::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Startup generation boundary requires sealed input and its fully released frozen epoch")));
    }
    if (control.controlKind() == MediaControlBufferKind::Eof || endGeneration) {
        auto sealed = m_coordinator->sealMember(stream,
            endGeneration ? MediaAvStartupMemberClosure::EndGeneration : MediaAvStartupMemberClosure::Eof,
            control.generation());
        if (!sealed) terminalStatus = MediaAvSyncStatus::failure(sealed.error());
        publishTerminal = sealed && sealed.value();
    } else {
        terminalStatus = m_coordinator->fail(
            control.controlKind() == MediaControlBufferKind::Abort
                ? "upstream abort"
                : "upstream flush/discontinuity");
        publishTerminal = true;
    }
    pending.reset();
    m_terminalBarrierActive = false;
    m_videoTerminalBarrierRemaining = 0;
    m_audioTerminalBarrierRemaining = 0;
    m_clockTerminalBarrierRemaining = 0;
    if (publishTerminal) {
        if (m_terminalControlCommitted) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaAvStartupCoordinatorNode rejects duplicate group boundary"));
        }
        m_terminalControlCommitted = true;
        auto emitted = emitOutput(context, "release", output);
        if (!emitted) {
            if (emitted.error().code == ::media::ErrorCode::WouldBlock && !terminalStatus) {
                m_deferredTerminalError = terminalStatus.error().toErrorInfo();
            }
            return control.controlKind() == MediaControlBufferKind::Eof && terminalStatus
                ? processFinished(emitted)
                : processProgress(emitted);
        }
        if (control.controlKind() == MediaControlBufferKind::Eof && terminalStatus) {
            return processFinished();
        }
    }
    return terminalStatus
        ? processProgress()
        : ::media::Result<MediaNodeProcessResult>::failure(
              terminalStatus.error().toErrorInfo());
}

::media::Status MediaAvStartupCoordinatorNode::start(
    MediaGraphExecutionContext& context)
{
    if (!m_coordinator || !m_generationState) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvStartupCoordinatorNode requires factory preparation"));
    }
    const auto group = context.findAvSyncGroup(m_generationState->groupKey());
    if (!group || group->plan().members != m_coordinator->members())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Startup coordinator members differ from the registered group"));
    auto reset = m_coordinator->reset();
    if (!reset) return ::media::Status::failure(reset.error().toErrorInfo());
    m_generationState->reset();
    clearTransientState();
    auto started = m_ownerPurge->start(context.sharedNodeWakeup(nodeId()));
    return started ? FFmpegNodeRuntime::start(context) : started;
}

::media::Status MediaAvStartupCoordinatorNode::stop(MediaGraphExecutionContext& context)
{
    m_ownerPurge->stop();
    if (m_coordinator) m_coordinator->stop();
    if (m_generationState) m_generationState->reset();
    clearTransientState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvStartupCoordinatorNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_ownerPurge->stop();
    if (m_coordinator) m_coordinator->abort();
    if (m_generationState) m_generationState->reset();
    clearTransientState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvStartupCoordinatorNode::resetClockBarrier() noexcept
{
    m_pendingClock.reset();
    m_pendingClockIsDeadline = false;
    m_clockBarrierActive = false;
    m_clockBarrierSnapshotSealed = false;
    m_videoClockBarrierRemaining = 0;
    m_audioClockBarrierRemaining = 0;
}

void MediaAvStartupCoordinatorNode::clearTransientState() noexcept
{
    m_pendingVideo.reset();
    m_pendingAudio.reset();
    resetClockBarrier();
    m_terminalBarrierActive = false;
    m_videoTerminalBarrierRemaining = 0;
    m_audioTerminalBarrierRemaining = 0;
    m_clockTerminalBarrierRemaining = 0;
    m_lastVideoObservedAt.reset();
    m_lastAudioObservedAt.reset();
    m_lastClock.reset();
    m_retiredClockEvidenceRevision.reset();
    m_terminalControlCommitted = false;
    m_keyTraceEmitted = false;
    m_lastReleasedGeneration.reset();
    m_deferredTerminalError.reset();
}

MediaAvStartupCoordinatorNode::~MediaAvStartupCoordinatorNode()
{
    if (m_generationState) m_generationState->reset();
}

} // namespace media::ffmpeg::graph
