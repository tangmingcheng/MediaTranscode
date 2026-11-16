#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaOutputSchedule.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"

#include <iomanip>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

bool reacquisitionInProgress(MediaAvReacquisitionPhase phase) noexcept
{
    return phase == MediaAvReacquisitionPhase::Purging ||
        phase == MediaAvReacquisitionPhase::Acquiring ||
        phase == MediaAvReacquisitionPhase::ReadyForActivation ||
        phase == MediaAvReacquisitionPhase::Activating ||
        phase == MediaAvReacquisitionPhase::Publishing;
}

} // namespace

MediaAvOutputSchedulerNode::MediaAvOutputSchedulerNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvOutputSchedulerNode")
    , m_generationSession(
          std::shared_ptr<MediaAvSchedulerGenerationState>(
              new MediaAvSchedulerGenerationState()))
    , m_generationData(m_generationSession->current())
    , m_generationState(
          std::make_shared<MediaProtocolOutputGenerationState>(
              std::string(generationPurgeIdentity()),
              m_generationSession))
    , m_outputAuthority(std::move(authority))
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
    auto transportLead = requiredPositiveInt64NodeOption(
        nodeOptions(context), "MediaAvOutputSchedulerNode",
        "av_scheduler.transport_lead_ns");
    if (!transportLead) return ::media::Status::failure(transportLead.error());
    m_transportLead = MediaRunningTime::fromNanoseconds(transportLead.value());
    auto preparationLead = requiredPositiveInt64NodeOption(
        nodeOptions(context), "MediaAvOutputSchedulerNode", "av_scheduler.protocol_preparation_lead_ns");
    if (!preparationLead) return ::media::Status::failure(preparationLead.error());
    m_protocolPreparationLead = MediaRunningTime::fromNanoseconds(preparationLead.value());
    auto scheduling = requiredNodeOption(nodeOptions(context), "MediaAvOutputSchedulerNode", "av_scheduler.submission");
    if (!scheduling) return ::media::Status::failure(scheduling.error());
    if (scheduling.value() != "encoded_access_unit")
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("A/V scheduler requires encoded access-unit submission"));
    m_group = context.findAvSyncGroup(*m_groupKey);
    if (!m_group) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires a registered sync group"));
    if (!m_outputAuthority ||
        m_outputAuthority->streamSet() !=
            MediaTranscodeStreamSet::AudioVideo) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler requires compiler-injected output authority"));
    }
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
    if (m_generationData->activeGeneration) return ::media::Status::success();
    if (!m_group) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "MediaAvOutputSchedulerNode requires an active sync group"));
    }
    auto activation =
        m_generationState->permitAuthorityActivation(*m_outputAuthority);
    if (!activation) {
        return ::media::Status::failure(activation.error());
    }
    m_generationData->activeGeneration =
        activation.value().activation.generation;
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
    if (m_group->plan().domainRole != MediaAvSyncDomainRole::ContinuousOutput) {
        auto arbitration = m_group->reserveGenerationArbitration();
        if (!arbitration) {
            return ::media::Result<MediaNodeProcessResult>::failure(arbitration.error());
        }
        const auto phase = arbitration.value().reacquisition().phase;
        if (reacquisitionInProgress(phase)) {
            // Purge callbacks can still replace the session in Purging. Only
            // consume after every participant acknowledged, under the same
            // arbitration that prevents another transition from beginning.
            if (phase != MediaAvReacquisitionPhase::Acquiring &&
                phase != MediaAvReacquisitionPhase::ReadyForActivation) {
                return processWaiting();
            }
            auto mutation = m_generationState->reserveSessionMutation();
            refreshGenerationSession();
            auto video = fillHead(context, Input::Video);
            if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
            auto audio = fillHead(context, Input::Audio);
            if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
            auto retired = preflightGenerations(
                arbitration.value().reacquisition(), arbitration.value().epoch());
            if (!retired) return ::media::Result<MediaNodeProcessResult>::failure(retired.error());
            // Target media and ordered controls stay in the existing bounded
            // heads. They acquire no publication authority while waiting.
            return video.value() || audio.value() || retired.value()
                ? processProgress() : processWaiting();
        }
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
    if (!m_group || !m_generationData->activeGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V scheduler has no active scheduling state"));
    }
    auto video = fillHead(context, Input::Video);
    if (!video) return ::media::Result<MediaNodeProcessResult>::failure(video.error());
    auto audio = fillHead(context, Input::Audio);
    if (!audio) return ::media::Result<MediaNodeProcessResult>::failure(audio.error());
    observeMissingMediaWait();
    auto control = arbitrateControlHeads();
    if (!control) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            control.error());
    }
    if (control.value()) {
        return processTerminal(context, *control.value());
    }
    const auto generationPreflight = [&]() -> ::media::Result<bool> {
        if (m_group->plan().domainRole == MediaAvSyncDomainRole::ContinuousOutput) {
            // The planner's independent output domain has an initial-only
            // epoch service, with no source reacquisition coordinator.
            return preflightGenerations(
                {MediaAvReacquisitionPhase::Inactive, std::nullopt, std::nullopt},
                m_group->epochTransitionSnapshot());
        }
        auto arbitration = m_group->reserveGenerationArbitration();
        if (!arbitration) return ::media::Result<bool>::failure(arbitration.error());
        return preflightGenerations(
            arbitration.value().reacquisition(), arbitration.value().epoch());
    }();
    if (!generationPreflight) {
        return ::media::Result<MediaNodeProcessResult>::failure(generationPreflight.error());
    }
    if (generationPreflight.value()) return processProgress();
    if (m_generationData->videoEof && m_generationData->audioEof && !m_generationData->videoHead && !m_generationData->audioHead) {
        if (!m_generationData->terminal) {
            m_generationData->terminal = makeMediaBufferRef<MediaControlBuffer>(
                MediaControlBufferKind::Eof, *m_generationData->activeGeneration);
        }
        return emitWithCommit(
            context, m_generationData->terminal,
            MediaAvSchedulerPendingCommit{
                MediaAvSchedulerCommitKind::Terminal, true});
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
            return ::media::Result<bool>::success(true);
        }
        return ::media::Result<bool>::success(false);
    }
    auto parsed = MediaAvSchedulerHead::parse(
        std::move(*popped.value()), input == Input::Video
            ? MediaScheduledStream::Video : MediaScheduledStream::Audio);
    if (!parsed) return ::media::Result<bool>::failure(parsed.error());
    head = std::move(parsed).value();
    if (input == Input::Audio) {
        if (const auto* unit = head->canonical(); unit && unit->audioSampleInterval())
            m_audioInputObservation.observe(unit->generation(), *unit->audioSampleInterval());
    }
    const auto arrived = m_group->clock()->now();
    head->recordArrival(arrived ? std::optional<MediaRunningTime>(arrived.value()) : std::nullopt);
    if (head->kind() != MediaAvSchedulerHeadKind::Control) {
        logMediaHeadArrival(input);
    }
    return ::media::Result<bool>::success(true);
}

void MediaAvOutputSchedulerNode::logMediaHeadArrival(Input input)
try
{
    auto& emitted = input == Input::Video
        ? m_generationData->firstVideoHeadDiagnosticEmitted
        : m_generationData->firstAudioHeadDiagnosticEmitted;
    if (!m_group || !mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
    const auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
    if (!head || head->kind() == MediaAvSchedulerHeadKind::Control) return;

    const auto epoch = m_group->playbackEpoch();
    const auto mapArrivalTime = [&](MediaRunningTime canonical)
        -> std::optional<MediaRunningTime> {
        if (!epoch || epoch.value().generation != head->generation()) return std::nullopt;
        const auto offset = canonical.checkedSubtract(epoch.value().sourceStart);
        if (!offset) return std::nullopt;
        const auto mapped = epoch.value().masterRelease.checkedAdd(offset.value());
        return mapped ? std::optional<MediaRunningTime>(mapped.value()) : std::nullopt;
    };
    const auto dispatch = head->canonicalDispatchTime();
    const auto dispatchMaster = dispatch
        ? mapArrivalTime(dispatch.value()) : std::nullopt;
    const auto now = m_group->clock()->now();
    const bool late = now && dispatchMaster && now.value() >= *dispatchMaster;
    if (emitted && !late) return;
    const auto observedSteady = mediaAccessUnitObservationNow();
    const auto targetMaster = mapArrivalTime(head->canonicalPresentation());
    std::ostringstream out;
    out << "av_scheduler_trace stage=" << (emitted ? "late_" : "first_")
        << (input == Input::Video ? "video" : "audio")
        << "_head generation=" << head->generation()
        << " mapping_epoch_generation="
        << (epoch ? std::to_string(epoch.value().generation) : "unavailable")
        << " canonical_target_ns=" << head->canonicalPresentation().nanoseconds()
        << " canonical_dispatch_ns="
        << (dispatch ? std::to_string(dispatch.value().nanoseconds()) : "error")
        << " target_master_ns="
        << (targetMaster ? std::to_string(targetMaster.value().nanoseconds()) : "error");
    out << " dispatch_master_ns="
        << (dispatchMaster
                ? std::to_string(dispatchMaster->nanoseconds())
                : "error");
    out << " now_ns="
        << (now ? std::to_string(now.value().nanoseconds()) : "error");
    if (const auto* unit = head->canonical()) {
        const auto& identity = unit->lineage()->identity;
        const auto* output = std::get_if<MediaOutputAccessUnitIdentity>(&identity);
        const auto name = output ? output->outputIdentity.view()
            : std::get<MediaSourceAccessUnitIdentity>(identity).sourceIdentity.view();
        out << " node=" << nodeId().value
            << " identity_kind=" << (output ? "output" : "source")
            << " identity=" << std::quoted(std::string(name))
            << " canonical_sequence=" << unit->canonicalSequence().value()
            << " canonical_created_steady_ns="
            << (unit->diagnosticCreatedSteadyNs()
                ? std::to_string(*unit->diagnosticCreatedSteadyNs()) : "unavailable")
            << " head_observed_steady_ns="
            << (observedSteady ? std::to_string(*observedSteady) : "unavailable")
            << " codec_receive_begin_steady_ns="
            << (unit->codecReceiveObservation()
                ? std::to_string(unit->codecReceiveObservation()->beginSteadyNs) : "unavailable")
            << " codec_receive_end_steady_ns="
            << (unit->codecReceiveObservation()
                ? std::to_string(unit->codecReceiveObservation()->endSteadyNs) : "unavailable");
    }
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            out.str());
    emitted = true;
}
catch (...) {
    // Optional observations must not prevent processing an already dequeued AU.
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

::media::Result<bool> MediaAvOutputSchedulerNode::preflightGenerations(
    const MediaAvReacquisitionSnapshot& reacquisition,
    const MediaAvEpochTransitionSnapshot& epoch)
{
    bool discardedOldHead = false;
    const auto inspect = [&](std::optional<MediaAvSchedulerHead>& head,
                             bool video) -> ::media::Result<bool> {
        if (!head || head->kind() == MediaAvSchedulerHeadKind::Control) {
            return ::media::Result<bool>::success(false);
        }
        auto classified = classifyMediaAvGenerationEvidence(
            reacquisition, epoch, head->generation());
        if (!classified) return ::media::Result<bool>::failure(classified.error());
        if (classified.value() == MediaAvGenerationEvidenceDisposition::Future) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V scheduler requires explicit generation reacquisition"));
        }
        if (classified.value() == MediaAvGenerationEvidenceDisposition::Retired) {
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                std::string("av_scheduler_trace stage=retired_input stream=") +
                    (video ? "video" : "audio") + " generation=" +
                    std::to_string(head->generation()));
            head.reset();
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
    if (m_generationData->videoHead && m_generationData->activeGeneration &&
        m_generationData->videoHead->generation() == *m_generationData->activeGeneration) {
        auto checked = prepareVideoProgress(*m_generationData->videoHead->canonical());
        if (!checked) return ::media::Result<std::optional<Input>>::failure(checked.error());
    }
    if ((!m_generationData->videoHead && !m_generationData->videoEof) || (!m_generationData->audioHead && !m_generationData->audioEof)) {
        if (!m_generationData->videoHead && m_generationData->audioHead) {
            auto permitted = audioPrecedesVideoProgress();
            if (!permitted) return ::media::Result<std::optional<Input>>::failure(permitted.error());
            if (permitted.value()) return ::media::Result<std::optional<Input>>::success(Input::Audio);
        }
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
    return processMedia(context, input);
}

::media::Result<MediaNodeProcessResult> MediaAvOutputSchedulerNode::processMedia(
    MediaGraphExecutionContext& context, Input input)
{
    const auto& head = input == Input::Video ? m_generationData->videoHead : m_generationData->audioHead;
    const auto* unit = head->canonical();
    if (!unit || !m_generationData->activeGeneration ||
        unit->generation() != *m_generationData->activeGeneration)
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("Encoded scheduler requires the active canonical generation"));
    auto presentation = m_group->mapCanonicalToMaster(unit->canonicalPresentation());
    if (!presentation) return ::media::Result<MediaNodeProcessResult>::failure(presentation.error());
    auto canonicalDispatch = unit->canonicalDispatch();
    if (!canonicalDispatch) return ::media::Result<MediaNodeProcessResult>::failure(canonicalDispatch.error());
    auto progress = prepareVideoProgress(*unit);
    if (!progress) return ::media::Result<MediaNodeProcessResult>::failure(progress.error());
    auto dispatch = m_group->mapCanonicalToMaster(canonicalDispatch.value());
    if (!dispatch) return ::media::Result<MediaNodeProcessResult>::failure(dispatch.error());
    auto schedule = MediaOutputSchedule::create(presentation.value(), dispatch.value(), m_transportLead);
    if (!schedule) return ::media::Result<MediaNodeProcessResult>::failure(schedule.error());
    auto release = MediaOutputSchedule::releaseTime(schedule.value().emit, m_protocolPreparationLead);
    if (!release) return ::media::Result<MediaNodeProcessResult>::failure(release.error());
    auto now = m_outputAuthority->now();
    if (!now) return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    if (now.value() < release.value())
        return ::media::Result<MediaNodeProcessResult>::success({MediaNodeProcessState::Waiting,
            m_outputAuthority->deadlineWait(release.value(), MediaNodeDeadlineWakePolicy::InputOrDeadline)});
    auto output = MediaAvScheduledOutputBuilder::canonical(*unit,
        schedule.value().presentation, schedule.value().dispatch, schedule.value().emit);
    if (!output) return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    return emitWithCommit(context, output.value(), MediaAvSchedulerPendingCommit{
        input == Input::Video ? MediaAvSchedulerCommitKind::Video : MediaAvSchedulerCommitKind::Audio,
        false, std::nullopt, now.value(), std::move(progress).value(),
        input == Input::Audio && !m_generationData->videoHead && !m_generationData->videoEof});
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
            if (m_group->plan().domainRole != MediaAvSyncDomainRole::SharedSourceOutput)
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::invalidArgument("Output-only scheduler cannot flush a source lifecycle"));
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
                MediaAvSchedulerCommitKind::Terminal, true});
    }
    if (!m_generationData->terminal) m_generationData->terminal = head->buffer();
    head.reset();
    if (input == Input::Video) {
        m_generationData->videoEof = true;
    } else {
        m_generationData->audioEof = true;
    }
    if (!m_generationData->videoEof || !m_generationData->audioEof) return processProgress();
    return emitWithCommit(
        context, m_generationData->terminal,
        MediaAvSchedulerPendingCommit{
            MediaAvSchedulerCommitKind::Terminal, true});
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
    if (!m_outputAuthority || !generation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "A/V scheduler commit requires a planned generation"));
    }
    auto reservation = m_generationState->reserveCommit(
        *m_outputAuthority, *generation);
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(
            std::move(reservation).value()));
}

::media::Status MediaAvOutputSchedulerNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (!m_generationData->pendingCommit || !m_generationData->pendingCommit->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "A/V scheduler output commit has no matching reserved generation transaction"));
    }
    try {
        if (const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(buffer.get());
            scheduled && m_group && mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) {
            const auto now = m_group->clock()->now();
            if (now && now.value() >= scheduled->dispatchOnMaster()) {
                std::ostringstream out;
                out << "av_scheduler_trace stage=late_output_commit stream="
                    << (scheduled->stream() == MediaScheduledStream::Video ? "video" : "audio")
                    << " generation=" << scheduled->generation()
                    << " canonical_dispatch_ns=" << scheduled->canonicalDispatch().nanoseconds()
                    << " emit_ns=" << scheduled->emitOnMaster().nanoseconds()
                    << " dispatch_ns=" << scheduled->dispatchOnMaster().nanoseconds()
                    << " committed_at_ns=" << now.value().nanoseconds();
                const auto& head = scheduled->stream() == MediaScheduledStream::Video
                    ? m_generationData->videoHead : m_generationData->audioHead;
                if (head && head->canonical()) {
                    out << " canonical_sequence=" << head->canonical()->canonicalSequence().value();
                    head->waitObservation().append(out);
                }
                const auto& other = scheduled->stream() == MediaScheduledStream::Video
                    ? m_generationData->audioHead : m_generationData->videoHead;
                if (other && other->canonical()) {
                    const auto otherDispatch = other->canonicalDispatchTime();
                    out << " other_generation=" << other->generation()
                        << " other_sequence=" << other->canonical()->canonicalSequence().value()
                        << " other_canonical_dispatch_ns="
                        << (otherDispatch ? std::to_string(otherDispatch.value().nanoseconds()) : "unavailable")
                        << " other_arrived_ns="
                        << (other->arrival() ? std::to_string(other->arrival()->nanoseconds()) : "unavailable");
                    if (otherDispatch && other->generation() == scheduled->generation()) {
                        // The commit permit already holds the transition lock.
                        // This accepted unit carries the same-generation mapping;
                        // translate relative to it without reacquiring that lock.
                        const auto delta = otherDispatch.value().checkedSubtract(scheduled->canonicalDispatch());
                        const auto mapped = delta
                            ? scheduled->dispatchOnMaster().checkedAdd(delta.value())
                            : ::media::Result<MediaRunningTime>::failure(delta.error());
                        out << " other_dispatch_master_ns="
                            << (mapped ? std::to_string(mapped.value().nanoseconds()) : "unavailable");
                    }
                }
                out << " committed_video_progress_ns="
                    << (m_generationData->videoProgress
                        ? std::to_string(m_generationData->videoProgress->followingAccessUnitsLowerBound.nanoseconds()) : "unavailable")
                    << " carrying_video_progress_ns="
                    << (m_generationData->pendingCommit->videoProgress
                        ? std::to_string(m_generationData->pendingCommit->videoProgress->followingAccessUnitsLowerBound.nanoseconds()) : "unavailable")
                    << " head_arrived_ns="
                    << (head && head->arrival() ? std::to_string(head->arrival()->nanoseconds()) : "unavailable")
                    << " scheduled_at_ns="
                    << (m_generationData->pendingCommit->scheduledAt
                        ? std::to_string(m_generationData->pendingCommit->scheduledAt->nanoseconds()) : "unavailable");
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                        MediaGraphDiagnosticPhase::RuntimeNode, out.str());
            }
        }
    } catch (...) {
        // Optional observations cannot prevent committing an accepted output.
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
        m_generationData->videoProgress = std::move(commit.videoProgress);
        m_generationData->videoHead.reset();
        m_generationData->nextEqualTimeVideo = false;
    } else if (commit.kind == MediaAvSchedulerCommitKind::Audio) {
        if (commit.usedVideoProgress) observeVideoProgressRelease();
        m_generationData->audioHead.reset();
        m_generationData->nextEqualTimeVideo = true;
    } else {
        m_generationData->videoProgress.reset();
        m_generationData->videoHead.reset();
        m_generationData->audioHead.reset();
        m_generationData->terminal.reset();
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
    if (m_group->plan().domainRole != MediaAvSyncDomainRole::SharedSourceOutput)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Output-only scheduler cannot flush a source lifecycle"));
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
    m_audioInputObservation.log(nodeId().value, "scheduler_input");
    logCorrelatedWait();
    auto reset = resetState();
    auto stopped = FFmpegNodeRuntime::stop(context);
    return reset ? stopped : reset;
}

void MediaAvOutputSchedulerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_audioInputObservation.log(nodeId().value, "scheduler_input");
    logCorrelatedWait();
    if (m_group) m_group->markAborted();
    if (auto reset = resetState(); !reset) {
        cancelPendingOutputTransfer();
        m_generationData.reset();
    }
    FFmpegNodeRuntime::abort(context);
}

::media::Status MediaAvOutputSchedulerNode::resetState()
{
    m_audioInputObservation = {};
    m_currentWait.reset();
    m_maximumWait.reset();
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
    m_currentWait.reset();
    m_generationData->activeGeneration.reset();
    m_generationData->videoProgress.reset();
    m_generationData->videoHead.reset();
    m_generationData->audioHead.reset();
    m_generationData->terminal.reset();
    m_generationData->pendingCommit.reset();
    m_generationData->completedCommitResult.reset();
    m_generationData->videoEof = false;
    m_generationData->audioEof = false;
    m_generationData->nextEqualTimeVideo = false;
    m_generationData->firstVideoHeadDiagnosticEmitted = false;
    m_generationData->firstAudioHeadDiagnosticEmitted = false;
    m_generationData->firstVideoProgressReleaseObserved = false;
    m_generationData->missingMediaWait.reset();
}

} // namespace media::ffmpeg::graph
