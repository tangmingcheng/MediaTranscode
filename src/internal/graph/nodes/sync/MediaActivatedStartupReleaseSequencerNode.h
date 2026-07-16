#pragma once

#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"

namespace media::ffmpeg::graph {

class MediaActivatedStartupReleaseSequencerNode final : public MediaRuntimeNode {
public:
    MediaActivatedStartupReleaseSequencerNode(
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

private:
    ::media::Result<MediaNodeProcessResult> failTerminal(
        ::media::ErrorInfo error);

    MediaNodeId m_nodeId;
    MediaAvSyncGroupKey m_groupKey;
    MediaPlaybackEpochActivationCapability m_capability;
    MediaBufferRef m_pendingTransaction;
    MediaBufferRef m_activatedEvent;
    bool m_terminalFailure = false;
};

} // namespace media::ffmpeg::graph
