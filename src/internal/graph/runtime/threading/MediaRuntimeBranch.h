#pragma once

#include "internal/graph/runtime/scheduler/MediaGraphScheduler.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaRuntimeBranchDrainPlan.h"
#include "internal/graph/model/MediaRuntimeReclamationPlan.h"
#include "internal/graph/runtime/threading/MediaRuntimeReclamationOwner.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"

#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeMetrics.h"
#include "internal/graph/runtime/diagnostics/MediaVideoOutputReadyEvidence.h"
#include <chrono>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

// The reservation owner returns all planner-admitted resources only when the
// segment has released its workers, nodes and queued payloads.
class MediaRuntimeBranchResourceReservation {
public:
    virtual ~MediaRuntimeBranchResourceReservation() = default;
};

struct MediaPreparedRuntimeBranch final {
    std::uint64_t id;
    MediaThreadingPolicy threadingPolicy;
    MediaRuntimeReclamationPlan reclamationPlan;
    std::shared_ptr<const MediaGraph> graph;
    std::vector<MediaNodeId> nodeIds;
    std::vector<MediaRuntimeSegmentOutputBinding> upstreamInputs;
    std::vector<std::unique_ptr<MediaRuntimeNode>> nodes;
    std::shared_ptr<MediaRuntimeBranchResourceReservation> resourceLease;
};

enum class MediaRuntimeBranchState { Prepared, Running, Draining, Failed, Retired, Retiring };

class MediaRuntimeBranch final {
public:
    static ::media::Result<std::shared_ptr<MediaRuntimeBranch>> prepare(
        MediaPreparedRuntimeBranch prepared, MediaGraphExecutionContext& session);
    ~MediaRuntimeBranch();
    static std::uint64_t fixedStorageBytes() noexcept;
    MediaRuntimeBranch(const MediaRuntimeBranch&) = delete;
    MediaRuntimeBranch& operator=(const MediaRuntimeBranch&) = delete;

    ::media::Status start();
    MediaChannelPushResult tryPublish(MediaEdgeId edge, MediaBufferRef buffer);
    ::media::Status beginDrain(std::uint64_t generation,
                              const MediaRuntimeBranchDrainPlan& plan);
    // Never joins a worker that has not published thread exit. A failed branch
    // remains owned and charged until its workers have actually exited.
    ::media::Result<bool> poll();
    void fail(::media::ErrorInfo error);
    void fail(MediaGraphWorkerFailure failure);
    std::uint64_t id() const noexcept { return m_id; }
    MediaRuntimeBranchState state() const;
    std::optional<MediaGraphWorkerFailure> failure() const;
    MediaGraphExecutionContext& context() noexcept { return m_context; }
    // The caller retains this branch and serializes lookup with retirement.
    MediaRuntimeNode* findNode(MediaNodeId id) noexcept;
    std::vector<MediaEdgeId> inputEdges() const;
    MediaGraphRuntimeMetrics metrics() const;
    std::optional<MediaVideoOutputReadyEvidence> videoReadyEvidence() const;

private:
    friend class MediaGraphRuntime;
    MediaRuntimeBranch() = default;
    void requestFailureStop() noexcept;
    ::media::Status prepareReclamation(const MediaRuntimeReclamationPlan& plan);
    void reclaim() noexcept;
    MediaGraphRuntimeMetrics collectMetrics() const;
    std::uint64_t drainProgress() const;
    std::optional<MediaVideoOutputReadyEvidence> collectVideoReadyEvidence() const;

    // Declared first, destroyed last: admission covers the branch/owner bodies
    // as well as payloads, including references retained after physical retirement.
    std::shared_ptr<MediaRuntimeBranchResourceReservation> m_resourceLease;
    std::uint64_t m_id = 0;
    MediaThreadingPolicy m_threadingPolicy;
    std::optional<MediaRuntimeReclamationPlan> m_reclamationPlan;
    std::unique_ptr<MediaRuntimeReclamationOwner> m_reclamationOwner;
    std::optional<MediaGraphWorkerFailure> m_unexpectedReleaseFailure;
    std::optional<MediaGraphWorkerFailure> m_releaseSilenceFailure;
    bool m_reclaimAbort = false;
    bool m_reclaimed = false;
    std::chrono::steady_clock::time_point m_lastReleaseProgressAt;
    std::uint64_t m_lastReleaseProgress = 0;
    MediaGraphRuntimeMetrics m_finalMetrics;
    std::optional<MediaVideoOutputReadyEvidence> m_finalVideoReadyEvidence;
    MediaGraphExecutionContext m_context;
    MediaGraphScheduler m_scheduler;
    MediaGraphWorkerFailureRecorder m_failures;
    MediaGraphWorkerFailureSupervisor m_supervisor;
    std::vector<std::unique_ptr<MediaGraphWorker>> m_workers;
    std::vector<MediaChannel*> m_inputs;
    bool m_inputsAccountedBySession = false;
    std::vector<std::shared_ptr<const MediaGraphWorkerExitToken>> m_retirementPrerequisites;
    std::optional<MediaRuntimeBranchDrainPlan> m_drainPlan;
    std::chrono::steady_clock::time_point m_lastDrainProgressAt;
    std::uint64_t m_lastDrainProgress = 0;
    mutable std::mutex m_publicationMutex;
    MediaRuntimeBranchState m_state = MediaRuntimeBranchState::Prepared;
};

} // namespace media::ffmpeg::graph
