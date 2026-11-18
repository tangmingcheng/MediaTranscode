#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/sync/MediaAvStartupReleaseKind.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationCapability.h"
#include "internal/graph/runtime/channel/MediaReservedOutputTransaction.h"

#include <optional>
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"

namespace media::ffmpeg::graph {

class MediaAvStartupReleaseBuffer;

class MediaAvBoundReleaseExtractorNode final : public FFmpegNodeRuntime {
public:
    MediaAvBoundReleaseExtractorNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaBranchMode audioBranchMode);
    MediaAvBoundReleaseExtractorNode(
        MediaNodeId nodeId,
        MediaAvSyncGroupKey groupKey,
        MediaBranchMode audioBranchMode,
        MediaAvStartupVideoPreparationCapability capability);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    std::size_t outputMemberCount() const noexcept;
    ::media::Result<MediaAvStartupReleaseDisposition> commit(
        MediaGraphExecutionContext& context,
        const MediaAvStartupReleaseBuffer& release);
    ::media::Result<MediaAvStartupReleaseDisposition> classifyRelease(
        MediaGraphExecutionContext& context,
        const MediaAvStartupReleaseBuffer& release) const;
    void discardPendingRelease() noexcept;
    ::media::Status stageRelease(const MediaAvStartupReleaseBuffer& release,
                                 std::size_t firstVideoIndex,
                                 const MediaPlaybackActivation& activation);
    ::media::Result<MediaNodeProcessResult> processPreparation(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> processBoundRelease(
        MediaGraphExecutionContext& context);
    void logFirstCommit();
    void resetState() noexcept;
    MediaBufferRef m_pending;
    MediaBufferRef m_preparationTransaction;
    struct StagedRelease final {
        // Arrays are destroyed before the resource owner.
        MediaLineageAllocationScope allocation;
        MediaFixedCapacityStorage<MediaBufferRef> video;
        MediaFixedCapacityStorage<MediaBufferRef> audio;
    };
    std::optional<StagedRelease> m_staged;
    bool m_prefixReservationPending = false;
    std::optional<MediaReservedOutputTransaction> m_initialOutputReservation;
    bool m_firstReleaseDiagnosticEmitted = false;
    bool m_firstCommitDiagnosticEmitted = false;
    MediaAvSyncGroupKey m_groupKey;
    MediaBranchMode m_audioBranchMode;
    std::optional<MediaAvStartupVideoPreparationCapability>
        m_preparationCapability;
};

} // namespace media::ffmpeg::graph
