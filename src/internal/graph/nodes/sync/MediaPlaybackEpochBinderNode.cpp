#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaAtomicOutputTransaction.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <array>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

std::vector<MediaChannel*> channelsForPort(
    MediaGraphExecutionContext& context,
    MediaNodeId nodeId,
    const char* portName)
{
    const MediaGraph* graph = context.graph();
    const MediaPort* port = graph ? graph->findOutputPort(nodeId, portName)
                                  : nullptr;
    std::vector<MediaChannel*> channels;
    if (!port) return channels;
    for (MediaChannel* channel : context.outputChannels(nodeId)) {
        if (channel && channel->binding().from.portId == port->id)
            channels.push_back(channel);
    }
    return channels;
}

} // namespace

MediaPlaybackEpochBinderNode::MediaPlaybackEpochBinderNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey)
    : m_nodeId(nodeId)
    , m_groupKey(std::move(groupKey))
{
}

MediaNodeKind MediaPlaybackEpochBinderNode::staticKind() noexcept
{
    return MediaNodeKind::PlaybackEpochBinder;
}

MediaNodeId MediaPlaybackEpochBinderNode::nodeId() const noexcept
{
    return m_nodeId;
}

::media::Result<MediaNodeProcessResult>
MediaPlaybackEpochBinderNode::process(MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    if (!m_pendingRelease) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "release"),
            "Playback epoch binder", "release");
        if (!input) {
            return failTerminal(input.error());
        }
        if (!input.value()) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
        m_pendingRelease = std::move(*input.value());
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        m_pendingRelease.get());
    const auto* control = dynamic_cast<const MediaControlBuffer*>(
        m_pendingRelease.get());
    if (!release && !control) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Playback epoch binder requires a typed release or control"));
    }
    if (release) {
        if (release->groupKey() != m_groupKey) {
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Playback epoch binder rejects mismatched startup release"));
        }
        if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
                release->releaseKind()); !status) {
            return failTerminal(status.error());
        }
    }
    auto transactionOutputs = channelsForPort(context, nodeId(), "transaction");
    if (transactionOutputs.empty()) {
        return failTerminal(::media::ErrorInfo::notInitialized(
            "Playback epoch binder requires a transaction output"));
    }
    if (!m_pendingTransaction) {
        auto transaction = release
            ? MediaStartupReleaseTransactionBuffer::create(m_pendingRelease)
            : MediaStartupReleaseTransactionBuffer::createControl(m_pendingRelease);
        if (!transaction) return failTerminal(transaction.error());
        m_pendingTransaction = std::move(transaction).value();
    }

    const bool initial = release && release->releaseKind() ==
        MediaAvStartupReleaseKind::InitialAtomicRelease;
    auto preparationOutputs = initial
        ? channelsForPort(context, nodeId(), "preparation")
        : std::vector<MediaChannel*>{};
    std::vector<MediaAtomicOutputBatch> batches;
    batches.reserve(transactionOutputs.size() + preparationOutputs.size());
    const std::span<const MediaBufferRef> one(&m_pendingTransaction, 1);
    for (MediaChannel* channel : transactionOutputs)
        batches.push_back({channel, one});
    for (MediaChannel* channel : preparationOutputs)
        batches.push_back({channel, one});
    auto atomic = MediaAtomicOutputTransaction::acquire(
        "Playback epoch binder", batches);
    if (!atomic) return failTerminal(atomic.error());
    if (!atomic.value()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    if (auto committed = atomic.value()->commit(); !committed)
        return failTerminal(committed.error());
    const bool finished = control &&
        (control->controlKind() == MediaControlBufferKind::Eof ||
         control->controlKind() == MediaControlBufferKind::Abort);
    m_pendingRelease.reset();
    m_pendingTransaction.reset();
    return ::media::Result<MediaNodeProcessResult>::success(
        finished ? MediaNodeProcessResult::finished()
                 : MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult>
MediaPlaybackEpochBinderNode::failTerminal(::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(
        *m_terminalFailure);
}

::media::Status MediaPlaybackEpochBinderNode::stop(
    MediaGraphExecutionContext& context)
{
    m_pendingRelease.reset();
    m_pendingTransaction.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Playback epoch binder was stopped");
    }
    return MediaRuntimeNode::stop(context);
}

void MediaPlaybackEpochBinderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_pendingRelease.reset();
    m_pendingTransaction.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Playback epoch binder was aborted");
    }
    MediaRuntimeNode::abort(context);
}

const MediaAvSyncGroupKey& MediaPlaybackEpochBinderNode::groupKey() const noexcept
{
    return m_groupKey;
}

} // namespace media::ffmpeg::graph
