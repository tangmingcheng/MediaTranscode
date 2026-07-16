#pragma once

#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

class MediaPlaybackEpochBinderNode final : public MediaRuntimeNode {
public:
    MediaPlaybackEpochBinderNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpochActivationCapability capability);

    static MediaNodeKind staticKind() noexcept;
    MediaNodeId nodeId() const noexcept override;
    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

    const MediaAvSyncGroupKey& groupKey() const noexcept;
    ::media::Status publishInitial(MediaPlaybackEpoch epoch,
                                   MediaAudioPlaybackOrigin audioOrigin);
    ::media::Status publishNext(MediaPlaybackEpoch epoch,
                                MediaAudioPlaybackOrigin audioOrigin,
                                std::uint64_t completedTransitionSequence);

private:
    MediaNodeId m_nodeId;
    MediaAvSyncGroupKey m_groupKey;
    MediaPlaybackEpochActivationCapability m_capability;
    MediaBufferRef m_pendingRelease;
    MediaBufferRef m_pendingActivatedEvent;
    bool m_activationCommitted = false;
    bool m_activatedEventCommitted = false;
};

} // namespace media::ffmpeg::graph
