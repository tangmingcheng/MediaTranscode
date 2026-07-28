#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include <array>
#include <span>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::vector<MediaChannel*>> channelsForPort(
    MediaGraphExecutionContext& context,
    MediaNodeId nodeId,
    const char* portName)
{
    const MediaGraph* graph = context.graph();
    const MediaPort* port = graph ? graph->findOutputPort(nodeId, portName)
                                  : nullptr;
    if (!port) {
        return ::media::Result<std::vector<MediaChannel*>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Activation release sequencer requires its planned outputs"));
    }
    std::vector<MediaChannel*> channels;
    for (MediaChannel* channel : context.outputChannels(nodeId)) {
        if (channel && channel->binding().from.portId == port->id) {
            channels.push_back(channel);
        }
    }
    if (channels.empty()) {
        return ::media::Result<std::vector<MediaChannel*>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Activation release sequencer output has no planned target"));
    }
    return ::media::Result<std::vector<MediaChannel*>>::success(
        std::move(channels));
}

} // namespace

MediaActivatedStartupReleaseSequencerNode::
    MediaActivatedStartupReleaseSequencerNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpochActivationCapability capability,
        MediaRunningTime outputLead,
        std::optional<MediaAvStartupVideoPreparationCapability>
            preparationCapability)
    : m_nodeId(nodeId)
    , m_groupKey(std::move(groupKey))
    , m_capability(std::move(capability))
    , m_outputLead(outputLead)
    , m_preparationCapability(std::move(preparationCapability))
{
}

MediaNodeKind MediaActivatedStartupReleaseSequencerNode::staticKind() noexcept
{
    return MediaNodeKind::ActivatedStartupReleaseSequencer;
}

MediaNodeId MediaActivatedStartupReleaseSequencerNode::nodeId() const noexcept
{
    return m_nodeId;
}

::media::Result<MediaNodeProcessResult>
MediaActivatedStartupReleaseSequencerNode::process(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    if (!m_pendingTransaction) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "transaction"),
            "Activation release sequencer", "transaction");
        if (!input) {
            return failTerminal(input.error());
        }
        if (!input.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
        m_pendingTransaction = std::move(*input.value());
    }
    const auto* transaction =
        dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
            m_pendingTransaction.get());
    if (!transaction) {
        return failTerminal(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer requires a typed transaction"));
    }

    auto releaseChannels = channelsForPort(context, nodeId(), "bound_release");
    if (!releaseChannels) {
        return failTerminal(releaseChannels.error());
    }
    if (releaseChannels.value().size() != 1) {
        return failTerminal(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer requires one release target"));
    }
    if (transaction->transactionKind() ==
        MediaStartupReleaseTransactionKind::Control) {
        const auto* control = transaction->control();
        if (!control) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer rejects an invalid control transaction"));
        }
        const std::array<MediaAtomicOutputBatch, 1> batches{
            MediaAtomicOutputBatch{
                releaseChannels.value().front(),
                std::span(&transaction->payload(), 1)}};
        auto atomic = MediaAtomicOutputTransaction::acquire(
            "Activation release sequencer", batches);
        if (!atomic) return failTerminal(atomic.error());
        if (!atomic.value()) return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
        if (auto status = atomic.value()->commit(); !status)
            return failTerminal(status.error());
        const bool finished =
            control->controlKind() == MediaControlBufferKind::Eof ||
            control->controlKind() == MediaControlBufferKind::Abort;
        m_pendingTransaction.reset();
        return ::media::Result<MediaNodeProcessResult>::success(
            finished ? MediaNodeProcessResult::finished()
                     : MediaNodeProcessResult::progress());
    }

    const auto* release = transaction->release();
    if (!release || release->groupKey() != m_groupKey) {
        return failTerminal(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer rejects a mismatched transaction"));
    }
    auto eventChannels = channelsForPort(context, nodeId(), "activated");
    if (!eventChannels) {
        return failTerminal(eventChannels.error());
    }
    MediaBufferRef eventForCommit = m_activatedEvent;
    bool activateInitial = false;
    bool activateNext = false;
    std::shared_ptr<MediaAvSyncGroupRuntime> activationGroup;
    switch (release->releaseKind()) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease: {
        if (m_activatedEvent) {
            return failTerminal(
                ::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer rejects duplicate initial activation"));
        }
        if (m_preparationCapability) {
            const auto preparation = m_preparationCapability->snapshot();
            if (preparation.phase ==
                    MediaAvStartupVideoPreparationPhase::Feeding ||
                preparation.phase ==
                    MediaAvStartupVideoPreparationPhase::Awaiting) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
            if (preparation.phase !=
                    MediaAvStartupVideoPreparationPhase::FilterReady ||
                preparation.generation != release->epoch().generation ||
                preparation.releaseIdentity != transaction->releaseIdentity()) {
                return failTerminal(::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer rejects mismatched video preparation"));
            }
            if (!preparation.filterOutputReserved ||
                !preparation.extractorOutputsReserved) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
            if (!preparation.anchoredEpoch) {
                auto group = context.findAvSyncGroup(m_groupKey);
                if (!group || group->key() != m_groupKey || !group->clock()) {
                    return failTerminal(::media::ErrorInfo::notInitialized(
                        "Activation release sequencer requires its exact master clock"));
                }
                auto now = group->clock()->now();
                if (!now) return failTerminal(now.error());
                auto masterRelease = now.value().checkedAdd(m_outputLead);
                if (!masterRelease) return failTerminal(masterRelease.error());
                MediaPlaybackEpoch anchoredEpoch = release->epoch();
                anchoredEpoch.masterRelease = masterRelease.value();
                MediaAudioPlaybackOrigin anchoredOrigin = release->audioOrigin();
                anchoredOrigin.masterRelease = masterRelease.value();
                auto reanchored =
                    MediaStartupReleaseTransactionBuffer::reanchor(
                        *transaction, anchoredEpoch, anchoredOrigin);
                if (!reanchored) return failTerminal(reanchored.error());
                if (auto published = m_preparationCapability->publishInitialAnchor(
                        anchoredEpoch.generation,
                        transaction->releaseIdentity(), anchoredEpoch,
                        anchoredOrigin); !published) {
                    return failTerminal(published.error());
                }
                m_reanchoredTransaction = std::move(reanchored).value();
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
            if (!preparation.anchoredAudioOrigin ||
                !preparation.extractorOutputsReanchored) {
                return ::media::Result<MediaNodeProcessResult>::success(
                    MediaNodeProcessResult::waiting());
            }
        }
        const auto* activationTransaction = m_preparationCapability
            ? dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
                  m_reanchoredTransaction.get())
            : transaction;
        const auto* activationRelease = activationTransaction
            ? activationTransaction->release() : nullptr;
        if (!activationRelease) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer lost its anchored initial release"));
        }
        auto event = MediaPlaybackEpochActivatedBuffer::create(
            m_groupKey, activationRelease->epoch(),
            activationRelease->audioOrigin(), std::nullopt);
        if (!event) {
            return failTerminal(event.error());
        }
        eventForCommit = std::move(event).value();
        activateInitial = true;
        break;
    }
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough: {
        activationGroup = context.findAvSyncGroup(m_groupKey);
        if (!activationGroup) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "Activation release sequencer requires its A/V sync group"));
        }
        const auto disposition = activationGroup->classifyStartupRelease(
            release->releaseKind(),
            release->epoch().generation,
            release->completedTransitionSequence());
        if (!disposition) return failTerminal(disposition.error());
        if (disposition.value() ==
            MediaAvStartupReleaseDisposition::DropOld) {
            m_pendingTransaction.reset();
            m_reanchoredTransaction.reset();
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        if (disposition.value() ==
            MediaAvStartupReleaseDisposition::Withhold) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
        if (disposition.value() ==
            MediaAvStartupReleaseDisposition::Reject) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer rejects a release outside the live playback epoch"));
        }
        const auto* event = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
            m_activatedEvent.get());
        if (!event || event->groupKey() != release->groupKey() ||
            event->epoch().generation != release->epoch().generation ||
            event->epoch().sourceStart != release->epoch().sourceStart) {
            return failTerminal(
                ::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer rejects pass-through before matching activation"));
        }
        if (m_preparationCapability) {
            auto reanchored = MediaStartupReleaseTransactionBuffer::reanchor(
                *transaction, event->epoch(), event->audioOrigin());
            if (!reanchored) return failTerminal(reanchored.error());
            m_reanchoredTransaction = std::move(reanchored).value();
        }
        break;
    }
    case MediaAvStartupReleaseKind::NextAtomicRelease: {
        if (!release->completedTransitionSequence()) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer requires a completed transition sequence"));
        }
        activationGroup = context.findAvSyncGroup(m_groupKey);
        const auto reacquisition = activationGroup
            ? activationGroup->reacquisitionSnapshot()
            : MediaAvReacquisitionSnapshot{
                  MediaAvReacquisitionPhase::Inactive,
                  std::nullopt,
                  std::nullopt};
        const auto transition = activationGroup
            ? activationGroup->epochTransitionSnapshot()
            : MediaAvEpochTransitionSnapshot{
                  MediaAvGenerationReadiness::Acquiring,
                  std::nullopt,
                  std::nullopt,
                  false,
                  true};
        if (!activationGroup ||
            reacquisition.phase !=
                MediaAvReacquisitionPhase::ReadyForActivation ||
            !reacquisition.transition ||
            reacquisition.transition->nextGeneration !=
                release->epoch().generation ||
            reacquisition.transition->transitionSequence !=
                *release->completedTransitionSequence() ||
            transition.poisoned ||
            transition.readiness !=
                MediaAvGenerationReadiness::Acquiring ||
            transition.outputPermitted) {
            if (activationGroup) activationGroup->markAborted();
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer rejects a mismatched next-epoch transition"));
        }
        auto event = MediaPlaybackEpochActivatedBuffer::create(
            m_groupKey, release->epoch(), release->audioOrigin(),
            release->completedTransitionSequence());
        if (!event) {
            activationGroup->markAborted();
            return failTerminal(event.error());
        }
        eventForCommit = std::move(event).value();
        activateNext = true;
        break;
    }
    }

    const MediaBufferRef& bound = m_reanchoredTransaction
        ? m_reanchoredTransaction : m_pendingTransaction;
    std::vector<MediaAtomicOutputBatch> batches;
    const bool publishesActivation = activateInitial || activateNext;
    batches.reserve(
        (publishesActivation ? eventChannels.value().size() : 0) + 1);
    if (publishesActivation) {
        for (MediaChannel* channel : eventChannels.value()) {
            batches.push_back({channel, std::span(&eventForCommit, 1)});
        }
    }
    batches.push_back({releaseChannels.value().front(),
                       std::span(&bound, 1)});
    auto atomic = MediaAtomicOutputTransaction::acquire(
        "Activation release sequencer", batches);
    if (!atomic) return failTerminal(atomic.error());
    if (!atomic.value()) return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::waiting());
    std::optional<MediaAvReacquisitionActivationReservation>
        activationReservation;
    std::optional<MediaAvStartupReleasePublicationReservation>
        publicationReservation;
    if (release->releaseKind() ==
        MediaAvStartupReleaseKind::ActiveEpochPassThrough) {
        auto reserved = activationGroup->reserveStartupReleasePublication(
            release->releaseKind(),
            release->epoch().generation,
            release->completedTransitionSequence());
        if (!reserved) return failTerminal(reserved.error());
        publicationReservation.emplace(std::move(reserved).value());
        if (publicationReservation->disposition() ==
            MediaAvStartupReleaseDisposition::DropOld) {
            m_pendingTransaction.reset();
            m_reanchoredTransaction.reset();
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::progress());
        }
        if (publicationReservation->disposition() ==
            MediaAvStartupReleaseDisposition::Withhold) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
        if (publicationReservation->disposition() ==
            MediaAvStartupReleaseDisposition::Reject) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer lost live playback publication authorization"));
        }
    }
    if (activateInitial) {
        if (m_preparationCapability) {
            if (auto committed = m_preparationCapability->authorizeRelease(
                    release->epoch().generation,
                    transaction->releaseIdentity(), [&] {
                        return m_capability.activateInitial(
                            dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
                                m_reanchoredTransaction.get())->release()->epoch(),
                            dynamic_cast<const MediaStartupReleaseTransactionBuffer*>(
                                m_reanchoredTransaction.get())->release()->audioOrigin());
                    }); !committed) {
                return failTerminal(committed.error());
            }
        } else if (auto activated = m_capability.activateInitial(
                       release->epoch(), release->audioOrigin()); !activated) {
            return failTerminal(activated.error());
        }
        activationGroup = context.findAvSyncGroup(m_groupKey);
        if (!activationGroup) {
            return failTerminal(::media::ErrorInfo::notInitialized(
                "Activation release sequencer requires its A/V sync group"));
        }
        auto reserved = activationGroup->reserveStartupReleasePublication(
            release->releaseKind(),
            release->epoch().generation,
            release->completedTransitionSequence());
        if (!reserved) return failTerminal(reserved.error());
        publicationReservation.emplace(std::move(reserved).value());
        if (publicationReservation->disposition() !=
            MediaAvStartupReleaseDisposition::Publish) {
            activationGroup->markAborted();
            return failTerminal(::media::ErrorInfo::cancelled(
                "Activation release sequencer initial epoch lost publication authorization"));
        }
        m_activatedEvent = eventForCommit;
    } else if (activateNext) {
        auto reserved = activationGroup->reserveReacquisitionActivation(
            release->epoch().generation,
            *release->completedTransitionSequence());
        if (!reserved) {
            activationGroup->markAborted();
            return failTerminal(reserved.error());
        }
        activationReservation.emplace(std::move(reserved).value());
        if (auto activated = m_capability.activateNext(
                release->epoch(), release->audioOrigin(),
                *release->completedTransitionSequence()); !activated) {
            activationReservation->abandon();
            activationGroup->markAborted();
            return failTerminal(activated.error());
        }
        if (auto authorized =
                activationReservation->authorizePublication();
            !authorized) {
            activationReservation->abandon();
            return failTerminal(authorized.error());
        }
        if (auto finalized =
                activationReservation->finalizePublication();
            !finalized) {
            activationReservation->abandon();
            return failTerminal(finalized.error());
        }
        m_activatedEvent = eventForCommit;
    }
    if (activateNext) {
        atomic.value()->commitReserved();
        activationReservation->completePublished();
    } else if (publicationReservation) {
        atomic.value()->commitReserved();
        publicationReservation->completePublished();
    } else if (auto committed = atomic.value()->commit(); !committed) {
        return failTerminal(committed.error());
    }
    m_pendingTransaction.reset();
    m_reanchoredTransaction.reset();
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult>
MediaActivatedStartupReleaseSequencerNode::failTerminal(
    ::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(
        *m_terminalFailure);
}

::media::Status MediaActivatedStartupReleaseSequencerNode::stop(
    MediaGraphExecutionContext& context)
{
    if (m_preparationCapability) m_preparationCapability->cancel();
    m_pendingTransaction.reset();
    m_activatedEvent.reset();
    m_reanchoredTransaction.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Activation release sequencer was stopped");
    }
    return MediaRuntimeNode::stop(context);
}

void MediaActivatedStartupReleaseSequencerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    if (m_preparationCapability) m_preparationCapability->cancel();
    m_pendingTransaction.reset();
    m_activatedEvent.reset();
    m_reanchoredTransaction.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Activation release sequencer was aborted");
    }
    MediaRuntimeNode::abort(context);
}

const MediaAvSyncGroupKey&
MediaActivatedStartupReleaseSequencerNode::groupKey() const noexcept
{
    return m_groupKey;
}

} // namespace media::ffmpeg::graph
