#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerPendingCommit.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerHead.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaVideoSyncController.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <functional>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaAvGenerationPurgeTarget;
enum class MediaAvSchedulerInput { Video, Audio };

class MediaAvSchedulerGenerationState final
    : public MediaProtocolOutputGenerationSessionState {
private:
    friend class MediaAvOutputSchedulerNode;
    void resetForGenerationPurge() noexcept override
    {
        videoController.reset();
        activeGeneration.reset();
        videoHead.reset();
        audioHead.reset();
        terminal.reset();
        lastDisplayedVideoClone.reset();
        lastDisplayedVideoSequence.reset();
        lastDisplayedVideoMasterTime.reset();
        heldControllerSequence.reset();
        pendingCommit.reset();
        completedCommitResult.reset();
        nextControllerSequence = 1;
        videoEof = false;
        audioEof = false;
        nextEqualTimeVideo = false;
        firstVideoHeadDiagnosticEmitted = false;
        firstAudioHeadDiagnosticEmitted = false;
        missingMediaWait.reset();
    }

    std::unique_ptr<MediaVideoSyncController> videoController;
    std::optional<std::uint64_t> activeGeneration;
    std::optional<MediaAvSchedulerHead> videoHead;
    std::optional<MediaAvSchedulerHead> audioHead;
    MediaBufferRef terminal;
    MediaBufferRef lastDisplayedVideoClone;
    std::optional<MediaSourceAccessUnitSequence> lastDisplayedVideoSequence;
    std::optional<MediaRunningTime> lastDisplayedVideoMasterTime;
    std::optional<std::uint64_t> heldControllerSequence;
    std::optional<MediaAvSchedulerPendingCommit> pendingCommit;
    std::optional<MediaNodeProcessResult> completedCommitResult;
    std::optional<std::uint64_t> nextControllerSequence{1};
    bool videoEof = false;
    bool audioEof = false;
    bool nextEqualTimeVideo = false;
    bool firstVideoHeadDiagnosticEmitted = false;
    bool firstAudioHeadDiagnosticEmitted = false;
    std::optional<MediaAvSchedulerInput> missingMediaWait;
};

class MediaAvOutputSchedulerNode final : public FFmpegNodeRuntime {
public:
    using VideoControllerFactory = std::function<
        MediaAvSyncResult<MediaVideoSyncController>(
            const MediaAvSyncPlan&, std::uint64_t)>;

    explicit MediaAvOutputSchedulerNode(MediaNodeId nodeId);
    MediaAvOutputSchedulerNode(MediaNodeId nodeId,
                               VideoControllerFactory controllerFactory);
    static MediaNodeKind staticKind() noexcept;
    static constexpr std::string_view generationPurgeIdentity() noexcept
    {
        return "scheduler_generation_state";
    }
    std::shared_ptr<MediaAvGenerationPurgeTarget>
    generationPurgeTarget() const noexcept;
    ::media::Status start(MediaGraphExecutionContext& context) override;
    ::media::Result<MediaNodeProcessResult> process(
        MediaGraphExecutionContext& context) override;
    ::media::Status flush(MediaGraphExecutionContext& context) override;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Result<
        std::optional<MediaProtocolOutputGenerationCommitReservation>>
    reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    using Input = MediaAvSchedulerInput;
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
    std::shared_ptr<MediaAvSchedulerGenerationState> m_generationSession;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    VideoControllerFactory m_videoControllerFactory;
    std::unique_ptr<MediaVideoSyncController>& m_videoController;
    std::optional<std::uint64_t>& m_activeGeneration;
    std::optional<MediaAvSchedulerHead>& m_videoHead;
    std::optional<MediaAvSchedulerHead>& m_audioHead;
    MediaBufferRef& m_terminal;
    MediaBufferRef& m_lastDisplayedVideoClone;
    std::optional<MediaSourceAccessUnitSequence>&
        m_lastDisplayedVideoSequence;
    std::optional<MediaRunningTime>& m_lastDisplayedVideoMasterTime;
    std::optional<std::uint64_t>& m_heldControllerSequence;
    std::optional<MediaAvSchedulerPendingCommit>& m_pendingCommit;
    std::optional<MediaNodeProcessResult>& m_completedCommitResult;
    std::optional<std::uint64_t>& m_nextControllerSequence;
    bool& m_videoEof;
    bool& m_audioEof;
    bool& m_nextEqualTimeVideo;
    bool& m_firstVideoHeadDiagnosticEmitted;
    bool& m_firstAudioHeadDiagnosticEmitted;
    std::optional<Input>& m_missingMediaWait;
};

} // namespace media::ffmpeg::graph
