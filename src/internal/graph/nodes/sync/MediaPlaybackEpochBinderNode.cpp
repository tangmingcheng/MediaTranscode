#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
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
    if (!m_pendingRelease) {
        MediaChannel* input = context.findInputChannel(nodeId(), "release");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "Playback epoch binder requires a release input"));
        }
        if (!input->tryPop(m_pendingRelease)) {
            return ::media::Result<MediaNodeProcessResult>::success(
                MediaNodeProcessResult::waiting());
        }
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        m_pendingRelease.get());
    if (!release || release->groupKey() != m_groupKey) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder rejects mismatched startup release"));
    }
    if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
            release->releaseKind()); !status) {
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    }
    MediaChannel* output = context.findOutputChannel(nodeId(), "transaction");
    if (!output) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Playback epoch binder requires a transaction output"));
    }
    if (output->policy().queuePolicy.overflowPolicy !=
        MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder requires a blocking transaction output"));
    }
    if (output->closed()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Playback epoch binder transaction output is closed"));
    }
    if (output->size() >= output->capacity()) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    auto transaction = MediaStartupReleaseTransactionBuffer::create(
        m_pendingRelease);
    if (!transaction) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            transaction.error());
    }
    const auto outcome = output->pushOutcome(std::move(transaction).value());
    if (outcome == MediaQueuePushOutcome::WouldBlock) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    if (outcome != MediaQueuePushOutcome::Accepted) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            outcome == MediaQueuePushOutcome::Closed
                ? ::media::ErrorInfo::cancelled(
                      "Playback epoch binder transaction output closed during commit")
                : ::media::ErrorInfo::internalError(
                      "Playback epoch binder transaction commit failed"));
    }
    m_pendingRelease.reset();
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::progress());
}

::media::Status MediaPlaybackEpochBinderNode::stop(
    MediaGraphExecutionContext& context)
{
    m_pendingRelease.reset();
    return MediaRuntimeNode::stop(context);
}

void MediaPlaybackEpochBinderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_pendingRelease.reset();
    MediaRuntimeNode::abort(context);
}

const MediaAvSyncGroupKey& MediaPlaybackEpochBinderNode::groupKey() const noexcept
{
    return m_groupKey;
}

} // namespace media::ffmpeg::graph
