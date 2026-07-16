#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

namespace media::ffmpeg::graph {

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
    MediaChannel* output = context.findOutputChannel(nodeId(), "transaction");
    if (!output) {
        return failTerminal(::media::ErrorInfo::notInitialized(
            "Playback epoch binder requires a transaction output"));
    }
    if (output->policy().queuePolicy.overflowPolicy !=
        MediaQueueOverflowPolicy::BlockProducer) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Playback epoch binder requires a blocking transaction output"));
    }
    if (output->closed() || output->aborted()) {
        return failTerminal(::media::ErrorInfo::cancelled(
            "Playback epoch binder transaction output is closed"));
    }
    if (output->size() >= output->capacity()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    auto transaction = release
        ? MediaStartupReleaseTransactionBuffer::create(m_pendingRelease)
        : MediaStartupReleaseTransactionBuffer::createControl(m_pendingRelease);
    if (!transaction) {
        return failTerminal(transaction.error());
    }
    const auto outcome = output->pushOutcome(std::move(transaction).value());
    if (outcome == MediaQueuePushOutcome::WouldBlock) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    if (outcome != MediaQueuePushOutcome::Accepted) {
        return failTerminal(
            outcome == MediaQueuePushOutcome::Closed ||
                    outcome == MediaQueuePushOutcome::Aborted
                ? ::media::ErrorInfo::cancelled(
                      "Playback epoch binder transaction output terminated during commit")
                : ::media::ErrorInfo::internalError(
                      "Playback epoch binder transaction commit failed"));
    }
    const bool finished = control &&
        (control->controlKind() == MediaControlBufferKind::Eof ||
         control->controlKind() == MediaControlBufferKind::Abort);
    m_pendingRelease.reset();
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
