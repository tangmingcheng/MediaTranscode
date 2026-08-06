#include "internal/graph/nodes/sync/MediaLockedPacketGateNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using GateDispositionResult =
    ::media::Result<MediaLockedPacketGateDisposition>;

::media::ErrorInfo invalidGateEvidence(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

bool transitionActive(MediaAvReacquisitionPhase phase) noexcept
{
    return phase == MediaAvReacquisitionPhase::Purging ||
        phase == MediaAvReacquisitionPhase::Acquiring ||
        phase == MediaAvReacquisitionPhase::ReadyForActivation;
}

} // namespace

MediaLockedPacketGateNode::MediaLockedPacketGateNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaLockedPacketGateNode")
{
}

MediaNodeKind MediaLockedPacketGateNode::staticKind() noexcept
{
    return MediaNodeKind::LockedPacketGate;
}

::media::Result<MediaOutputCommitReservation>
MediaLockedPacketGateNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    if (!dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        return FFmpegNodeRuntime::reserveOutputCommit(buffer);
    }
    if (!m_controlGenerationPolicy || !m_syncGroup) {
        return ::media::Result<
            MediaOutputCommitReservation>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Locked packet gate control policy is not configured"));
    }
    auto classified = classifyMediaAvSyncControl(
        buffer, *m_controlGenerationPolicy, *m_syncGroup,
        m_initialGeneration);
    if (!classified) {
        return ::media::Result<
            MediaOutputCommitReservation>::failure(
                classified.error());
    }
    return reserveMediaAvSyncControlPublication(
        std::move(classified).value(), m_lockedGeneration,
        MediaControlConsumerGenerationRequirement::ExactWhenPresent);
}

::media::Result<MediaNodeProcessResult>
MediaLockedPacketGateNode::onProcess(MediaGraphExecutionContext& context)
{
    if (auto status = configure(context); !status) return processProgress(status);
    if (m_acquisitionDeadline->deadline()) {
        auto now = m_syncGroup->clock()->now();
        if (!now) {
            return ::media::Result<MediaNodeProcessResult>::failure(now.error());
        }
        if (auto status = m_acquisitionDeadline->preflight(now.value()); !status) {
            return processProgress(status);
        }
    }

    auto clock = tryPopInputOptional(context, "clock");
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(clock.error());
    }
    if (clock.value()) {
        auto disposition = acceptClock(*clock.value());
        return disposition
            ? processProgress()
            : processProgress(
                  ::media::Status::failure(disposition.error()));
    }
    if (m_pendingInput) {
        if (!m_lockedGeneration &&
            m_syncGroup->reacquisitionSnapshot().phase ==
                MediaAvReacquisitionPhase::Inactive) {
            if (m_acquisitionDeadline->deadline()) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waitingUntil(
                        *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
            }
            return processWaiting();
        }
        MediaBufferRef pending = std::move(m_pendingInput);
        return processInput(context, std::move(pending));
    }
    auto packet = tryPopInputOptional(context, "packet");
    if (!packet) {
        return ::media::Result<MediaNodeProcessResult>::failure(packet.error());
    }
    if (!packet.value()) {
        if (m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
    MediaBufferRef input = std::move(*packet.value());
    if (dynamic_cast<const MediaControlBuffer*>(input.get())) {
        return processInput(context, std::move(input));
    }
    if (!m_lockedGeneration) {
        const auto snapshot = m_syncGroup->reacquisitionSnapshot();
        if (snapshot.phase != MediaAvReacquisitionPhase::Inactive) {
            return processProgress(
                processPacket(context, std::move(input)));
        }
        auto generation = packetGeneration(input);
        if (!generation) {
            return processProgress(
                ::media::Status::failure(generation.error()));
        }
        if (auto status = retainPendingInput(std::move(input)); !status) {
            return processProgress(status);
        }
        if (m_acquisitionDeadline->deadline()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waitingUntil(
                    *m_syncGroupKey, *m_acquisitionDeadline->deadline()));
        }
        return processWaiting();
    }
    return processInput(context, std::move(input));
}

::media::Status MediaLockedPacketGateNode::configure(
    MediaGraphExecutionContext& context)
{
    if (m_configured) return ::media::Status::success();
    const MediaNodeOptions* options = nodeOptions(context);
    auto stream = requiredStreamKindNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.stream");
    auto timeout = requiredPositiveInt64NodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.acquiring_timeout_ns");
    auto group = requiredNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.sync_group");
    auto initialGeneration = requiredPositiveInt64NodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.initial_generation");
    auto initialPolicy = requiredNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.initial_generation_policy");
    auto controlGenerationPolicy = requiredNodeOption(
        options, "MediaLockedPacketGateNode",
        "locked_packet_gate.control_generation_policy");
    if (!stream) return ::media::Status::failure(stream.error());
    if (!timeout) return ::media::Status::failure(timeout.error());
    if (!group) return ::media::Status::failure(group.error());
    if (!initialGeneration) {
        return ::media::Status::failure(initialGeneration.error());
    }
    if (!initialPolicy) return ::media::Status::failure(initialPolicy.error());
    if (!controlGenerationPolicy) {
        return ::media::Status::failure(
            controlGenerationPolicy.error());
    }
    if (initialPolicy.value() != "first_locked_only_fail_on_change") {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Locked packet gate rejects an unsupported initial generation policy"));
    }
    m_streamKind = stream.value();
    m_initialGeneration =
        static_cast<std::uint64_t>(initialGeneration.value());
    auto decodedControlGenerationPolicy =
        decodeMediaControlGenerationPolicy(
            controlGenerationPolicy.value());
    if (!decodedControlGenerationPolicy) {
        return ::media::Status::failure(
            decodedControlGenerationPolicy.error());
    }
    m_controlGenerationPolicy =
        decodedControlGenerationPolicy.value();
    auto deadline = MediaInitialClockAcquisitionDeadline::create(
        MediaRunningTime::fromNanoseconds(timeout.value()));
    if (!deadline) return ::media::Status::failure(deadline.error());
    m_acquisitionDeadline.emplace(std::move(deadline).value());
    m_syncGroupKey.emplace(std::move(group).value());
    m_syncGroup = context.findAvSyncGroup(*m_syncGroupKey);
    if (!m_syncGroup || !m_syncGroup->clock()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Locked packet gate requires registered master clock"));
    }
    m_configured = true;
    return ::media::Status::success();
}

::media::Result<MediaLockedPacketGateDisposition>
MediaLockedPacketGateNode::acceptClock(const MediaBufferRef& buffer)
{
    if (dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        auto control = classifyMediaAvSyncControl(
            buffer, *m_controlGenerationPolicy, *m_syncGroup,
            m_initialGeneration);
        if (!control) {
            return GateDispositionResult::failure(control.error());
        }
        switch (control.value().generation) {
        case MediaControlGenerationDisposition::DropOld:
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::DropOldGeneration);
        case MediaControlGenerationDisposition::Withhold:
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::
                    WithholdForReacquisition);
        case MediaControlGenerationDisposition::Forward:
            if (control.value().control.kind ==
                MediaControlBufferKind::Flush) {
                return GateDispositionResult::failure(
                    invalidGateEvidence(
                        "Locked packet gate rejects clock flush"));
            }
            if (control.value().control.control->generation() &&
                (!m_lockedGeneration ||
                 *control.value().control.control->generation() !=
                     *m_lockedGeneration)) {
                return GateDispositionResult::failure(
                    invalidGateEvidence(
                        "Locked packet gate clock control differs from its locked generation"));
            }
            return m_lockedGeneration
                ? GateDispositionResult::success(
                      MediaLockedPacketGateDisposition::Pass)
                : GateDispositionResult::failure(
                      invalidGateEvidence(
                          "Locked packet gate clock ended before lock"));
        }
    }
    const auto* state =
        dynamic_cast<const MediaSourceClockStateBuffer*>(buffer.get());
    if (!state) {
        return GateDispositionResult::failure(
            invalidGateEvidence(
                "Locked packet gate requires source clock state"));
    }

    const bool discontinuity =
        hasFlag(state->flags(), MediaBufferFlag::Discontinuity);
    if (discontinuity) {
        if (!m_lockedGeneration ||
            state->readiness() !=
                MediaSourceClockReadiness::ReacquireRequired ||
            state->generation() == 0 ||
            state->generation() != *m_lockedGeneration) {
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "locked_gate_discontinuity_evidence stream=" +
                    std::string(m_streamKind == MediaStreamKind::Video
                        ? "video" : "audio") +
                    " locked_generation=" +
                    (m_lockedGeneration
                        ? std::to_string(*m_lockedGeneration)
                        : std::string("none")) +
                    " state_generation=" +
                    std::to_string(state->generation()) +
                    " readiness=" +
                    std::to_string(static_cast<int>(state->readiness())));
            return GateDispositionResult::failure(
                invalidGateEvidence(
                    "Locked packet gate rejects malformed discontinuity evidence"));
        }
        const auto snapshot = m_syncGroup->reacquisitionSnapshot();
        if (transitionActive(snapshot.phase)) {
            if (!snapshot.transition ||
                snapshot.transition->oldGeneration != state->generation() ||
                snapshot.reason !=
                    MediaAvReacquisitionReason::HardDiscontinuity) {
                return GateDispositionResult::failure(
                    invalidGateEvidence(
                        "Locked packet gate rejects incompatible discontinuity evidence"));
            }
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::WithholdForReacquisition);
        }
        if (snapshot.phase != MediaAvReacquisitionPhase::Inactive ||
            snapshot.transition) {
            return GateDispositionResult::failure(
                invalidGateEvidence(
                    "Locked packet gate rejects discontinuity outside a live group"));
        }
        auto observed = m_syncGroup->observeGeneration(state->generation());
        if (!observed ||
            observed.value() !=
                MediaAvSyncGroupRuntime::GenerationDisposition::Current) {
            return observed
                ? GateDispositionResult::failure(
                      invalidGateEvidence(
                          "Locked packet gate discontinuity requires the active generation"))
                : GateDispositionResult::failure(observed.error());
        }
        auto requested = m_syncGroup->requestReacquisition(
            MediaAvReacquisitionRequest{
                state->generation(),
                MediaAvReacquisitionReason::HardDiscontinuity});
        if (!requested) {
            return GateDispositionResult::failure(requested.error());
        }
        const auto requestedSnapshot =
            m_syncGroup->reacquisitionSnapshot();
        if (!transitionActive(requestedSnapshot.phase) ||
            !requestedSnapshot.transition ||
            requestedSnapshot.transition->oldGeneration !=
                state->generation()) {
            return GateDispositionResult::failure(
                invalidGateEvidence(
                    "Locked packet gate requires the requested group transition"));
        }
        return GateDispositionResult::success(
            MediaLockedPacketGateDisposition::WithholdForReacquisition);
    }
    if (state->readiness() ==
        MediaSourceClockReadiness::ReacquireRequired) {
        return GateDispositionResult::failure(
            invalidGateEvidence(
                "Locked packet gate rejects unmarked reacquisition evidence"));
    }
    if (state->readiness() == MediaSourceClockReadiness::Acquiring) {
        const auto snapshot = m_syncGroup->reacquisitionSnapshot();
        if (transitionActive(snapshot.phase)) {
            if ((snapshot.phase !=
                     MediaAvReacquisitionPhase::Acquiring &&
                 snapshot.phase !=
                     MediaAvReacquisitionPhase::ReadyForActivation) ||
                !snapshot.transition ||
                state->generation() !=
                    snapshot.transition->nextGeneration) {
                return GateDispositionResult::failure(
                    invalidGateEvidence(
                        "Locked packet gate rejects acquiring evidence without the active transition"));
            }
            return GateDispositionResult::success(
                MediaLockedPacketGateDisposition::
                    WithholdForReacquisition);
        }
        if (snapshot.phase != MediaAvReacquisitionPhase::Inactive ||
            snapshot.transition ||
            m_lockedGeneration) {
            return GateDispositionResult::failure(
                invalidGateEvidence(
                    "Locked packet gate rejects acquiring evidence without the active transition"));
        }
        {
            auto now = m_syncGroup->clock()->now();
            if (!now) return GateDispositionResult::failure(now.error());
            auto established =
                m_acquisitionDeadline->establish(now.value());
            return established
                ? GateDispositionResult::success(
                      MediaLockedPacketGateDisposition::
                          WithholdForReacquisition)
                : GateDispositionResult::failure(established.error());
        }
    }
    if (state->readiness() != MediaSourceClockReadiness::Locked) {
        return GateDispositionResult::failure(
            invalidGateEvidence(
                "Locked packet gate rejects degraded clock evidence"));
    }
    auto disposition =
        classifyLockedGeneration(state->generation(), true);
    if (disposition &&
        (disposition.value() ==
             MediaLockedPacketGateDisposition::Pass ||
         disposition.value() ==
             MediaLockedPacketGateDisposition::PassToInitialAcquisition ||
         disposition.value() ==
             MediaLockedPacketGateDisposition::PassToReacquisition)) {
        m_lockedGeneration = state->generation();
        m_acquisitionDeadline->clear();
    }
    return disposition;
}

::media::Result<MediaLockedPacketGateDisposition>
MediaLockedPacketGateNode::classifyLockedGeneration(
    std::uint64_t generation,
    bool acceptingInitialClock)
{
    auto arbitration = m_syncGroup->reserveGenerationArbitration();
    if (!arbitration) {
        return GateDispositionResult::failure(arbitration.error());
    }
    auto disposition = classifyLockedPacketGateGeneration(
        arbitration.value().reacquisition(),
        arbitration.value().epoch(),
        generation, m_initialGeneration);
    if (!disposition ||
        disposition.value() !=
            MediaLockedPacketGateDisposition::PassToInitialAcquisition) {
        return disposition;
    }
    if (!acceptingInitialClock &&
        (!m_lockedGeneration || generation != *m_lockedGeneration)) {
        return GateDispositionResult::failure(
            invalidGateEvidence(
                "Locked packet gate initial packet generation differs from its planned lock"));
    }
    return disposition;
}

::media::Result<std::uint64_t>
MediaLockedPacketGateNode::packetGeneration(const MediaBufferRef& buffer) const
{
    const auto* packet =
        dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
    if (!packet || !packet->sourceTiming() ||
        packet->streamKind() != m_streamKind ||
        packet->sourceTiming()->readiness !=
            MediaSourceClockReadiness::Locked ||
        packet->sourceTiming()->generation == 0 ||
        hasFlag(packet->flags(), MediaBufferFlag::Discontinuity)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Locked packet gate rejects packet clock evidence"));
    }
    return ::media::Result<std::uint64_t>::success(
        packet->sourceTiming()->generation);
}

::media::Status MediaLockedPacketGateNode::processPacket(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    auto generation = packetGeneration(buffer);
    if (!generation) {
        return ::media::Status::failure(generation.error());
    }
    auto arbitration = m_syncGroup->reserveGenerationArbitration();
    if (!arbitration) {
        return ::media::Status::failure(arbitration.error());
    }
    auto disposition = classifyLockedPacketGateGeneration(
        arbitration.value().reacquisition(),
        arbitration.value().epoch(),
        generation.value(), m_initialGeneration);
    if (!disposition) {
        return ::media::Status::failure(disposition.error());
    }
    if (disposition.value() ==
            MediaLockedPacketGateDisposition::PassToInitialAcquisition &&
        (!m_lockedGeneration ||
         generation.value() != *m_lockedGeneration)) {
        return ::media::Status::failure(
            invalidGateEvidence(
                "Locked packet gate initial packet generation differs from its planned lock"));
    }
    switch (disposition.value()) {
    case MediaLockedPacketGateDisposition::Pass:
    case MediaLockedPacketGateDisposition::PassToInitialAcquisition:
    case MediaLockedPacketGateDisposition::PassToReacquisition:
        return emitOutput(context, "packet", buffer);
    case MediaLockedPacketGateDisposition::WithholdForReacquisition:
        return retainPendingInput(std::move(buffer));
    case MediaLockedPacketGateDisposition::DropOldGeneration:
        return ::media::Status::success();
    }
    return ::media::Status::failure(
        invalidGateEvidence(
            "Locked packet gate rejects unknown disposition"));
}

::media::Result<MediaNodeProcessResult>
MediaLockedPacketGateNode::processInput(
    MediaGraphExecutionContext& context,
    MediaBufferRef buffer)
{
    if (dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        auto control = classifyMediaAvSyncControl(
            buffer, *m_controlGenerationPolicy, *m_syncGroup,
            m_initialGeneration);
        if (!control) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                control.error());
        }
        switch (control.value().generation) {
        case MediaControlGenerationDisposition::DropOld:
            return processProgress();
        case MediaControlGenerationDisposition::Withhold:
            if (auto retained = retainPendingInput(std::move(buffer));
                !retained) {
                return processProgress(retained);
            }
            return processWaiting();
        case MediaControlGenerationDisposition::Forward:
            if (control.value().control.kind ==
                MediaControlBufferKind::Flush) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    invalidGateEvidence(
                        "Locked packet gate rejects discontinuity flush"));
            }
            if (!m_lockedGeneration) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    invalidGateEvidence(
                        "Locked packet gate cannot terminate before initial lock"));
            }
            if (control.value().control.control->generation() &&
                *control.value().control.control->generation() !=
                    *m_lockedGeneration) {
                if (auto retained =
                        retainPendingInput(std::move(buffer));
                    !retained) {
                    return processProgress(retained);
                }
                return processWaiting();
            }
            {
                control.value().publicationAuthority.reset();
                auto emitted = emitOutput(
                    context, "packet", buffer);
                return control.value().control.flow ==
                        MediaControlBufferFlow::Terminal
                    ? processFinished(std::move(emitted))
                    : processProgress(std::move(emitted));
            }
        }
    }
    return processProgress(
        processPacket(context, std::move(buffer)));
}

::media::Status MediaLockedPacketGateNode::retainPendingInput(
    MediaBufferRef buffer)
{
    if (m_pendingInput || !buffer) {
        return ::media::Status::failure(
            invalidGateEvidence(
                "Locked packet gate retains at most one pending input"));
    }
    m_pendingInput = std::move(buffer);
    return ::media::Status::success();
}

::media::Status MediaLockedPacketGateNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaLockedPacketGateNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaLockedPacketGateNode::resetState() noexcept
{
    m_lockedGeneration.reset();
    m_syncGroupKey.reset();
    m_syncGroup.reset();
    m_acquisitionDeadline.reset();
    m_pendingInput.reset();
    m_streamKind = MediaStreamKind::Unknown;
    m_controlGenerationPolicy.reset();
    m_configured = false;
}

} // namespace media::ffmpeg::graph
