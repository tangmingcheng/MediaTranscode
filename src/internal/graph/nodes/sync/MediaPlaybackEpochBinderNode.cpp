#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

namespace media::ffmpeg::graph {

// The binder is the sole runtime owner of activation authority.

MediaPlaybackEpochBinderNode::MediaPlaybackEpochBinderNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey groupKey,
    MediaPlaybackEpochActivationCapability capability)
    : m_nodeId(nodeId)
    , m_groupKey(std::move(groupKey))
    , m_capability(std::move(capability))
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
    MediaChannel* input = context.findInputChannel(nodeId(), "release");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    MediaBufferRef buffer;
    if (!input->tryPop(buffer)) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waiting());
    }
    const auto* release = dynamic_cast<const MediaAvStartupReleaseBuffer*>(buffer.get());
    if (!release || release->groupKey() != m_groupKey) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch binder rejects mismatched startup release"));
    }
    if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
            release->releaseKind()); !status) {
        return ::media::Result<MediaNodeProcessResult>::failure(status.error());
    }
    switch (release->releaseKind()) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease: {
        auto status = m_capability.activateInitial(release->epoch(),
                                                   release->audioOrigin());
        if (!status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
        break;
    }
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough:
        break;
    }
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::progress());
}

const MediaAvSyncGroupKey& MediaPlaybackEpochBinderNode::groupKey() const noexcept
{
    return m_groupKey;
}

::media::Status MediaPlaybackEpochBinderNode::publishInitial(
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin)
{
    return m_capability.activateInitial(epoch, audioOrigin);
}

::media::Status MediaPlaybackEpochBinderNode::publishNext(
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin,
    std::uint64_t completedTransitionSequence)
{
    return m_capability.activateNext(epoch, audioOrigin,
                                     completedTransitionSequence);
}

} // namespace media::ffmpeg::graph
