#pragma once

#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpochActivationCapability.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaActivatedStartupReleaseSequencerNode final : public MediaRuntimeNode {
public:
    MediaActivatedStartupReleaseSequencerNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaPlaybackEpochActivationCapability capability,
        MediaRunningTime outputLead,
        std::optional<MediaAvStartupVideoPreparationCapability>
            preparationCapability = std::nullopt);

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
    MediaRunningTime m_outputLead;
    std::optional<MediaAvStartupVideoPreparationCapability>
        m_preparationCapability;
    MediaBufferRef m_pendingTransaction;
    MediaBufferRef m_activatedEvent;
    MediaBufferRef m_reanchoredTransaction;
    std::optional<::media::ErrorInfo> m_terminalFailure;
};

} // namespace media::ffmpeg::graph
