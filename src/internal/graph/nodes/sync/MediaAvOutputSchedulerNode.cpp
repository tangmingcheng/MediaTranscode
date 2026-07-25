#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaVideoRepeatRequestBuffer.h"

#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

const char* reacquisitionCauseName(
    MediaVideoReacquisitionCause cause) noexcept
{
    switch (cause) {
    case MediaVideoReacquisitionCause::HardPhaseError:
        return "hard_phase_error";
    case MediaVideoReacquisitionCause::RecoveryBudgetExhausted:
        return "recovery_budget_exhausted";
    case MediaVideoReacquisitionCause::GenerationMismatch:
        return "generation_mismatch";
    }
    return "unknown";
}

} // namespace

MediaAvOutputSchedulerNode::MediaAvOutputSchedulerNode(MediaNodeId nodeId)
    : MediaAvOutputSchedulerNode(
          nodeId,
          [](const MediaAvSyncPlan& plan, std::uint64_t generation) {
              return MediaVideoSyncController::create(plan, generation);
          })
{
}

MediaAvOutputSchedulerNode::MediaAvOutputSchedulerNode(
    MediaNodeId nodeId,
    VideoControllerFactory controllerFactory)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvOutputSchedulerNode")
    , m_videoControllerFactory(std::move(controllerFactory))
{
}

MediaNodeKind MediaAvOutputSchedulerNode::staticKind() noexcept
{
    return MediaNodeKind::AvOutputScheduler;
}

::media::Status MediaAvOutputSchedulerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto status = configure(context);
    return status ? FFmpegNodeRuntime::start(context) : status;
}

::media::Status MediaAvOutputSchedulerNode::configure(
    MediaGraphExecutionContext& context)
{
    auto groupName = requiredNodeOption(
        nodeOptions(context), "MediaAvOutputSchedulerNode",
        "av_scheduler.sync_group");
    if (!groupName) return ::media::Status::failure(groupName.error());
    m_groupKey.emplace(std::move(groupName).value());
    auto transportLead = requiredNonNegativeIntNodeOption(
        nodeOptions(context), "MediaAvOutputSchedulerNode",
        "av_scheduler.transport_lead_ns");
    if (!transportLead) return ::media::Status::failure(transportLead.error());
    m_transportLead = MediaRunningTime::fromNanoseconds(transportLead.value());
    m_group = context.findAvSyncGroup(*m_groupKey);
    if (!m_group) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires a registered sync group"));
    if (context.inputChannels(nodeId()).size() != 2 ||
        !context.findInputChannel(nodeId(), "video") ||
        !context.findInputChannel(nodeId(), "audio")) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler requires exactly one video and one audio input"));
    }
    auto outputs = context.outputChannels(nodeId());
    if (outputs.size() != 1 || !outputs.front() ||
        outputs.front()->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler output requires one BlockProducer edge"));
    }
    return ::media::Status::success();
}

::media::Status MediaAvOutputSchedulerNode::configureActiveScheduling()
{
    if (m_videoController) return ::media::Status::success();
    if (!m_videoControllerFactory) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires a video controller factory"));
    }
    if (!m_group || m_group->lifecycleState() !=
            MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires an active sync group"));
    }
    auto epoch = m_group->playbackEpoch();
    if (!epoch) return ::media::Status::failure(epoch.error());
    auto controller = m_videoControllerFactory(
        m_group->plan(), epoch.value().generation);
    if (!controller) {
        return ::media::Status::failure(controller.error().toErrorInfo());
    }
    m_videoController = std::make_unique<MediaVideoSyncController>(
        std::move(controller).value());
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::process(
    MediaGraphExecutionContext& context)
{
    auto abortStatus = preflightInputAbort(context);
    if (!abortStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            abortStatus.error());
    }
    if (!m_group) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler has no registered sync group"));
    }
    if (m_group->lifecycleState() ==
        MediaAvSyncGroupRuntime::LifecycleState::AwaitingEpoch) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    if (auto configured = configureActiveScheduling(); !configured) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            configured.error());
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Status MediaAvOutputSchedulerNode::preflightInputAbort(
    MediaGraphExecutionContext& context) noexcept
{
    auto* video = context.findInputChannel(nodeId(), "video");
    auto* audio = context.findInputChannel(nodeId(), "audio");
    if (!video || !audio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler abort preflight requires both inputs"));
    }
    if (!video->aborted() && !audio->aborted()) {
        return ::media::Status::success();
    }
    if (m_group) {
        m_group->markAborted();
    }
    cancelPendingOutputTransfer();
    clearSchedulingState();
    return ::media::Status::failure(
        ::media::ErrorInfo::cancelled(
            "A/V scheduler input was aborted"));
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_pendingCommit) {
        if (pendingOutputBufferCount() != 0) return processWaiting();
        auto commit = std::move(*m_pendingCommit);
        m_pendingCommit.reset();
        return ::media::Result<MediaNodeProcessResult>::success(
            applyCommit(std::move(commit)));
    }
    if (!m_group || !m_videoController) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler has no active scheduling state"));
    }
    if (m_group->lifecycleState() !=
        MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "A/V scheduler sync group is not active"));
    }
    auto video = fillHead(context, Input::Video);
    if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
    auto audio = fillHead(context, Input::Audio);
    if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
    auto control = arbitrateControlHeads();
    if (!control) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            control.error());
    }
    if (control.value()) {
        return processTerminal(context, *control.value());
    }
    auto generationPreflight = preflightGenerations();
    if (!generationPreflight) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            generationPreflight.error());
    }
    if (generationPreflight.value()) return processProgress();
    if (m_videoEof && m_audioEof && !m_videoHead && !m_audioHead && m_terminal) {
        return emitWithCommit(
            context, m_terminal,
            MediaAvSchedulerPendingCommit{
                MediaAvSchedulerCommitKind::Terminal, {}, {}, {}, true});
    }
    auto selected = selectMediaHead();
    if (!selected) return ::media::Result<MediaNodeProcessResult>::failure(selected.error());
    if (!selected.value()) logMissingMediaWait();
    return selected.value() ? processSelected(context, *selected.value())
                            : processWaiting();
}

::media::Result<bool> MediaAvOutputSchedulerNode::fillHead(
    MediaGraphExecutionContext& context, Input input)
{
    auto& head = input == Input::Video ? m_videoHead : m_audioHead;
    const bool eof = input == Input::Video ? m_videoEof : m_audioEof;
    if (head || eof) return ::media::Result<bool>::success(false);
    const char* port = input == Input::Video ? "video" : "audio";
    auto* channel = context.findInputChannel(nodeId(), port);
    if (!channel) return ::media::Result<bool>::failure(
        ::media::ErrorInfo::notInitialized("A/V scheduler input is missing"));
    auto popped = tryPopInputOptional(context, port);
    if (!popped) return ::media::Result<bool>::failure(popped.error());
    if (!popped.value()) {
        if (channel->closed()) {
            if (input == Input::Video) m_videoEof = true;
            else m_audioEof = true;
            if (!m_terminal) {
                m_terminal = makeMediaBufferRef<MediaControlBuffer>(
                    MediaControlBufferKind::Eof);
            }
            return ::media::Result<bool>::success(true);
        }
        return ::media::Result<bool>::success(false);
    }
    auto parsed = MediaAvSchedulerHead::parse(
        std::move(*popped.value()), input == Input::Video
            ? MediaScheduledStream::Video : MediaScheduledStream::Audio);
    if (!parsed) return ::media::Result<bool>::failure(parsed.error());
    head = std::move(parsed).value();
    if (head->kind() != MediaAvSchedulerHeadKind::Control) {
        logFirstMediaHead(input);
    }
    return ::media::Result<bool>::success(true);
}

void MediaAvOutputSchedulerNode::logFirstMediaHead(Input input)
{
    auto& emitted = input == Input::Video
        ? m_firstVideoHeadDiagnosticEmitted
        : m_firstAudioHeadDiagnosticEmitted;
    if (emitted || !m_group) return;
    const auto& head = input == Input::Video ? m_videoHead : m_audioHead;
    if (!head || head->kind() == MediaAvSchedulerHeadKind::Control) return;

    const auto dispatch = head->canonicalDispatchTime();
    const auto targetMaster = m_group->mapCanonicalToMaster(
        head->canonicalPresentation());
    const auto now = m_group->clock()->now();
    std::ostringstream out;
    out << "av_scheduler_trace stage=first_"
        << (input == Input::Video ? "video" : "audio")
        << "_head generation=" << head->generation()
        << " canonical_target_ns=" << head->canonicalPresentation().nanoseconds()
        << " canonical_dispatch_ns="
        << (dispatch ? std::to_string(dispatch.value().nanoseconds()) : "error")
        << " target_master_ns="
        << (targetMaster ? std::to_string(targetMaster.value().nanoseconds()) : "error");
    if (dispatch) {
        const auto dispatchMaster = m_group->mapCanonicalToMaster(dispatch.value());
        out << " dispatch_master_ns="
            << (dispatchMaster
                    ? std::to_string(dispatchMaster.value().nanoseconds())
                    : "error");
    } else {
        out << " dispatch_master_ns=error";
    }
    out << " now_ns="
        << (now ? std::to_string(now.value().nanoseconds()) : "error");
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            out.str());
    emitted = true;
}

void MediaAvOutputSchedulerNode::logMissingMediaWait()
{
    if (!m_group) return;
    const std::optional<Input> missing =
        !m_videoHead && !m_videoEof ? std::optional<Input>(Input::Video) :
        !m_audioHead && !m_audioEof ? std::optional<Input>(Input::Audio) :
        std::nullopt;
    if (!missing || m_missingMediaWait == missing) return;

    const auto& present = *missing == Input::Video ? m_audioHead : m_videoHead;
    const auto now = m_group->clock()->now();
    std::ostringstream out;
    out << "av_scheduler_trace stage=missing_media_wait missing="
        << (*missing == Input::Video ? "video" : "audio")
        << " deadline_ns=none now_ns="
        << (now ? std::to_string(now.value().nanoseconds()) : "error");
    if (present && present->kind() != MediaAvSchedulerHeadKind::Control) {
        const auto dispatch = present->canonicalDispatchTime();
        const auto targetMaster = m_group->mapCanonicalToMaster(
            present->canonicalPresentation());
        out << " present_generation=" << present->generation()
            << " present_canonical_target_ns="
            << present->canonicalPresentation().nanoseconds()
            << " present_canonical_dispatch_ns="
            << (dispatch ? std::to_string(dispatch.value().nanoseconds()) : "error")
            << " present_target_master_ns="
            << (targetMaster
                    ? std::to_string(targetMaster.value().nanoseconds())
                    : "error");
        if (dispatch) {
            const auto dispatchMaster = m_group->mapCanonicalToMaster(
                dispatch.value());
            out << " present_dispatch_master_ns="
                << (dispatchMaster
                        ? std::to_string(dispatchMaster.value().nanoseconds())
                        : "error");
        } else {
            out << " present_dispatch_master_ns=error";
        }
    }
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            out.str());
    m_missingMediaWait = missing;
}

::media::Result<std::optional<MediaAvOutputSchedulerNode::Input>>
MediaAvOutputSchedulerNode::arbitrateControlHeads()
{
    const auto readKind = [](const std::optional<MediaAvSchedulerHead>& head)
        -> ::media::Result<std::optional<MediaControlBufferKind>> {
        if (!head || head->kind() != MediaAvSchedulerHeadKind::Control) {
            return ::media::Result<std::optional<MediaControlBufferKind>>::success(
                std::nullopt);
        }
        const auto* control = dynamic_cast<const MediaControlBuffer*>(
            head->buffer().get());
        if (!control) {
            return ::media::Result<std::optional<MediaControlBufferKind>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V scheduler control head has an invalid typed payload"));
        }
        return ::media::Result<std::optional<MediaControlBufferKind>>::success(
            control->controlKind());
    };

    auto videoKind = readKind(m_videoHead);
    auto audioKind = readKind(m_audioHead);
    if (!videoKind || !audioKind) {
        auto error = !videoKind ? videoKind.error() : audioKind.error();
        cancelPendingOutputTransfer();
        clearSchedulingState();
        return ::media::Result<std::optional<Input>>::failure(error);
    }
    const bool hasUnknown =
        videoKind.value() == MediaControlBufferKind::Unknown ||
        audioKind.value() == MediaControlBufferKind::Unknown;
    const bool hasAbort =
        videoKind.value() == MediaControlBufferKind::Abort ||
        audioKind.value() == MediaControlBufferKind::Abort;
    if (hasUnknown) {
        if (hasAbort && m_group) {
            m_group->markAborted();
        }
        cancelPendingOutputTransfer();
        clearSchedulingState();
        return ::media::Result<std::optional<Input>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler received an unknown control kind"));
    }
    if (!videoKind.value() && !audioKind.value()) {
        return ::media::Result<std::optional<Input>>::success(std::nullopt);
    }
    if (!videoKind.value()) {
        return ::media::Result<std::optional<Input>>::success(Input::Audio);
    }
    if (!audioKind.value()) {
        return ::media::Result<std::optional<Input>>::success(Input::Video);
    }
    const auto priority = [](MediaControlBufferKind kind) noexcept
        -> std::optional<int> {
        switch (kind) {
        case MediaControlBufferKind::Abort: return 3;
        case MediaControlBufferKind::Flush: return 2;
        case MediaControlBufferKind::Eof: return 1;
        case MediaControlBufferKind::Unknown: return std::nullopt;
        }
        return std::nullopt;
    };
    const auto videoPriority = priority(*videoKind.value());
    const auto audioPriority = priority(*audioKind.value());
    if (!videoPriority || !audioPriority) {
        cancelPendingOutputTransfer();
        clearSchedulingState();
        return ::media::Result<std::optional<Input>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler received an unsupported control kind"));
    }
    if (*videoPriority != *audioPriority) {
        return ::media::Result<std::optional<Input>>::success(
            *videoPriority > *audioPriority ? Input::Video : Input::Audio);
    }
    return ::media::Result<std::optional<Input>>::success(
        m_nextEqualTimeVideo ? Input::Video : Input::Audio);
}

::media::Result<bool> MediaAvOutputSchedulerNode::preflightGenerations()
{
    bool discardedOldHead = false;
    const auto inspect = [&](std::optional<MediaAvSchedulerHead>& head,
                             bool video) -> ::media::Result<bool> {
        if (!head || head->kind() == MediaAvSchedulerHeadKind::Control) {
            return ::media::Result<bool>::success(false);
        }
        auto disposition = m_group->observeGeneration(head->generation());
        if (!disposition) {
            return ::media::Result<bool>::failure(disposition.error());
        }
        if (disposition.value() ==
            MediaAvSyncGroupRuntime::GenerationDisposition::ReacquisitionRequired) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V scheduler requires explicit generation reacquisition"));
        }
        if (disposition.value() ==
            MediaAvSyncGroupRuntime::GenerationDisposition::Old) {
            head.reset();
            if (video) {
                m_heldControllerSequence.reset();
            }
            return ::media::Result<bool>::success(true);
        }
        return ::media::Result<bool>::success(false);
    };
    auto video = inspect(m_videoHead, true);
    if (!video) return video;
    discardedOldHead = video.value();
    auto audio = inspect(m_audioHead, false);
    if (!audio) return audio;
    return ::media::Result<bool>::success(
        discardedOldHead || audio.value());
}

::media::Result<std::optional<MediaAvOutputSchedulerNode::Input>>
MediaAvOutputSchedulerNode::selectMediaHead() const
{
    const bool videoControl = m_videoHead &&
        m_videoHead->kind() == MediaAvSchedulerHeadKind::Control;
    const bool audioControl = m_audioHead &&
        m_audioHead->kind() == MediaAvSchedulerHeadKind::Control;
    if (videoControl || audioControl) {
        return ::media::Result<std::optional<Input>>::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler media selection received a control head"));
    }
    if ((!m_videoHead && !m_videoEof) || (!m_audioHead && !m_audioEof)) {
        return ::media::Result<std::optional<Input>>::success(std::nullopt);
    }
    if (!m_videoHead) return ::media::Result<std::optional<Input>>::success(
        m_audioHead ? std::optional<Input>(Input::Audio) : std::nullopt);
    if (!m_audioHead) return ::media::Result<std::optional<Input>>::success(Input::Video);
    const auto videoGeneration = m_videoHead->generation();
    const auto audioGeneration = m_audioHead->generation();
    if (videoGeneration != audioGeneration) {
        return ::media::Result<std::optional<Input>>::success(
            videoGeneration < audioGeneration ? Input::Video : Input::Audio);
    }
    auto videoDispatch = m_videoHead->canonicalDispatchTime();
    auto audioDispatch = m_audioHead->canonicalDispatchTime();
    if (!videoDispatch) {
        return ::media::Result<std::optional<Input>>::failure(
            videoDispatch.error());
    }
    if (!audioDispatch) {
        return ::media::Result<std::optional<Input>>::failure(
            audioDispatch.error());
    }
    auto videoTime = m_group->mapCanonicalToMaster(videoDispatch.value());
    auto audioTime = m_group->mapCanonicalToMaster(audioDispatch.value());
    if (!videoTime) return ::media::Result<std::optional<Input>>::failure(videoTime.error());
    if (!audioTime) return ::media::Result<std::optional<Input>>::failure(audioTime.error());
    if (videoTime.value() != audioTime.value()) {
        return ::media::Result<std::optional<Input>>::success(
            videoTime.value() < audioTime.value() ? Input::Video : Input::Audio);
    }
    return ::media::Result<std::optional<Input>>::success(
        m_nextEqualTimeVideo ? Input::Video : Input::Audio);
}

::media::Result<MediaNodeProcessResult>
MediaAvOutputSchedulerNode::processSelected(
    MediaGraphExecutionContext& context, Input input)
{
    auto& head = input == Input::Video ? m_videoHead : m_audioHead;
    if (head->kind() == MediaAvSchedulerHeadKind::Control) {
        return processTerminal(context, input);
    }
    return input == Input::Video ? processVideo(context) : processAudio(context);
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::processVideo(
    MediaGraphExecutionContext& context)
{
    auto now = m_group->clock()->now();
    if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    auto decisionHorizon = now.value().checkedAdd(m_transportLead);
    if (!decisionHorizon) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            decisionHorizon.error());
    }
    auto target = m_group->mapCanonicalToMaster(m_videoHead->canonicalPresentation());
    if (!target) return ::media::Result<MediaNodeProcessResult>::failure(target.error());
    auto canonicalDispatch = m_videoHead->canonicalDispatchTime();
    if (!canonicalDispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            canonicalDispatch.error());
    }
    auto dispatch = m_group->mapCanonicalToMaster(canonicalDispatch.value());
    if (!dispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(dispatch.error());
    }
    auto emit = dispatch.value().checkedSubtract(m_transportLead);
    if (!emit) {
        return ::media::Result<MediaNodeProcessResult>::failure(emit.error());
    }
    auto epoch = m_group->playbackEpoch();
    if (!epoch) return ::media::Result<MediaNodeProcessResult>::failure(epoch.error());
    auto generation = m_group->observeGeneration(m_videoHead->generation());
    if (!generation) return ::media::Result<MediaNodeProcessResult>::failure(
        generation.error());
    if (generation.value() == MediaAvSyncGroupRuntime::GenerationDisposition::Old) {
        m_videoHead.reset();
        m_heldControllerSequence.reset();
        return processProgress();
    }
    if (generation.value() ==
        MediaAvSyncGroupRuntime::GenerationDisposition::ReacquisitionRequired) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Video scheduler requested explicit generation reacquisition"));
    }

    const auto* repeat = m_videoHead->repeat();
    if (repeat && (!m_lastDisplayedVideoClone ||
                   !m_lastDisplayedVideoSequence ||
                   !m_lastDisplayedVideoMasterTime)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Video repeat has no previously displayed frame"));
    }
    if (!m_heldControllerSequence && !m_nextControllerSequence) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video controller sequence is exhausted"));
    }
    const std::uint64_t controllerSequence = m_heldControllerSequence
        ? *m_heldControllerSequence : *m_nextControllerSequence;
    if (!m_heldControllerSequence) {
        if (*m_nextControllerSequence ==
            std::numeric_limits<std::uint64_t>::max()) {
            m_nextControllerSequence.reset();
        } else {
            ++*m_nextControllerSequence;
        }
    }
    MediaAvSyncResult<MediaVideoSyncDecision> decision = repeat
        ? m_videoController->update(MediaVideoRepeatRequest{
              dispatch.value(),
              target.value(),
              *m_lastDisplayedVideoMasterTime,
              decisionHorizon.value(), epoch.value().generation,
              controllerSequence, now.value()})
        : m_videoController->update(MediaVideoFrameMeasurement{
              dispatch.value(), target.value(), decisionHorizon.value(),
              epoch.value().generation,
              controllerSequence,
              m_videoHead->canonical()->media()->isKeyFrame(),
              now.value()});
    if (!decision) return ::media::Result<MediaNodeProcessResult>::failure(
        decision.error().toErrorInfo());
    if (decision.value().kind() == MediaVideoSyncDecisionKind::Hold) {
        if (!decision.value().recheckAtMasterTime()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError("Hold decision has no deadline"));
        }
        m_heldControllerSequence = controllerSequence;
        auto recheck = decision.value().recheckAtMasterTime()->checkedSubtract(
            m_transportLead);
        if (!recheck) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                recheck.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_groupKey, recheck.value()));
    }
    m_heldControllerSequence.reset();
    const auto kind = decision.value().kind();
    if (kind == MediaVideoSyncDecisionKind::Drop ||
        kind == MediaVideoSyncDecisionKind::DropOldGeneration ||
        kind == MediaVideoSyncDecisionKind::NoAction) {
        m_videoHead.reset();
        m_nextEqualTimeVideo = false;
        return processProgress();
    }
    if (kind == MediaVideoSyncDecisionKind::Reacquire) {
        if (!decision.value().reacquisitionCause()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError(
                    "Video reacquisition decision has no typed cause"));
        }
        const MediaVideoReacquisitionCause cause =
            *decision.value().reacquisitionCause();
        const MediaChannel* videoInput =
            context.findInputChannel(nodeId(), "video");
        const MediaChannel* audioInput =
            context.findInputChannel(nodeId(), "audio");
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "av_scheduler_trace stage=reacquire cause=" +
                std::string(reacquisitionCauseName(cause)) +
                " generation=" +
                std::to_string(decision.value().generation()) +
                " sequence=" +
                std::to_string(decision.value().sequence()) +
                " target_master_ns=" +
                std::to_string(target.value().nanoseconds()) +
                " dispatch_master_ns=" +
                std::to_string(dispatch.value().nanoseconds()) +
                " decision_horizon_ns=" +
                std::to_string(decisionHorizon.value().nanoseconds()) +
                " phase_error_ns=" +
                std::to_string(decision.value().phaseError().nanoseconds()) +
                " recovery_actions=" +
                std::to_string(
                    decision.value().consecutiveRecoveryActions()) +
                " video_queued=" +
                std::to_string(videoInput ? videoInput->size() : 0) +
                " audio_queued=" +
                std::to_string(audioInput ? audioInput->size() : 0));
        const MediaAvReacquisitionReason reason =
            cause == MediaVideoReacquisitionCause::RecoveryBudgetExhausted
            ? MediaAvReacquisitionReason::RecoveryBudgetExhausted
            : cause == MediaVideoReacquisitionCause::GenerationMismatch
            ? MediaAvReacquisitionReason::FutureGeneration
            : MediaAvReacquisitionReason::HardDiscontinuity;
        auto requested = m_group->requestReacquisition(MediaAvReacquisitionRequest{
            cause == MediaVideoReacquisitionCause::GenerationMismatch
                ? decision.value().generation()
                : epoch.value().generation,
            reason});
        if (!requested) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                requested.error());
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                std::string("A/V scheduler requires explicit clock reacquisition: ") +
                reacquisitionCauseName(cause)));
    }

    auto prepared = repeat
        ? MediaAvScheduledOutputBuilder::repeatedVideo(
              *repeat, m_lastDisplayedVideoClone,
              *m_lastDisplayedVideoSequence, target.value(), dispatch.value(),
              emit.value(), kind)
        : MediaAvScheduledOutputBuilder::canonicalVideo(
              *m_videoHead, target.value(), dispatch.value(), emit.value(), kind);
    if (!prepared) {
        return ::media::Result<MediaNodeProcessResult>::failure(prepared.error());
    }
    const auto sourceSequence = repeat
        ? *m_lastDisplayedVideoSequence
        : m_videoHead->canonical()->sourceSequence();
    MediaAvSchedulerPendingCommit commit{
        MediaAvSchedulerCommitKind::Video,
        std::move(prepared.value().displayedVideoClone),
        sourceSequence,
        target.value(),
        false};
    return emitWithCommit(
        context, prepared.value().output, std::move(commit));
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::processAudio(
    MediaGraphExecutionContext& context)
{
    const auto* unit = m_audioHead->canonical();
    auto generation = m_group->observeGeneration(unit->generation());
    if (!generation) return ::media::Result<MediaNodeProcessResult>::failure(
        generation.error());
    if (generation.value() == MediaAvSyncGroupRuntime::GenerationDisposition::Old) {
        m_audioHead.reset();
        return processProgress();
    }
    if (generation.value() ==
        MediaAvSyncGroupRuntime::GenerationDisposition::ReacquisitionRequired) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Audio scheduler requested explicit generation reacquisition"));
    }
    auto target = m_group->mapCanonicalToMaster(unit->canonicalPresentation());
    if (!target) return ::media::Result<MediaNodeProcessResult>::failure(target.error());
    auto canonicalDispatch = unit->canonicalDispatch();
    if (!canonicalDispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            canonicalDispatch.error());
    }
    auto dispatch = m_group->mapCanonicalToMaster(canonicalDispatch.value());
    if (!dispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(dispatch.error());
    }
    auto emit = dispatch.value().checkedSubtract(m_transportLead);
    if (!emit) {
        return ::media::Result<MediaNodeProcessResult>::failure(emit.error());
    }
    auto now = m_group->clock()->now();
    if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    if (emit.value() > now.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_groupKey, emit.value()));
    }
    auto epoch = m_group->playbackEpoch();
    if (!epoch) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            epoch.error());
    }
    auto output = MediaAvScheduledOutputBuilder::audio(
        *unit, target.value(), dispatch.value(), emit.value());
    if (!output) {
        return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    }
    return emitWithCommit(
        context, output.value(),
        MediaAvSchedulerPendingCommit{MediaAvSchedulerCommitKind::Audio});
}

::media::Result<MediaNodeProcessResult>
MediaAvOutputSchedulerNode::processTerminal(
    MediaGraphExecutionContext& context, Input input)
{
    auto& head = input == Input::Video ? m_videoHead : m_audioHead;
    const auto* control = dynamic_cast<const MediaControlBuffer*>(
        head->buffer().get());
    if (control->controlKind() == MediaControlBufferKind::Unknown) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V scheduler received an unknown control kind"));
    }
    if (control->controlKind() != MediaControlBufferKind::Eof) {
        MediaBufferRef terminal = head->buffer();
        if (control->controlKind() == MediaControlBufferKind::Abort) {
            m_group->markAborted();
        } else if (control->controlKind() == MediaControlBufferKind::Flush) {
            auto epoch = m_group->playbackEpoch();
            if (!epoch) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    epoch.error());
            }
            auto requested = m_group->requestReacquisition(MediaAvReacquisitionRequest{
                epoch.value().generation,
                MediaAvReacquisitionReason::Flush});
            if (!requested) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    requested.error());
            }
        } else {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V scheduler received an unsupported control kind"));
        }
        return emitWithCommit(
            context, terminal,
            MediaAvSchedulerPendingCommit{
                MediaAvSchedulerCommitKind::Terminal, {}, {}, {}, true});
    }
    if (!m_terminal) m_terminal = head->buffer();
    head.reset();
    if (input == Input::Video) {
        m_videoEof = true;
        m_lastDisplayedVideoClone.reset();
        m_lastDisplayedVideoSequence.reset();
        m_lastDisplayedVideoMasterTime.reset();
        m_heldControllerSequence.reset();
    } else {
        m_audioEof = true;
    }
    if (!m_videoEof || !m_audioEof) return processProgress();
    return emitWithCommit(
        context, m_terminal,
        MediaAvSchedulerPendingCommit{
            MediaAvSchedulerCommitKind::Terminal, {}, {}, {}, true});
}

::media::Result<MediaNodeProcessResult>
MediaAvOutputSchedulerNode::emitWithCommit(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& output,
    MediaAvSchedulerPendingCommit commit)
{
    auto status = emitOutput(context, "scheduled", output);
    if (status) {
        return ::media::Result<MediaNodeProcessResult>::success(
            applyCommit(std::move(commit)));
    }
    if (status.error().code == ::media::ErrorCode::WouldBlock) {
        m_pendingCommit = std::move(commit);
        return processProgress(std::move(status));
    }
    return ::media::Result<MediaNodeProcessResult>::failure(status.error());
}

MediaNodeProcessResult MediaAvOutputSchedulerNode::applyCommit(
    MediaAvSchedulerPendingCommit commit)
{
    if (commit.kind == MediaAvSchedulerCommitKind::Video) {
        m_videoHead.reset();
        m_nextEqualTimeVideo = false;
        if (commit.displayedVideoClone) {
            m_lastDisplayedVideoClone = std::move(commit.displayedVideoClone);
        }
        m_lastDisplayedVideoSequence = commit.displayedVideoSequence;
        m_lastDisplayedVideoMasterTime = commit.displayedVideoMasterTime;
    } else if (commit.kind == MediaAvSchedulerCommitKind::Audio) {
        m_audioHead.reset();
        m_nextEqualTimeVideo = true;
    } else {
        m_videoHead.reset();
        m_audioHead.reset();
        m_terminal.reset();
        m_lastDisplayedVideoClone.reset();
        m_lastDisplayedVideoSequence.reset();
        m_lastDisplayedVideoMasterTime.reset();
        m_heldControllerSequence.reset();
    }
    return commit.terminalFinishes ? MediaNodeProcessResult::finished()
                                   : MediaNodeProcessResult::progress();
}

::media::Status MediaAvOutputSchedulerNode::flush(
    MediaGraphExecutionContext&)
{
    if (!m_group) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler flush requires a configured sync group"));
    }
    auto epoch = m_group->playbackEpoch();
    if (!epoch) return ::media::Status::failure(epoch.error());
    auto requested = m_group->requestReacquisition(MediaAvReacquisitionRequest{
        epoch.value().generation, MediaAvReacquisitionReason::Flush});
    if (!requested) return requested;
    cancelPendingOutputTransfer();
    clearSchedulingState();
    return ::media::Status::success();
}

::media::Status MediaAvOutputSchedulerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvOutputSchedulerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    if (m_group) m_group->markAborted();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvOutputSchedulerNode::resetState() noexcept
{
    clearSchedulingState();
    m_groupKey.reset();
    m_group.reset();
}

void MediaAvOutputSchedulerNode::clearSchedulingState() noexcept
{
    m_videoController.reset();
    m_videoHead.reset();
    m_audioHead.reset();
    m_terminal.reset();
    m_lastDisplayedVideoClone.reset();
    m_lastDisplayedVideoSequence.reset();
    m_lastDisplayedVideoMasterTime.reset();
    m_heldControllerSequence.reset();
    m_pendingCommit.reset();
    m_nextControllerSequence = 1;
    m_videoEof = false;
    m_audioEof = false;
    m_nextEqualTimeVideo = false;
    m_firstVideoHeadDiagnosticEmitted = false;
    m_firstAudioHeadDiagnosticEmitted = false;
    m_missingMediaWait.reset();
}

} // namespace media::ffmpeg::graph
