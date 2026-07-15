#include "internal/graph/nodes/sync/MediaPlaybackEpochBinderNode.h"

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
MediaPlaybackEpochBinderNode::process(MediaGraphExecutionContext&)
{
    return ::media::Result<MediaNodeProcessResult>::success(
        MediaNodeProcessResult::waiting());
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
