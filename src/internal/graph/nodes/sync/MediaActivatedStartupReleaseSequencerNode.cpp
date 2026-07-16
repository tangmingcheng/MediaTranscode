#include "internal/graph/nodes/sync/MediaActivatedStartupReleaseSequencerNode.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

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

::media::Result<bool> preflight(const std::vector<MediaChannel*>& channels)
{
    for (const MediaChannel* channel : channels) {
        if (channel->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer requires blocking outputs"));
        }
        if (channel->aborted()) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "Activation release sequencer output is aborted"));
        }
        if (channel->closed()) {
            return ::media::Result<bool>::failure(
                ::media::ErrorInfo::cancelled(
                    "Activation release sequencer output is closed"));
        }
        if (channel->size() >= channel->capacity()) {
            return ::media::Result<bool>::success(false);
        }
    }
    return ::media::Result<bool>::success(true);
}

::media::Status commit(MediaChannel& channel, const MediaBufferRef& buffer)
{
    const auto outcome = channel.pushOutcome(buffer);
    if (outcome == MediaQueuePushOutcome::Accepted) {
        return ::media::Status::success();
    }
    return ::media::Status::failure(
        outcome == MediaQueuePushOutcome::Closed ||
                outcome == MediaQueuePushOutcome::Aborted
            ? ::media::ErrorInfo::cancelled(
                  "Activation release sequencer output closed during commit")
            : ::media::ErrorInfo::internalError(
                  "Activation release sequencer preflight invariant failed"));
}

} // namespace

MediaActivatedStartupReleaseSequencerNode::
    MediaActivatedStartupReleaseSequencerNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpochActivationCapability capability)
    : m_nodeId(nodeId)
    , m_groupKey(std::move(groupKey))
    , m_capability(std::move(capability))
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
    if (!transaction || transaction->groupKey() != m_groupKey) {
        return failTerminal(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer rejects a mismatched transaction"));
    }

    auto eventChannels = channelsForPort(context, nodeId(), "activated");
    auto releaseChannels = channelsForPort(context, nodeId(), "bound_release");
    if (!eventChannels) {
        return failTerminal(eventChannels.error());
    }
    if (!releaseChannels) {
        return failTerminal(releaseChannels.error());
    }
    if (releaseChannels.value().size() != 1) {
        return failTerminal(
            ::media::ErrorInfo::invalidArgument(
                "Activation release sequencer requires one release target"));
    }
    auto eventReady = preflight(eventChannels.value());
    auto releaseReady = preflight(releaseChannels.value());
    if (!eventReady) {
        return failTerminal(eventReady.error());
    }
    if (!releaseReady) {
        return failTerminal(releaseReady.error());
    }
    if (!eventReady.value() || !releaseReady.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }

    switch (transaction->releaseKind()) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease: {
        if (m_activatedEvent) {
            return failTerminal(
                ::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer rejects duplicate initial activation"));
        }
        auto event = MediaPlaybackEpochActivatedBuffer::create(
            m_groupKey, transaction->epoch(), transaction->audioOrigin());
        if (!event) {
            return failTerminal(event.error());
        }
        if (auto activated = m_capability.activateInitial(
                transaction->epoch(), transaction->audioOrigin()); !activated) {
            return failTerminal(activated.error());
        }
        m_activatedEvent = std::move(event).value();
        break;
    }
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough: {
        const auto* event = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
            m_activatedEvent.get());
        if (!event || event->groupKey() != transaction->groupKey() ||
            event->epoch() != transaction->epoch() ||
            event->audioOrigin() != transaction->audioOrigin()) {
            return failTerminal(
                ::media::ErrorInfo::invalidArgument(
                    "Activation release sequencer rejects pass-through before matching activation"));
        }
        break;
    }
    }

    for (MediaChannel* channel : eventChannels.value()) {
        if (auto status = commit(*channel, m_activatedEvent); !status) {
            return failTerminal(status.error());
        }
    }
    if (auto status = commit(*releaseChannels.value().front(),
                             transaction->release()); !status) {
        return failTerminal(status.error());
    }
    m_pendingTransaction.reset();
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
    m_pendingTransaction.reset();
    m_activatedEvent.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Activation release sequencer was stopped");
    }
    return MediaRuntimeNode::stop(context);
}

void MediaActivatedStartupReleaseSequencerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_pendingTransaction.reset();
    m_activatedEvent.reset();
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
