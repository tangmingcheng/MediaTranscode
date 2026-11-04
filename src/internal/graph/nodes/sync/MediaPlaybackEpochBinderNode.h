#pragma once

#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include "internal/graph/model/MediaStartupSharedObjectPlan.h"
#include <optional>

namespace media::ffmpeg::graph {

class MediaPlaybackEpochBinderNode final : public MediaRuntimeNode {
public:
    MediaPlaybackEpochBinderNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaStartupSharedObjectPlan releaseStorage,
        MediaStartupSharedObjectPlan controlStorage);

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

    MediaStartupSharedObjectPlan m_releaseStorage;
    MediaStartupSharedObjectPlan m_controlStorage;
    MediaNodeId m_nodeId;
    MediaAvSyncGroupKey m_groupKey;
    MediaBufferRef m_pendingRelease;
    MediaBufferRef m_pendingTransaction;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
