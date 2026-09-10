#include "internal/graph/runtime/threading/MediaRuntimeBranch.h"
#include "internal/graph/nodes/output/MediaScheduledDatagramSenderNode.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/diagnostics/MediaRuntimeMetricsCollector.h"

#include <algorithm>
#include <exception>

namespace media::ffmpeg::graph {

::media::Result<std::shared_ptr<MediaRuntimeBranch>> MediaRuntimeBranch::prepare(
    MediaPreparedRuntimeBranch prepared, MediaGraphExecutionContext& session)
{
    using Result = ::media::Result<std::shared_ptr<MediaRuntimeBranch>>;
    if (!prepared.id || !prepared.resourceLease ||
        prepared.threadingPolicy.mode != MediaThreadingMode::PerNodeWorker ||
        prepared.threadingPolicy.maxWorkerThreads < prepared.nodeIds.size() ||
        prepared.nodes.size() != prepared.nodeIds.size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "runtime branch requires an ID, resource reservation and exact node set"));
    }
    for (const auto& node : prepared.nodes) {
        if (!node || std::find(prepared.nodeIds.begin(), prepared.nodeIds.end(),
                              node->nodeId()) == prepared.nodeIds.end()) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "runtime branch implementation differs from planned node set"));
        }
    }
    auto branch = std::shared_ptr<MediaRuntimeBranch>(new MediaRuntimeBranch());
    branch->m_id = prepared.id;
    branch->m_threadingPolicy = prepared.threadingPolicy;
    branch->m_resourceLease = std::move(prepared.resourceLease);
    auto reclamation = branch->prepareReclamation(prepared.reclamationPlan);
    if (!reclamation) return Result::failure(reclamation.error());
    auto compiled = branch->m_context.compileSegment(
        std::move(prepared.graph), prepared.nodeIds, session, prepared.upstreamInputs);
    if (!compiled) return Result::failure(compiled.error());
    auto registered = branch->m_scheduler.registerNodes(std::move(prepared.nodes));
    if (!registered) return Result::failure(registered.error());
    for (auto* channel : branch->m_context.channels().channels()) {
        if (std::find(prepared.nodeIds.begin(), prepared.nodeIds.end(),
                      channel->binding().from.nodeId) == prepared.nodeIds.end()) {
            branch->m_inputs.push_back(channel);
        }
    }
    return Result::success(std::move(branch));
}

std::uint64_t MediaRuntimeBranch::fixedStorageBytes() noexcept
{
    return sizeof(MediaRuntimeBranch) + MediaRuntimeReclamationOwner::fixedStorageBytes();
}

::media::Status MediaRuntimeBranch::prepareReclamation(const MediaRuntimeReclamationPlan& plan)
{
    if (m_reclamationOwner || plan.mode != MediaRuntimeReclamationMode::DedicatedSingleShotOwner ||
        plan.ownerThreads != 1 || plan.commandSlots != 1 ||
        plan.fixedStorageBytes != fixedStorageBytes() ||
        plan.maximumProgressSilence.nanoseconds() <= 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime branch requires the exact planned single-shot reclamation owner"));
    m_reclamationPlan = plan;
    m_unexpectedReleaseFailure = MediaGraphWorkerFailure{{}, MediaNodeKind::Unknown,
        "output branch reclamation", ::media::ErrorInfo::internalError(
            "physical branch reclamation raised an exception; resources remain owned"),
        MediaGraphWorkerFailurePhase::Release};
    m_releaseSilenceFailure = MediaGraphWorkerFailure{{}, MediaNodeKind::Unknown,
        "output branch reclamation", ::media::ErrorInfo::internalError(
            "physical branch reclamation exceeded its planned progress silence; resources remain owned"),
        MediaGraphWorkerFailurePhase::Release};
    auto owner = MediaRuntimeReclamationOwner::create(this, [](void* branch) noexcept {
        static_cast<MediaRuntimeBranch*>(branch)->reclaim();
    });
    if (!owner) return ::media::Status::failure(owner.error());
    m_reclamationOwner = std::move(owner).value();
    return ::media::Status::success();
}

MediaRuntimeBranch::~MediaRuntimeBranch()
{
    // Normal control flow retains the segment until poll observes completion.
    // Final owner destruction still joins: driver resources must never detach.
    if (m_state != MediaRuntimeBranchState::Retired &&
        m_state != MediaRuntimeBranchState::Retiring) {
        requestFailureStop();
        m_reclaimAbort = true;
        if (m_reclamationOwner) m_reclamationOwner->request();
        else reclaim(); // Preparation failed before an owner could be created.
    }
    if (m_reclamationOwner) m_reclamationOwner->join();
}

void MediaRuntimeBranch::reclaim() noexcept
{
    try {
        for (auto& worker : m_workers) worker->join();
        m_supervisor.disarm();
        if (m_reclaimAbort) m_scheduler.abort(m_context);
        else {
            auto stopped = m_scheduler.stop(m_context);
            if (!stopped) {
                m_failures.recordFirst(MediaGraphWorkerFailure{
                    {}, MediaNodeKind::Unknown, "output branch teardown", stopped.error(),
                    MediaGraphWorkerFailurePhase::Release});
                m_scheduler.abort(m_context);
            }
        }
        if (m_reclamationOwner) m_reclamationOwner->markProgress();
        m_workers.clear();
        m_scheduler.clear(&m_context);
        if (m_reclamationOwner) m_reclamationOwner->markProgress();
        m_inputs.clear();
        for (auto* channel : m_context.channels().channels()) channel->clear();
        m_context.reset();
        m_reclaimed = true;
    } catch (...) {
        // Preallocated before worker creation: preserve first-failure semantics
        // without allocating while reporting an exceptional release path.
        if (m_unexpectedReleaseFailure) {
            m_failures.recordFirst(std::move(*m_unexpectedReleaseFailure));
            m_unexpectedReleaseFailure.reset();
        }
    }
}

::media::Status MediaRuntimeBranch::start()
{
    std::lock_guard lock(m_publicationMutex);
    if (m_state != MediaRuntimeBranchState::Prepared) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime branch can only start once"));
    }
    m_failures.setPhase(MediaGraphWorkerFailurePhase::Preparation);
    auto started = m_scheduler.start(m_context);
    if (!started) {
        m_failures.recordFirst(MediaGraphWorkerFailure{
            {}, MediaNodeKind::Unknown, "output branch preparation", started.error(), MediaGraphWorkerFailurePhase::Preparation});
        m_state = MediaRuntimeBranchState::Failed;
        return started;
    }
    for (auto* node : m_scheduler.orderedRuntimeNodes(m_context)) {
        m_workers.push_back(std::make_unique<MediaGraphWorker>(
            *node, m_context, m_failures, m_supervisor));
    }
    m_supervisor.arm([this] { requestFailureStop(); });
    m_failures.setPhase(MediaGraphWorkerFailurePhase::Runtime);
    try {
        for (auto& worker : m_workers) {
            auto status = worker->start();
            if (!status) {
                m_failures.recordFirst(MediaGraphWorkerFailure{{}, MediaNodeKind::Unknown,
                    "output worker creation", status.error(), MediaGraphWorkerFailurePhase::Preparation});
                requestFailureStop(); m_state = MediaRuntimeBranchState::Failed; return status;
            }
        }
    } catch (const std::exception& error) {
        m_failures.recordFirst(MediaGraphWorkerFailure{{}, MediaNodeKind::Unknown,
            "output worker creation", ::media::ErrorInfo::internalError(error.what()), MediaGraphWorkerFailurePhase::Preparation});
        requestFailureStop();
        m_state = MediaRuntimeBranchState::Failed;
        return ::media::Status::failure(::media::ErrorInfo::internalError(error.what()));
    }
    m_state = MediaRuntimeBranchState::Running;
    return ::media::Status::success();
}

MediaChannelPushResult MediaRuntimeBranch::tryPublish(MediaEdgeId edge, MediaBufferRef buffer)
{
    std::lock_guard lock(m_publicationMutex);
    if (m_state != MediaRuntimeBranchState::Running || m_failures.hasFailure())
        return {MediaQueuePushOutcome::Closed, 0, 0};
    auto found = std::find_if(m_inputs.begin(), m_inputs.end(),
        [edge](const auto* channel) { return channel->edgeId() == edge; });
    if (found == m_inputs.end() || !buffer) return {MediaQueuePushOutcome::Aborted, 0, 0};
    return (*found)->pushOutcome(std::move(buffer));
}

::media::Status MediaRuntimeBranch::beginDrain(
    std::uint64_t generation, const MediaRuntimeBranchDrainPlan& plan)
{
    auto eos = makeMediaBufferRef<MediaControlBuffer>(MediaControlBufferKind::Eof, generation);
    std::lock_guard lock(m_publicationMutex);
    if (m_state != MediaRuntimeBranchState::Running || plan.maximumProgressSilence.nanoseconds() <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "runtime branch drain requires a running branch and positive planned progress timeout"));
    }
    m_drainPlan = plan;
    m_failures.setPhase(MediaGraphWorkerFailurePhase::Drain);
    m_state = MediaRuntimeBranchState::Draining;
    for (auto* channel : m_inputs) {
        // Configuration remains publishable by the shared resolver during
        // startup. Only its media input carries the ordered lifecycle signal.
        if (channel->binding().payloadKind != MediaPayloadKind::Frame &&
            channel->binding().payloadKind != MediaPayloadKind::Packet) continue;
        auto closed = channel->closeWithTerminal(eos);
        if (!closed) {
            m_failures.recordFirst(MediaGraphWorkerFailure{
                {}, MediaNodeKind::Unknown, "output branch drain", closed.error(), MediaGraphWorkerFailurePhase::Drain});
            m_state = MediaRuntimeBranchState::Failed;
            requestFailureStop();
            return closed;
        }
    }
    m_lastDrainProgress = drainProgress();
    m_lastDrainProgressAt = std::chrono::steady_clock::now();
    return ::media::Status::success();
}

void MediaRuntimeBranch::requestFailureStop() noexcept
{
    // Control callers hold m_publicationMutex through both passes, preventing
    // poll from handing these objects to the reclamation owner mid-request.
    // The supervisor runs on a worker: its exit publication follows this
    // callback, so poll's all-workers-exited gate supplies the same lifetime
    // barrier. Final destruction requests stop before starting/joining reclaim.
    for (auto& worker : m_workers) worker->requestStop();
    for (auto& worker : m_workers) worker->interrupt();
}

void MediaRuntimeBranch::fail(::media::ErrorInfo error)
{
    fail(MediaGraphWorkerFailure{{}, MediaNodeKind::Unknown, "output branch",
        std::move(error), m_failures.phase()});
}

void MediaRuntimeBranch::fail(MediaGraphWorkerFailure failure)
{
    std::lock_guard lock(m_publicationMutex);
    if (m_state == MediaRuntimeBranchState::Retired ||
        m_state == MediaRuntimeBranchState::Retiring) return;
    m_failures.recordFirst(std::move(failure));
    m_state = MediaRuntimeBranchState::Failed;
    requestFailureStop();
}

::media::Result<bool> MediaRuntimeBranch::poll()
{
    using Result = ::media::Result<bool>;
    std::unique_lock lock(m_publicationMutex);
    if (m_state == MediaRuntimeBranchState::Retired) return Result::success(true);
    if (m_state == MediaRuntimeBranchState::Retiring) {
        if (m_reclamationOwner->completed()) {
            // The release/acquire token follows every driver operation. Only
            // the thread return remains; join cannot wait for driver teardown.
            m_reclamationOwner->join();
            if (!m_reclaimed) return Result::success(false);
            m_finalMetrics.threadCount = 0;
            m_finalMetrics.activeWorkers = 0;
            m_finalMetrics.queuedBuffers = 0;
            m_state = MediaRuntimeBranchState::Retired;
            return Result::success(true);
        }
        const auto now = std::chrono::steady_clock::now();
        const auto progress = m_reclamationOwner->progress();
        if (progress != m_lastReleaseProgress) {
            m_lastReleaseProgress = progress;
            m_lastReleaseProgressAt = now;
        } else if (m_releaseSilenceFailure && now - m_lastReleaseProgressAt >=
            std::chrono::nanoseconds(m_reclamationPlan->maximumProgressSilence.nanoseconds())) {
            m_failures.recordFirst(std::move(*m_releaseSilenceFailure));
            m_releaseSilenceFailure.reset();
        }
        return Result::success(false);
    }
    if (m_failures.hasFailure()) m_state = MediaRuntimeBranchState::Failed;
    const bool workersExited = std::all_of(m_workers.begin(), m_workers.end(),
        [](const auto& worker) { return worker->exited(); });
    if (m_state == MediaRuntimeBranchState::Draining && !workersExited) {
        const auto now = std::chrono::steady_clock::now();
        const auto progress = drainProgress();
        if (progress != m_lastDrainProgress) {
            m_lastDrainProgress = progress;
            m_lastDrainProgressAt = now;
        } else if (now - m_lastDrainProgressAt >=
                   std::chrono::nanoseconds(m_drainPlan->maximumProgressSilence.nanoseconds())) {
            lock.unlock();
            fail(::media::ErrorInfo::internalError(
                "runtime branch drain made no progress within its planned silence bound"));
            return Result::success(false);
        }
    }
    if (m_state == MediaRuntimeBranchState::Prepared || !workersExited)
        return Result::success(false);
    if (!std::all_of(m_retirementPrerequisites.begin(), m_retirementPrerequisites.end(),
        [](const auto& token) { return token && token->completed(); })) return Result::success(false);
    m_reclaimAbort = m_state == MediaRuntimeBranchState::Failed;
    m_finalVideoReadyEvidence = collectVideoReadyEvidence();
    m_finalMetrics = collectMetrics();
    m_finalMetrics.threadCount = m_reclamationPlan->ownerThreads;
    m_finalMetrics.activeWorkers = m_reclamationPlan->ownerThreads;
    m_state = MediaRuntimeBranchState::Retiring;
    m_lastReleaseProgress = m_reclamationOwner->progress();
    m_lastReleaseProgressAt = std::chrono::steady_clock::now();
    m_reclamationOwner->request();
    return Result::success(false);
}

std::optional<MediaVideoOutputReadyEvidence>
MediaRuntimeBranch::collectVideoReadyEvidence() const
{
    for (const auto* node : m_scheduler.orderedRuntimeNodes(m_context)) {
        const auto* sender = dynamic_cast<const MediaScheduledDatagramSenderNode*>(node);
        if (sender) {
            if (auto evidence = sender->videoReadyEvidence()) return evidence;
        }
    }
    return std::nullopt;
}

std::optional<MediaVideoOutputReadyEvidence> MediaRuntimeBranch::videoReadyEvidence() const
{
    std::lock_guard lock(m_publicationMutex);
    return m_state == MediaRuntimeBranchState::Retired || m_state == MediaRuntimeBranchState::Retiring
        ? m_finalVideoReadyEvidence : collectVideoReadyEvidence();
}

std::uint64_t MediaRuntimeBranch::drainProgress() const
{
    std::uint64_t progress = 0;
    for (const auto* channel : m_context.channels().channels()) {
        progress += channel->metrics().pushed.load(std::memory_order_relaxed);
        progress += channel->metrics().popped.load(std::memory_order_relaxed);
    }
    for (const auto& worker : m_workers) if (worker->exited()) ++progress;
    return progress;
}

MediaGraphRuntimeMetrics MediaRuntimeBranch::collectMetrics() const
{
    auto result = MediaRuntimeMetricsCollector::workers(m_workers);
    if (m_reclamationPlan) result.threadCount += m_reclamationPlan->ownerThreads;
    for (const auto* channel : m_context.channels().channels()) {
        if (m_inputsAccountedBySession &&
            std::find(m_inputs.begin(), m_inputs.end(), channel) != m_inputs.end()) continue;
        MediaRuntimeMetricsCollector::includeChannel(result, *channel);
    }
    return result;
}

MediaGraphRuntimeMetrics MediaRuntimeBranch::metrics() const
{
    std::lock_guard lock(m_publicationMutex);
    return m_state == MediaRuntimeBranchState::Retired || m_state == MediaRuntimeBranchState::Retiring
        ? m_finalMetrics : collectMetrics();
}

MediaRuntimeBranchState MediaRuntimeBranch::state() const
{
    std::lock_guard lock(m_publicationMutex);
    return m_state;
}

std::optional<MediaGraphWorkerFailure> MediaRuntimeBranch::failure() const
{
    return m_failures.primaryFailure();
}

MediaRuntimeNode* MediaRuntimeBranch::findNode(MediaNodeId id) noexcept
{
    std::lock_guard lock(m_publicationMutex);
    if (m_state == MediaRuntimeBranchState::Retiring ||
        m_state == MediaRuntimeBranchState::Retired) return nullptr;
    return m_scheduler.findNode(id);
}

std::vector<MediaEdgeId> MediaRuntimeBranch::inputEdges() const
{
    std::lock_guard lock(m_publicationMutex);
    std::vector<MediaEdgeId> edges;
    if (m_state == MediaRuntimeBranchState::Retiring ||
        m_state == MediaRuntimeBranchState::Retired) return edges;
    for (const auto* channel : m_inputs) edges.push_back(channel->edgeId());
    return edges;
}

} // namespace media::ffmpeg::graph
