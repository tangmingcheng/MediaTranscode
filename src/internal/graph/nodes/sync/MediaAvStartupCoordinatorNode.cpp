#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNode.h"

#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool hasTerminalHead(const std::deque<MediaBufferRef>& pending) noexcept
{
    return !pending.empty() &&
           dynamic_cast<const MediaControlBuffer*>(pending.front().get()) != nullptr;
}

} // namespace

MediaAvStartupCoordinatorNode::MediaAvStartupCoordinatorNode(
    MediaNodeId nodeId,
    MediaAvStartupCoordinatorNodePreparation preparation)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvStartupCoordinatorNode")
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
    return m_generationState;
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_deferredTerminalError) {
        auto error = std::move(*m_deferredTerminalError);
        m_deferredTerminalError.reset();
        return ::media::Result<MediaNodeProcessResult>::failure(std::move(error));
    }
    if (m_terminalBarrierActive) {
        auto video = fillSnapshotBarrierMedia(context, "video", m_pendingVideo,
                                              m_videoTerminalBarrierRemaining);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        auto audio = fillSnapshotBarrierMedia(context, "audio", m_pendingAudio,
                                              m_audioTerminalBarrierRemaining);
        if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
        auto clock = fillTerminalBarrierClock(context);
        if (!clock) return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    } else if (!m_clockBarrierActive) {
        auto video = fillPendingMedia(context, "video", m_pendingVideo);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        auto audio = fillPendingMedia(context, "audio", m_pendingAudio);
        if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
    } else {
        auto video = fillSnapshotBarrierMedia(context, "video", m_pendingVideo,
                                              m_videoClockBarrierRemaining);
        if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
        auto audio = fillSnapshotBarrierMedia(context, "audio", m_pendingAudio,
                                              m_audioClockBarrierRemaining);
        if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
    }
    if (!m_terminalBarrierActive) {
        auto clock = fillPendingClock(context);
        if (!clock) return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (m_pendingClock && !m_clockBarrierActive && !m_terminalBarrierActive) {
        if (auto status = activateClockBarrier(context); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    if (!m_terminalBarrierActive &&
        (hasTerminalHead(m_pendingVideo) || hasTerminalHead(m_pendingAudio))) {
        if (auto status = activateTerminalBarrier(context); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }
    auto selected = selectPending();
    if (!selected) return ::media::Result<MediaNodeProcessResult>::failure(selected.error());
    if (!selected.value()) return processWaiting();
    return *selected.value() == PendingInput::Clock
        ? processClock()
        : processOne(context, *selected.value());
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillPendingMedia(
    MediaGraphExecutionContext& context,
    const char* portName,
    std::deque<MediaBufferRef>& pending)
{
    if (!pending.empty()) return ::media::Result<bool>::success(false);
    auto input = tryPopInputOptional(context, portName);
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    pending.push_back(std::move(*input.value()));
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
    return ::media::Result<bool>::success(true);
}

::media::Result<bool> MediaAvStartupCoordinatorNode::fillSnapshotBarrierMedia(
    MediaGraphExecutionContext& context,
    const char* portName,
    std::deque<MediaBufferRef>& pending,
    std::size_t& remaining)
{
    if (!pending.empty() || remaining == 0) {
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
    pending.push_back(std::move(*input.value()));
    return ::media::Result<bool>::success(true);
}

::media::Status MediaAvStartupCoordinatorNode::activateClockBarrier(
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
    auto audio = snapshotSize("audio");
    if (!audio) return ::media::Status::failure(audio.error());
    m_videoClockBarrierRemaining = video.value();
    m_audioClockBarrierRemaining = audio.value();
    m_clockBarrierActive = true;
    return ::media::Status::success();
}

::media::Status MediaAvStartupCoordinatorNode::activateTerminalBarrier(
    MediaGraphExecutionContext& context)
{
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
    auto audio = snapshotSize("audio", hasTerminalHead(m_pendingAudio));
    if (!audio) return ::media::Status::failure(audio.error());
    MediaChannel* clock = context.findInputChannel(nodeId(), "clock");
    if (!clock) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvStartupCoordinatorNode is missing the clock input"));
    }
    m_videoTerminalBarrierRemaining = video.value();
    m_audioTerminalBarrierRemaining = audio.value();
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
    if (!m_pendingAudio.empty() && !hasTerminalHead(m_pendingAudio)) {
        const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
            m_pendingAudio.front().get());
        if (!envelope) return ::media::Result<std::optional<PendingInput>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode audio input requires a common canonical envelope"));
        consider(PendingInput::Audio, m_pendingAudio.front(), envelope->observedAt());
    }
    if (!m_pendingVideo.empty() && !hasTerminalHead(m_pendingVideo)) {
        const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
            m_pendingVideo.front().get());
        if (!envelope) return ::media::Result<std::optional<PendingInput>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode video input requires a common canonical envelope"));
        consider(PendingInput::Video, m_pendingVideo.front(), envelope->observedAt());
    }
    if (m_pendingClock) {
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

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::processClock()
{
    const auto* tick = dynamic_cast<const MediaAvStartupClockBuffer*>(m_pendingClock.get());
    if (!tick) return ::media::Result<MediaNodeProcessResult>::failure(
        ::media::ErrorInfo::internalError("Selected startup clock head is invalid"));
    if (m_lastClock && tick->masterNow() < *m_lastClock) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode rejects master clock regression"));
    }
    m_lastClock = tick->masterNow();
    m_pendingClock.reset();
    m_clockBarrierActive = false;
    m_videoClockBarrierRemaining = 0;
    m_audioClockBarrierRemaining = 0;
    auto status = m_coordinator->poll(*m_lastClock);
    return status ? processProgress()
                   : ::media::Result<MediaNodeProcessResult>::failure(
                         status.error().toErrorInfo());
}

::media::Result<MediaNodeProcessResult> MediaAvStartupCoordinatorNode::processOne(
    MediaGraphExecutionContext& context,
    PendingInput input)
{
    const char* portName = input == PendingInput::Video ? "video" : "audio";
    auto& pending = input == PendingInput::Video ? m_pendingVideo : m_pendingAudio;
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            pending.front().get())) {
        return processControl(context, input, *control);
    }
    const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(
        pending.front().get());
    if (!envelope || !envelope->media()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode requires a common canonical envelope"));
    }
    const auto& unit = envelope->unit();
    if ((unit.stream == MediaAvStartupStream::Video && std::string(portName) != "video") ||
        (unit.stream == MediaAvStartupStream::Audio && std::string(portName) != "audio")) {
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
    const MediaAvStartupUnitId id{unit.stream, unit.generation, unit.sequence};
    if (unit.readiness == MediaSourceClockReadiness::Locked &&
        m_generationState) {
        auto stored = m_generationState->store(
            m_generationState->groupKey(), unit, envelope->media());
        if (!stored) {
            return ::media::Result<MediaNodeProcessResult>::failure(stored.error());
        }
    }
    auto decision = m_coordinator->submit(unit, envelope->observedAt());
    if (!decision) {
        if (m_generationState) m_generationState->erase(id);
        return ::media::Result<MediaNodeProcessResult>::failure(
            decision.error().toErrorInfo());
    }
    erasePurged(decision.value().purged);
    auto output = prepareOutput(decision.value(), *envelope);
    if (!output) return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    pending.pop_front();
    if (output.value()) {
        auto status = emitOutput(context, "release", *output.value());
        if (!status) return processProgress(status);
    }
    return processProgress();
}

::media::Result<std::optional<MediaBufferRef>>
MediaAvStartupCoordinatorNode::prepareOutput(
    const MediaAvStartupDecision& decision,
    const MediaAvStartupEnvelopeBuffer& envelope)
{
    std::vector<MediaAvReleasedUnit> video;
    std::vector<MediaAvReleasedUnit> audio;
    std::optional<MediaPlaybackEpoch> epoch;
    if (decision.release) {
        epoch = decision.release->epoch;
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
        m_generationState->erase(MediaAvStartupUnitId{envelope.unit().stream,
                                                      envelope.unit().generation,
                                                      envelope.unit().sequence});
    }
    if (!epoch) {
        return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
    }
    if (!m_generationState || !m_generationState->audioSampleRate()) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V startup release requires planned group and audio origin"));
    }
    auto release = MediaAvStartupReleaseBuffer::create(
        m_generationState->groupKey(),
        decision.release ? MediaAvStartupReleaseKind::InitialAtomicRelease
                         : MediaAvStartupReleaseKind::ActiveEpochPassThrough,
        *epoch,
        MediaAudioPlaybackOrigin{epoch->generation, epoch->sourceStart,
                                 epoch->masterRelease, 0,
                                 *m_generationState->audioSampleRate()},
        std::move(video), std::move(audio));
    if (!release) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            release.error());
    }
    return ::media::Result<std::optional<MediaBufferRef>>::success(
        std::move(release).value());
}

void MediaAvStartupCoordinatorNode::erasePurged(
    const std::vector<MediaAvStartupUnitId>& purged) noexcept
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
    MediaBufferRef output = pending.front();
    MediaAvSyncStatus terminalStatus = MediaAvSyncStatus::success();
    bool publishTerminal = false;
    if (control.controlKind() == MediaControlBufferKind::Eof) {
        terminalStatus = m_coordinator->endOfStream(stream);
        publishTerminal = terminalStatus && m_coordinator->terminalEofReached();
    } else {
        terminalStatus = m_coordinator->fail(
            control.controlKind() == MediaControlBufferKind::Abort
                ? "upstream abort"
                : "upstream flush/discontinuity");
        publishTerminal = true;
    }
    pending.pop_front();
    m_terminalBarrierActive = false;
    m_videoTerminalBarrierRemaining = 0;
    m_audioTerminalBarrierRemaining = 0;
    m_clockTerminalBarrierRemaining = 0;
    if (publishTerminal) {
        if (m_terminalControlCommitted) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaAvStartupCoordinatorNode rejects duplicate terminal control"));
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
    auto reset = m_coordinator->reset();
    if (!reset) return ::media::Status::failure(reset.error().toErrorInfo());
    m_generationState->reset();
    clearTransientState();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAvStartupCoordinatorNode::stop(MediaGraphExecutionContext& context)
{
    if (m_coordinator) m_coordinator->stop();
    if (m_generationState) m_generationState->reset();
    clearTransientState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvStartupCoordinatorNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_coordinator) m_coordinator->abort();
    if (m_generationState) m_generationState->reset();
    clearTransientState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvStartupCoordinatorNode::clearTransientState() noexcept
{
    m_pendingVideo.clear();
    m_pendingAudio.clear();
    m_pendingClock.reset();
    m_clockBarrierActive = false;
    m_videoClockBarrierRemaining = 0;
    m_audioClockBarrierRemaining = 0;
    m_terminalBarrierActive = false;
    m_videoTerminalBarrierRemaining = 0;
    m_audioTerminalBarrierRemaining = 0;
    m_clockTerminalBarrierRemaining = 0;
    m_lastVideoObservedAt.reset();
    m_lastAudioObservedAt.reset();
    m_lastClock.reset();
    m_terminalControlCommitted = false;
    m_deferredTerminalError.reset();
}

MediaAvStartupCoordinatorNode::~MediaAvStartupCoordinatorNode()
{
    if (m_generationState) m_generationState->reset();
}

} // namespace media::ffmpeg::graph
