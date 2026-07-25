#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerPendingCommit.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerHead.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaVideoSyncController.h"

#include <memory>
#include <optional>
#include <functional>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;

class MediaAvOutputSchedulerNode final : public FFmpegNodeRuntime {
public:
    using VideoControllerFactory = std::function<
        MediaAvSyncResult<MediaVideoSyncController>(
            const MediaAvSyncPlan&, std::uint64_t)>;

    explicit MediaAvOutputSchedulerNode(MediaNodeId nodeId);
    MediaAvOutputSchedulerNode(MediaNodeId nodeId,
                               VideoControllerFactory controllerFactory);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    enum class Input { Video, Audio };
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status configureActiveScheduling();
    ::media::Status preflightInputAbort(
        MediaGraphExecutionContext& context) noexcept;
    ::media::Result<bool> fillHead(MediaGraphExecutionContext& context,
                                   Input input);
    ::media::Result<std::optional<Input>> arbitrateControlHeads();
    ::media::Result<bool> preflightGenerations();
    ::media::Result<std::optional<Input>> selectMediaHead() const;
    ::media::Result<MediaNodeProcessResult> processSelected(
        MediaGraphExecutionContext& context, Input input);
    ::media::Result<MediaNodeProcessResult> processVideo(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> processAudio(
        MediaGraphExecutionContext& context);
    ::media::Result<MediaNodeProcessResult> processTerminal(
        MediaGraphExecutionContext& context, Input input);
    ::media::Result<MediaNodeProcessResult> emitWithCommit(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& output,
        MediaAvSchedulerPendingCommit commit);
    MediaNodeProcessResult applyCommit(MediaAvSchedulerPendingCommit commit);
    void logFirstMediaHead(Input input);
    void logMissingMediaWait();
    void clearSchedulingState() noexcept;
    void resetState() noexcept;

    std::optional<MediaAvSyncGroupKey> m_groupKey;
    MediaRunningTime m_transportLead = MediaRunningTime::fromNanoseconds(0);
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    VideoControllerFactory m_videoControllerFactory;
    std::unique_ptr<MediaVideoSyncController> m_videoController;
    std::optional<MediaAvSchedulerHead> m_videoHead;
    std::optional<MediaAvSchedulerHead> m_audioHead;
    MediaBufferRef m_terminal;
    MediaBufferRef m_lastDisplayedVideoClone;
    std::optional<MediaSourceAccessUnitSequence> m_lastDisplayedVideoSequence;
    std::optional<MediaRunningTime> m_lastDisplayedVideoMasterTime;
    std::optional<std::uint64_t> m_heldControllerSequence;
    std::optional<MediaAvSchedulerPendingCommit> m_pendingCommit;
    std::optional<std::uint64_t> m_nextControllerSequence{1};
    bool m_videoEof = false;
    bool m_audioEof = false;
    bool m_nextEqualTimeVideo = false;
    bool m_firstVideoHeadDiagnosticEmitted = false;
    bool m_firstAudioHeadDiagnosticEmitted = false;
    std::optional<Input> m_missingMediaWait;
};

} // namespace media::ffmpeg::graph
