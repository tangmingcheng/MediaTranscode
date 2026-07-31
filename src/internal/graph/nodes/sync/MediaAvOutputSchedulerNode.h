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
#include <atomic>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
class MediaAvGenerationPurgeTarget;
struct MediaAvOutputSchedulerNodeTestAccess;
enum class MediaAvSchedulerInput { Video, Audio };

class MediaAvSchedulerGenerationState final
    : public MediaProtocolOutputGenerationSessionState {
private:
    friend class MediaAvOutputSchedulerNode;
    friend struct MediaAvOutputSchedulerNodeTestAccess;

    struct Data final {
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

    MediaAvSchedulerGenerationState()
        : m_current(std::make_shared<Data>())
        , m_prepared(std::make_shared<Data>())
    {
    }

    std::shared_ptr<Data> current() const noexcept
    {
        return m_current.load(std::memory_order_acquire);
    }

    ::media::Status prepareForGenerationPurge() override
    {
        try {
            m_prepared.store(
                std::make_shared<Data>(), std::memory_order_release);
        } catch (const std::bad_alloc&) {
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError(
                    "A/V scheduler could not allocate generation session state"));
        }
        return ::media::Status::success();
    }

    void resetForGenerationPurge() noexcept override
    {
        auto replacement =
            m_prepared.exchange(nullptr, std::memory_order_acq_rel);
        m_current.store(std::move(replacement), std::memory_order_release);
    }

    std::atomic<std::shared_ptr<Data>> m_current;
    std::atomic<std::shared_ptr<Data>> m_prepared;
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
    bool pendingOutputIsCurrent(
        const MediaBufferRef& buffer) const noexcept override;
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;
    ::media::Result<MediaOutputCommitReservation>
    reserveOutputCommit(const MediaBufferRef& buffer) const override;
    ::media::Status commitReservedOutput(
        const MediaBufferRef& buffer) override;
    ::media::Status cancelReservedOutput(
        const MediaBufferRef& buffer) override;

private:
    friend struct MediaAvOutputSchedulerNodeTestAccess;
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
    ::media::Status resetState();
    void refreshGenerationSession() noexcept;

    std::optional<MediaAvSyncGroupKey> m_groupKey;
    MediaRunningTime m_transportLead = MediaRunningTime::fromNanoseconds(0);
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::shared_ptr<MediaAvSchedulerGenerationState> m_generationSession;
    std::shared_ptr<MediaAvSchedulerGenerationState::Data> m_generationData;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    VideoControllerFactory m_videoControllerFactory;
};

} // namespace media::ffmpeg::graph
