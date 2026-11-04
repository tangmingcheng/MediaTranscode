#pragma once
#include "internal/graph/utils/MediaCleanupError.h"
#include "internal/graph/diagnostics/MediaAudioStageObservation.h"

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerPendingCommit.h"
#include "internal/graph/nodes/sync/MediaAvSchedulerHead.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <memory>
#include <optional>
#include <string_view>
#include <atomic>

namespace media::ffmpeg::graph {

class MediaAvSyncGroupRuntime;
struct MediaAvReacquisitionSnapshot;
struct MediaAvEpochTransitionSnapshot;
class MediaProtocolOutputRuntimeAuthority;
class MediaAvGenerationPurgeTarget;
enum class MediaAvSchedulerInput { Video, Audio };

class MediaAvSchedulerGenerationState final
    : public MediaProtocolOutputGenerationSessionState {
private:
    friend class MediaAvOutputSchedulerNode;
    struct Data final {
        std::optional<std::uint64_t> activeGeneration;
        std::optional<MediaAvSchedulerHead> videoHead;
        std::optional<MediaAvSchedulerHead> audioHead;
        MediaBufferRef terminal;
        std::optional<MediaAvSchedulerPendingCommit> pendingCommit;
        std::optional<MediaNodeProcessResult> completedCommitResult;
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
        return std::atomic_load_explicit(
            &m_current, std::memory_order_acquire);
    }

    ::media::Status prepareForGenerationPurge() override
    {
        try {
            std::atomic_store_explicit(
                &m_prepared,
                std::make_shared<Data>(),
                std::memory_order_release);
        } catch (const std::bad_alloc&) {
            return ::media::Status::failure(
                mediaCleanupError(::media::ErrorCode::AllocationFailed,
                    "A/V scheduler could not allocate generation session state", 0));
        }
        return ::media::Status::success();
    }

    void resetForGenerationPurge() noexcept override
    {
        auto replacement = std::atomic_exchange_explicit(
            &m_prepared,
            std::shared_ptr<Data>{},
            std::memory_order_acq_rel);
        std::atomic_store_explicit(
            &m_current, std::move(replacement), std::memory_order_release);
    }

    std::shared_ptr<Data> m_current;
    std::shared_ptr<Data> m_prepared;
};

class MediaAvOutputSchedulerNode final : public FFmpegNodeRuntime {
public:
    MediaAvOutputSchedulerNode(
        MediaNodeId nodeId,
        std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority);
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
    using Input = MediaAvSchedulerInput;
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Status configureActiveScheduling();
    ::media::Status preflightInputAbort(
        MediaGraphExecutionContext& context) noexcept;
    ::media::Result<bool> fillHead(MediaGraphExecutionContext& context,
                                   Input input);
    ::media::Result<std::optional<Input>> arbitrateControlHeads();
    ::media::Result<bool> preflightGenerations(
        const MediaAvReacquisitionSnapshot& reacquisition,
        const MediaAvEpochTransitionSnapshot& epoch);
    ::media::Result<std::optional<Input>> selectMediaHead() const;
    ::media::Result<MediaNodeProcessResult> processSelected(
        MediaGraphExecutionContext& context, Input input);
    ::media::Result<MediaNodeProcessResult> processMedia(
        MediaGraphExecutionContext& context, Input input);
    ::media::Result<MediaNodeProcessResult> processTerminal(
        MediaGraphExecutionContext& context, Input input);
    ::media::Result<MediaNodeProcessResult> emitWithCommit(
        MediaGraphExecutionContext& context,
        const MediaBufferRef& output,
        MediaAvSchedulerPendingCommit commit);
    MediaNodeProcessResult applyCommit(MediaAvSchedulerPendingCommit commit);
    void logMediaHeadArrival(Input input);
    void logMissingMediaWait();
    void observeMissingMediaWait();
    void logCorrelatedWait() const noexcept;
    void clearSchedulingState() noexcept;
    ::media::Status resetState();
    void refreshGenerationSession() noexcept;

    std::optional<MediaAvSyncGroupKey> m_groupKey;
    MediaRunningTime m_protocolPreparationLead = MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_transportLead = MediaRunningTime::fromNanoseconds(0);
    std::shared_ptr<MediaAvSyncGroupRuntime> m_group;
    std::shared_ptr<MediaAvSchedulerGenerationState> m_generationSession;
    std::shared_ptr<MediaAvSchedulerGenerationState::Data> m_generationData;
    std::shared_ptr<MediaProtocolOutputGenerationState> m_generationState;
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> m_outputAuthority;
    MediaAudioStageObservation m_audioInputObservation;
    struct MissingWaitObservation {
        std::uint64_t generation;
        std::uint64_t sequence;
        Input missing;
        std::int64_t begin;
        std::int64_t end;
        std::int64_t dispatch;
        std::int64_t beginMaster;
        std::int64_t endMaster;
        std::optional<std::int64_t> resumedDispatch;
    };
    std::optional<MissingWaitObservation> m_currentWait;
    std::optional<MissingWaitObservation> m_maximumWait;
};

} // namespace media::ffmpeg::graph
