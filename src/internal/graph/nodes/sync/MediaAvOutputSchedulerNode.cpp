#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaOutputSchedule.h"
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

bool reacquisitionInProgress(MediaAvReacquisitionPhase phase) noexcept
{
    return phase == MediaAvReacquisitionPhase::Purging ||
        phase == MediaAvReacquisitionPhase::Acquiring ||
        phase == MediaAvReacquisitionPhase::ReadyForActivation ||
        phase == MediaAvReacquisitionPhase::Activating ||
        phase == MediaAvReacquisitionPhase::Publishing;
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
    , m_generationSession(
          std::shared_ptr<MediaAvSchedulerGenerationState>(
              new MediaAvSchedulerGenerationState()))
    , m_generationData(m_generationSession->current())
    , m_generationState(
          std::make_shared<MediaProtocolOutputGenerationState>(
              std::string(generationPurgeIdentity()),
              m_generationSession))
    , m_videoControllerFactory(std::move(controllerFactory))
{
}

MediaNodeKind MediaAvOutputSchedulerNode::staticKind() noexcept
{
    return MediaNodeKind::AvOutputScheduler;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaAvOutputSchedulerNode::generationPurgeTarget() const noexcept
{
    return m_generationState;
}

::media::Status MediaAvOutputSchedulerNode::start(
    MediaGraphExecutionContext& context)
{
    auto reset = resetState();
    if (!reset) return reset;
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
    if (m_generationData->videoController) return ::media::Status::success();
    if (!m_videoControllerFactory) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires a video controller factory"));
    }
    if (!m_group) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires an active sync group"));
    }
    auto activation =
        m_generationState->permitAuthorityActivation(*m_group);
    if (!activation) {
        return ::media::Status::failure(activation.error());
    }
    auto controller = m_videoControllerFactory(
        m_group->plan(), activation.value().epoch.generation);
    if (!controller) {
        return ::media::Status::failure(controller.error().toErrorInfo());
    }
    m_generationData->videoController = std::make_unique<MediaVideoSyncController>(
        std::move(controller).value());
    m_generationData->activeGeneration = activation.value().epoch.generation;
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::process(
    MediaGraphExecutionContext& context)
{
    refreshGenerationSession();
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
    if (!m_generationData->videoController &&
        reacquisitionInProgress(
            m_group->reacquisitionSnapshot().phase)) {
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
    if (m_generationData->completedCommitResult) {
        auto completed = *m_generationData->completedCommitResult;
        m_generationData->completedCommitResult.reset();
        return ::media::Result<MediaNodeProcessResult>::success(completed);
    }
    if (m_generationData->pendingCommit) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler retained commit lost its output transfer transaction"));
    }
    if (!m_group || !m_generationData->videoController) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler has no active scheduling state"));
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
    if (m_generationData->videoEof && m_generationData->audioEof && !m_generationData->videoHead && !m_generationData->audioHead && m_generationData->terminal) {
        return emitWithCommit(
            context, m_generationData->terminal,
            MediaAvSchedulerPendingCommit{
                MediaAvSchedulerCommitKind::Terminal, {}, {}, {}, true});
    }
    auto selected = selectMediaHead();
    if (!selected) return ::media::Result<MediaNodeProcessResult>::failure(selected.error());
    if (!selected.value()) logMissingMediaWait();
    return selected.value() ? processSelected(context, *selected.value())
                            : processWaiting();
}

bool MediaAvOutputSchedulerNode::pendingOutputIsCurrent(
    const MediaBufferRef& buffer) const noexcept
{
    if (!m_generationData->pendingCommit ||
        !m_generationData->pendingCommit->generation) {
        return false;
    }
    if (const auto* scheduled =
            dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get())) {
        return scheduled->generation() ==
            *m_generationData->pendingCommit->generation;
    }
    return m_generationData->activeGeneration &&
        *m_generationData->pendingCommit->generation ==
            *m_generationData->activeGeneration;
}

::media::Result<bool> MediaAvOutputSchedulerNode::fillHead(
    MediaGraphExecutionContext& context, Input input)
{
    auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
    const bool eof = input == Input::Video ? m_generationData->videoEof : m_generationData->audioEof;
    if (head || eof) return ::media::Result<bool>::success(false);
    const char* port = input == Input::Video ? "video" : "audio";
    auto* channel = context.findInputChannel(nodeId(), port);
    if (!channel) return ::media::Result<bool>::failure(
        ::media::ErrorInfo::notInitialized("A/V scheduler input is missing"));
    auto popped = tryPopInputOptional(context, port);
    if (!popped) return ::media::Result<bool>::failure(popped.error());
    if (!popped.value()) {
        if (channel->closed()) {
            if (input == Input::Video) m_generationData->videoEof = true;
            else m_generationData->audioEof = true;
            if (!m_generationData->terminal) {
                m_generationData->terminal = makeMediaBufferRef<MediaControlBuffer>(
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
        ? m_generationData->firstVideoHeadDiagnosticEmitted
        : m_generationData->firstAudioHeadDiagnosticEmitted;
    if (emitted || !m_group) return;
    const auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
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
        !m_generationData->videoHead && !m_generationData->videoEof ? std::optional<Input>(Input::Video) :
        !m_generationData->audioHead && !m_generationData->audioEof ? std::optional<Input>(Input::Audio) :
        std::nullopt;
    if (!missing || m_generationData->missingMediaWait == missing) return;

    const auto& present = *missing == Input::Video ? m_generationData->audioHead : m_generationData->videoHead;
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
    m_generationData->missingMediaWait = missing;
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

    auto videoKind = readKind(m_generationData->videoHead);
    auto audioKind = readKind(m_generationData->audioHead);
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
        m_generationData->nextEqualTimeVideo ? Input::Video : Input::Audio);
}

::media::Result<bool> MediaAvOutputSchedulerNode::preflightGenerations()
{
    bool discardedOldHead = false;
    const auto inspect = [&](std::optional<MediaAvSchedulerHead>& head,
                             bool video) -> ::media::Result<bool> {
        if (!head || head->kind() == MediaAvSchedulerHeadKind::Control) {
            return ::media::Result<bool>::success(false);
        }
        if (!m_generationData->activeGeneration) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V scheduler generation preflight has no active generation"));
        }
        if (head->generation() > *m_generationData->activeGeneration) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V scheduler requires explicit generation reacquisition"));
        }
        if (head->generation() < *m_generationData->activeGeneration) {
            head.reset();
            if (video) {
                m_generationData->heldControllerSequence.reset();
            }
            return ::media::Result<bool>::success(true);
        }
        return ::media::Result<bool>::success(false);
    };
    auto video = inspect(m_generationData->videoHead, true);
    if (!video) return video;
    discardedOldHead = video.value();
    auto audio = inspect(m_generationData->audioHead, false);
    if (!audio) return audio;
    return ::media::Result<bool>::success(
        discardedOldHead || audio.value());
}

::media::Result<std::optional<MediaAvOutputSchedulerNode::Input>>
MediaAvOutputSchedulerNode::selectMediaHead() const
{
    const bool videoControl = m_generationData->videoHead &&
        m_generationData->videoHead->kind() == MediaAvSchedulerHeadKind::Control;
    const bool audioControl = m_generationData->audioHead &&
        m_generationData->audioHead->kind() == MediaAvSchedulerHeadKind::Control;
    if (videoControl || audioControl) {
        return ::media::Result<std::optional<Input>>::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler media selection received a control head"));
    }
    if ((!m_generationData->videoHead && !m_generationData->videoEof) || (!m_generationData->audioHead && !m_generationData->audioEof)) {
        return ::media::Result<std::optional<Input>>::success(std::nullopt);
    }
    if (!m_generationData->videoHead) return ::media::Result<std::optional<Input>>::success(
        m_generationData->audioHead ? std::optional<Input>(Input::Audio) : std::nullopt);
    if (!m_generationData->audioHead) return ::media::Result<std::optional<Input>>::success(Input::Video);
    const auto videoGeneration = m_generationData->videoHead->generation();
    const auto audioGeneration = m_generationData->audioHead->generation();
    if (videoGeneration != audioGeneration) {
        return ::media::Result<std::optional<Input>>::success(
            videoGeneration < audioGeneration ? Input::Video : Input::Audio);
    }
    auto videoDispatch = m_generationData->videoHead->canonicalDispatchTime();
    auto audioDispatch = m_generationData->audioHead->canonicalDispatchTime();
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
        m_generationData->nextEqualTimeVideo ? Input::Video : Input::Audio);
}

::media::Result<MediaNodeProcessResult>
MediaAvOutputSchedulerNode::processSelected(
    MediaGraphExecutionContext& context, Input input)
{
    auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
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
    auto target = m_group->mapCanonicalToMaster(m_generationData->videoHead->canonicalPresentation());
    if (!target) return ::media::Result<MediaNodeProcessResult>::failure(target.error());
    auto canonicalDispatch = m_generationData->videoHead->canonicalDispatchTime();
    if (!canonicalDispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            canonicalDispatch.error());
    }
    auto dispatch = m_group->mapCanonicalToMaster(canonicalDispatch.value());
    if (!dispatch) {
        return ::media::Result<MediaNodeProcessResult>::failure(dispatch.error());
    }
    auto schedule = MediaOutputSchedule::create(
        target.value(), dispatch.value(), m_transportLead);
    if (!schedule) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            schedule.error());
    }
    if (!m_generationData->activeGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Video scheduler has no active generation"));
    }
    if (m_generationData->videoHead->generation() < *m_generationData->activeGeneration) {
        m_generationData->videoHead.reset();
        m_generationData->heldControllerSequence.reset();
        return processProgress();
    }
    if (m_generationData->videoHead->generation() > *m_generationData->activeGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Video scheduler requested explicit generation reacquisition"));
    }

    const auto* repeat = m_generationData->videoHead->repeat();
    if (repeat && (!m_generationData->lastDisplayedVideoClone ||
                   !m_generationData->lastDisplayedVideoSequence ||
                   !m_generationData->lastDisplayedVideoMasterTime)) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Video repeat has no previously displayed frame"));
    }
    if (!m_generationData->heldControllerSequence && !m_generationData->nextControllerSequence) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video controller sequence is exhausted"));
    }
    const std::uint64_t controllerSequence = m_generationData->heldControllerSequence
        ? *m_generationData->heldControllerSequence : *m_generationData->nextControllerSequence;
    if (!m_generationData->heldControllerSequence) {
        if (*m_generationData->nextControllerSequence ==
            std::numeric_limits<std::uint64_t>::max()) {
            m_generationData->nextControllerSequence.reset();
        } else {
            ++*m_generationData->nextControllerSequence;
        }
    }
    MediaAvSyncResult<MediaVideoSyncDecision> decision = repeat
        ? m_generationData->videoController->update(MediaVideoRepeatRequest{
              dispatch.value(),
              target.value(),
              *m_generationData->lastDisplayedVideoMasterTime,
              decisionHorizon.value(), *m_generationData->activeGeneration,
              controllerSequence, now.value()})
        : m_generationData->videoController->update(MediaVideoFrameMeasurement{
              dispatch.value(), target.value(), decisionHorizon.value(),
              *m_generationData->activeGeneration,
              controllerSequence,
              m_generationData->videoHead->canonical()->media()->isKeyFrame(),
              now.value()});
    if (!decision) return ::media::Result<MediaNodeProcessResult>::failure(
        decision.error().toErrorInfo());
    if (decision.value().kind() == MediaVideoSyncDecisionKind::Hold) {
        if (!decision.value().recheckAtMasterTime()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError("Hold decision has no deadline"));
        }
        m_generationData->heldControllerSequence = controllerSequence;
        auto recheck = decision.value().recheckAtMasterTime()->checkedSubtract(
            m_transportLead);
        if (!recheck) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                recheck.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_groupKey, recheck.value()));
    }
    m_generationData->heldControllerSequence.reset();
    const auto kind = decision.value().kind();
    if (kind == MediaVideoSyncDecisionKind::Drop ||
        kind == MediaVideoSyncDecisionKind::DropOldGeneration ||
        kind == MediaVideoSyncDecisionKind::NoAction) {
        m_generationData->videoHead.reset();
        m_generationData->nextEqualTimeVideo = false;
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
        auto requested = m_group->requestReacquisition(
            MediaAvReacquisitionRequest{
            cause == MediaVideoReacquisitionCause::GenerationMismatch
                ? decision.value().generation()
                : *m_generationData->activeGeneration,
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
              *repeat, m_generationData->lastDisplayedVideoClone,
              *m_generationData->lastDisplayedVideoSequence,
              schedule.value().presentation, schedule.value().dispatch,
              schedule.value().emit, kind)
        : MediaAvScheduledOutputBuilder::canonicalVideo(
              *m_generationData->videoHead, schedule.value().presentation,
              schedule.value().dispatch, schedule.value().emit, kind);
    if (!prepared) {
        return ::media::Result<MediaNodeProcessResult>::failure(prepared.error());
    }
    const auto sourceSequence = repeat
        ? *m_generationData->lastDisplayedVideoSequence
        : m_generationData->videoHead->canonical()->sourceSequence();
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
    const auto* unit = m_generationData->audioHead->canonical();
    if (!m_generationData->activeGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Audio scheduler has no active generation"));
    }
    if (unit->generation() < *m_generationData->activeGeneration) {
        m_generationData->audioHead.reset();
        return processProgress();
    }
    if (unit->generation() > *m_generationData->activeGeneration) {
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
    auto schedule = MediaOutputSchedule::create(
        target.value(), dispatch.value(), m_transportLead);
    if (!schedule) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            schedule.error());
    }
    auto now = m_group->clock()->now();
    if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    if (schedule.value().emit > now.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(
                *m_groupKey, schedule.value().emit));
    }
    auto output = MediaAvScheduledOutputBuilder::audio(
        *unit, schedule.value().presentation, schedule.value().dispatch,
        schedule.value().emit);
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
    auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
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
            if (!m_generationData->activeGeneration) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "A/V scheduler flush has no active generation"));
            }
            auto requested = m_group->requestReacquisition(
                MediaAvReacquisitionRequest{
                *m_generationData->activeGeneration,
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
    if (!m_generationData->terminal) m_generationData->terminal = head->buffer();
    head.reset();
    if (input == Input::Video) {
        m_generationData->videoEof = true;
        m_generationData->lastDisplayedVideoClone.reset();
        m_generationData->lastDisplayedVideoSequence.reset();
        m_generationData->lastDisplayedVideoMasterTime.reset();
        m_generationData->heldControllerSequence.reset();
    } else {
        m_generationData->audioEof = true;
    }
    if (!m_generationData->videoEof || !m_generationData->audioEof) return processProgress();
    return emitWithCommit(
        context, m_generationData->terminal,
        MediaAvSchedulerPendingCommit{
            MediaAvSchedulerCommitKind::Terminal, {}, {}, {}, true});
}

::media::Result<MediaNodeProcessResult>
MediaAvOutputSchedulerNode::emitWithCommit(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& output,
    MediaAvSchedulerPendingCommit commit)
{
    if (const auto* scheduled =
            dynamic_cast<const MediaScheduledAccessUnit*>(output.get())) {
        commit.generation = scheduled->generation();
    } else {
        commit.generation = m_generationData->activeGeneration;
    }
    m_generationData->pendingCommit = std::move(commit);
    auto status = emitOutput(context, "scheduled", output);
    if (status) {
        if (!m_generationData->completedCommitResult) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError(
                    "A/V scheduler accepted output without committing its generation state"));
        }
        auto completed = *m_generationData->completedCommitResult;
        m_generationData->completedCommitResult.reset();
        return ::media::Result<MediaNodeProcessResult>::success(completed);
    }
    if (status.error().code == ::media::ErrorCode::WouldBlock) {
        return processProgress(std::move(status));
    }
    m_generationData->pendingCommit.reset();
    return ::media::Result<MediaNodeProcessResult>::failure(status.error());
}

::media::Result<MediaOutputCommitReservation>
MediaAvOutputSchedulerNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    std::optional<std::uint64_t> generation;
    if (const auto* scheduled =
            dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get())) {
        generation = scheduled->generation();
    } else if (m_generationData->pendingCommit && m_generationData->pendingCommit->generation) {
        generation = m_generationData->pendingCommit->generation;
    }
    if (!m_group || !generation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "A/V scheduler commit requires a planned generation"));
    }
    auto reservation = m_generationState->reserveCommit(
        *m_group, *generation);
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(
            std::move(reservation).value()));
}

::media::Status MediaAvOutputSchedulerNode::commitReservedOutput(
    const MediaBufferRef&)
{
    if (!m_generationData->pendingCommit || !m_generationData->pendingCommit->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler output commit has no matching reserved generation transaction"));
    }
    m_generationData->completedCommitResult =
        applyCommit(std::move(*m_generationData->pendingCommit));
    m_generationData->pendingCommit.reset();
    return ::media::Status::success();
}

::media::Status MediaAvOutputSchedulerNode::cancelReservedOutput(
    const MediaBufferRef& buffer)
{
    if (!m_generationData->pendingCommit || !m_generationData->pendingCommit->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler cancelled output has no matching pending generation"));
    }
    if (const auto* scheduled =
            dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get());
        scheduled &&
        scheduled->generation() != *m_generationData->pendingCommit->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler cancelled output generation differs from its pending commit"));
    }
    m_generationData->pendingCommit.reset();
    m_generationData->completedCommitResult = MediaNodeProcessResult::progress();
    return ::media::Status::success();
}

MediaNodeProcessResult MediaAvOutputSchedulerNode::applyCommit(
    MediaAvSchedulerPendingCommit commit)
{
    if (commit.kind == MediaAvSchedulerCommitKind::Video) {
        m_generationData->videoHead.reset();
        m_generationData->nextEqualTimeVideo = false;
        if (commit.displayedVideoClone) {
            m_generationData->lastDisplayedVideoClone = std::move(commit.displayedVideoClone);
        }
        m_generationData->lastDisplayedVideoSequence = commit.displayedVideoSequence;
        m_generationData->lastDisplayedVideoMasterTime = commit.displayedVideoMasterTime;
    } else if (commit.kind == MediaAvSchedulerCommitKind::Audio) {
        m_generationData->audioHead.reset();
        m_generationData->nextEqualTimeVideo = true;
    } else {
        m_generationData->videoHead.reset();
        m_generationData->audioHead.reset();
        m_generationData->terminal.reset();
        m_generationData->lastDisplayedVideoClone.reset();
        m_generationData->lastDisplayedVideoSequence.reset();
        m_generationData->lastDisplayedVideoMasterTime.reset();
        m_generationData->heldControllerSequence.reset();
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
    auto reset = resetState();
    auto stopped = FFmpegNodeRuntime::stop(context);
    return reset ? stopped : reset;
}

void MediaAvOutputSchedulerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    if (m_group) m_group->markAborted();
    if (auto reset = resetState(); !reset) {
        cancelPendingOutputTransfer();
        m_generationData.reset();
    }
    FFmpegNodeRuntime::abort(context);
}

::media::Status MediaAvOutputSchedulerNode::resetState()
{
    cancelPendingOutputTransfer();
    m_groupKey.reset();
    m_group.reset();
    auto reset = m_generationState->resetLifecycle();
    if (!reset) return reset;
    refreshGenerationSession();
    return ::media::Status::success();
}

void MediaAvOutputSchedulerNode::refreshGenerationSession() noexcept
{
    m_generationData = m_generationSession->current();
}

void MediaAvOutputSchedulerNode::clearSchedulingState() noexcept
{
    m_generationData->videoController.reset();
    m_generationData->activeGeneration.reset();
    m_generationData->videoHead.reset();
    m_generationData->audioHead.reset();
    m_generationData->terminal.reset();
    m_generationData->lastDisplayedVideoClone.reset();
    m_generationData->lastDisplayedVideoSequence.reset();
    m_generationData->lastDisplayedVideoMasterTime.reset();
    m_generationData->heldControllerSequence.reset();
    m_generationData->pendingCommit.reset();
    m_generationData->completedCommitResult.reset();
    m_generationData->nextControllerSequence = 1;
    m_generationData->videoEof = false;
    m_generationData->audioEof = false;
    m_generationData->nextEqualTimeVideo = false;
    m_generationData->firstVideoHeadDiagnosticEmitted = false;
    m_generationData->firstAudioHeadDiagnosticEmitted = false;
    m_generationData->missingMediaWait.reset();
}

} // namespace media::ffmpeg::graph
