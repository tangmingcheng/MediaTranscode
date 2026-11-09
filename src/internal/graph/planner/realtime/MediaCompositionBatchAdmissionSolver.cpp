#include "internal/graph/planner/realtime/MediaCompositionBatchAdmissionSolver.h"
#include "internal/graph/planner/realtime/MediaCompositionAdmissionMaterializer.h"
#include "internal/graph/planner/realtime/MediaAudioCorrectionReachabilityPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncComponentBoundsPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpSourceStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpBatchStoragePlanner.h"
#include "internal/graph/planner/realtime/MediaAvStartupIndexStoragePlanner.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::ErrorInfo invalid(const char* message)
{ return ::media::ErrorInfo::invalidArgument(message); }
}
MediaCompositionBatchAdmissionSolver::MediaCompositionBatchAdmissionSolver(
    std::span<const MediaCompositionSourceFixedRequest> prepared,
    MediaFixedCapacityStorage<MediaCompositionSourceFixedProducts>& fixed,
    const MediaRealtimeAvOutputRuntimePlan& output, const MediaVideoCanvasGeometry& canvas,
    MediaRational cadence, std::size_t audioSource, const MediaPreparedRealtimeEmissionSet& emission,
    const MediaRealtimeDeploymentLatencyBudget& latency,
    std::shared_ptr<MediaPreparationMemoryResource> root, MediaPreparationControl control)
    : m_prepared(prepared), m_fixed(fixed), m_output(output), m_canvas(canvas), m_cadence(cadence),
      m_audioSource(audioSource), m_emission(emission), m_latency(latency), m_root(std::move(root)),
      m_control(std::move(control)) {}

MediaCompositionCandidateWorkspace* MediaCompositionBatchAdmissionSolver::workspace() noexcept
{ return m_solved && m_workspace ? &*m_workspace : nullptr; }
std::span<const MediaCompositionSourceCapacityCandidate> MediaCompositionBatchAdmissionSolver::capacities() const
{ return m_solved && m_capacities ? m_capacities->view() : std::span<const MediaCompositionSourceCapacityCandidate>{}; }
std::optional<std::size_t> MediaCompositionBatchAdmissionSolver::selectedIncrement() const noexcept
{ return m_solved ? m_selected : std::nullopt; }

::media::Status MediaCompositionBatchAdmissionSolver::check(
    std::span<const MediaPreparedRtpStorageSourceSnapshot> sources) const
{
    if (auto status = m_control.check("Composition storage negotiation"); !status) return status;
    const auto now = std::chrono::steady_clock::now();
    for (const auto& source : sources)
        if (!source.plan || now >= source.plan->deadline)
            return ::media::Status::failure(invalid("Composition storage negotiation exceeded an original source deadline"));
    return ::media::Status::success();
}

::media::Result<MediaCompositionBatchAdmissionSolver::Membership> MediaCompositionBatchAdmissionSolver::select(
    std::span<const MediaPreparedRtpStorageSourceSnapshot> sources, std::size_t increment) const
{
    using Result = ::media::Result<Membership>;
    if (auto status = check(sources); !status) return Result::failure(status.error());
    m_capacities->clear();
    Membership membership = Membership::Legal;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (auto status = check(sources); !status) return Result::failure(status.error());
        auto planned = MediaCompositionSourceCapacityPlanner::plan(m_prepared[i].video, m_prepared[i].deployment,
            *sources[i].plan, sources[i].snapshot, increment);
        if (!planned) return Result::failure(planned.error());
        m_capacities->push_back(std::move(planned).value());
        const MediaRealtimeAvSourceFixedView fixed = m_workspace
            ? MediaRealtimeAvSourceFixedView{m_workspace->sources[i].runtime}
            : MediaRealtimeAvSourceFixedView{m_fixed[i].runtime};
        if (!fixed.audioPipeline.enabled) continue;
        auto bounds = MediaRealtimeAvSyncComponentBoundsPlanner::planSource(
            (*m_capacities)[i].capacity.queues, fixed.audioPipeline);
        if (!bounds) return Result::failure(bounds.error());
        auto assessed = MediaAudioCorrectionReachabilityPlanner::assessCapacity(fixed.synchronization.audioServo,
            MediaAudioSourceCorrectionFacts{fixed.audioPipeline.resolvedOutput->sampleRate(), bounds.value()});
        if (!assessed) return Result::failure(assessed.error());
        if (const auto* rejected = std::get_if<MediaAudioCorrectionCapacityRejection>(&assessed.value())) {
            if (rejected->position == MediaAudioCorrectionCapacityPosition::UpperBound)
                membership = Membership::UpperBound;
            else if (membership == Membership::Legal) membership = Membership::Gap;
        }
        // A gap/upper bound never hides another source's malformed fixed facts.
    }
    return Result::success(membership);
}

::media::Result<std::size_t> MediaCompositionBatchAdmissionSolver::upperBound(
    std::span<const MediaPreparedRtpStorageSourceSnapshot> sources, std::size_t available) const
{
    using Result = ::media::Result<std::size_t>;
    auto display = MediaAvStartupIndexStoragePlanner::maximumPreparedDisplayCapacity(available);
    if (!display) return Result::failure(display.error());
    if (display.value() < 2)
        return Result::failure(invalid("Composition storage cannot hold two decoded display points"));
    std::size_t upper = (std::min)(display.value() - 2,
        static_cast<std::size_t>((std::numeric_limits<int>::max)()) - 2);
    const auto limitStream = [&](const MediaPreparedRtpSourceSessionStreamPlan& stream,
        const MediaPreparedRtpStreamStorageSnapshot& held) -> ::media::Status {
        auto floor = MediaPreparedRtpSourceStoragePlanner::minimumBounds(stream, held);
        if (!floor) return ::media::Status::failure(floor.error());
        const auto intMaximum = static_cast<std::size_t>((std::numeric_limits<int>::max)());
        if (floor.value().acquiringObjects > intMaximum)
            return ::media::Status::failure(invalid("Composition acquiring floor exceeds binder representation"));
        upper = (std::min)(upper, intMaximum - floor.value().acquiringObjects);
        upper = (std::min)(upper, (std::numeric_limits<std::size_t>::max)() - floor.value().outputObjects);
        return ::media::Status::success();
    };
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (auto status = check(sources); !status) return Result::failure(status.error());
        const auto& source = sources[i];
        if (auto status = limitStream(source.plan->video, source.snapshot.video); !status)
            return Result::failure(status.error());
        if (source.plan->audio) {
            if (!source.snapshot.audio) return Result::failure(invalid("Composition source audio snapshot is absent"));
            if (auto status = limitStream(*source.plan->audio, *source.snapshot.audio); !status)
                return Result::failure(status.error());
        }
        auto initial = MediaCompositionSourceCapacityPlanner::plan(m_prepared[i].video, m_prepared[i].deployment,
            *source.plan, source.snapshot, 0);
        if (!initial) return Result::failure(initial.error());
        const auto unit = initial.value().capacity.startup.videoUnitBytes;
        if (!unit) return Result::failure(invalid("Composition source has no decoded frame footprint"));
        const auto count = static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) / unit;
        if (count < 2) return Result::failure(invalid("Composition display payload exceeds signed byte representation"));
        upper = static_cast<std::size_t>((std::min)(static_cast<std::uint64_t>(upper), count - 2));
    }
    return Result::success(upper);
}

::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>>
MediaCompositionBatchAdmissionSolver::solve(std::span<const MediaPreparedRtpStorageSourceSnapshot> sources,
    const MediaPreparedRtpBatchBudgetView& budgets, const MediaPreparedRtpBatchStoragePlan& batch) const
try {
    using Result = ::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>>;
    if (m_attempted) return Result::failure(invalid("Composition admission solver is single-use"));
    m_attempted = true;
    if (!m_root || sources.empty() || sources.size() != m_prepared.size() || sources.size() != m_fixed.size() ||
        budgets.children.size() != sources.size() || budgets.snapshots.size() != sources.size() ||
        m_fixed.resource() != m_root.get() || batch.sourceCount != sources.size() || !batch.owner ||
        batch.owner == m_root || m_audioSource >= sources.size())
        return Result::failure(invalid("Composition admission requires matching prepared sources and storage owners"));
    if (auto status = check(sources); !status) return Result::failure(status.error());
    auto expectedBatch = MediaPreparedRtpBatchStoragePlanner::plan(sources.size(), batch.owner);
    if (!expectedBatch) return Result::failure(expectedBatch.error());
    if (expectedBatch.value().scratchBytes != batch.scratchBytes || expectedBatch.value().resultBytes != batch.resultBytes ||
        expectedBatch.value().totalBytes != batch.totalBytes)
        return Result::failure(invalid("Composition batch storage differs from actual array geometry"));
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (!sources[i].snapshot.owner || !budgets.children[i] ||
            !sources[i].snapshot.owner->usesBudget(*budgets.children[i]) ||
            m_fixed[i].planningResource != m_root ||
            &m_fixed[i].video != &m_prepared[i].video ||
            m_fixed[i].sourceRequest.mediaId != m_prepared[i].sourceRequest.mediaId)
            return Result::failure(invalid("Composition admission changed original source association"));
    }
    if (!batch.owner->usesBudget(*budgets.children.front()))
        return Result::failure(invalid("Composition batch storage is not charged to source zero's budget"));
    auto partition = budgets.refresh();
    if (!partition) return Result::failure(partition.error());
    if (!partition.value().root || !m_root->usesBudget(*partition.value().root))
        return Result::failure(invalid("Composition workspace does not use the source partition root budget"));
    const auto capacity = partition.value().budget.capacity;
    if (partition.value().otherRetainedBytes > capacity || batch.totalBytes > capacity - partition.value().otherRetainedBytes)
        return Result::failure(invalid("Composition batch storage exceeds deployment capacity"));
    for (const auto& source : sources)
        if (source.plan->storageBytes != capacity)
            return Result::failure(invalid("Composition sources require the same deployment capacity"));
    auto limit = upperBound(sources, capacity - partition.value().otherRetainedBytes - batch.totalBytes);
    if (!limit) return Result::failure(limit.error());
    m_capacities.emplace(sources.size(), *m_root);
    std::size_t first = 0;
    for (;;) {
        auto membership = select(sources, first);
        if (!membership) return Result::failure(membership.error());
        if (membership.value() == Membership::Legal) break;
        if (membership.value() == Membership::UpperBound || first == limit.value())
            return Result::failure(invalid("Composition has no legal capacity in its representation and storage domain"));
        ++first;
    }
    MediaFixedCapacityStorage<MediaCompositionSourcePlanningProducts> products(sources.size(), *m_root);
    for (std::size_t i = 0; i < sources.size(); ++i) {
        if (auto status = check(sources); !status) return Result::failure(status.error());
        auto bound = MediaCompositionSourcePlanningKernel::bind(std::move(m_fixed[i]),
            std::cref((*m_capacities)[i].capacity));
        if (!bound) return Result::failure(bound.error());
        products.push_back(std::move(bound).value());
    }
    auto workspace = MediaCompositionCandidateWorkspace::create(std::move(products), m_prepared, m_capacities->view(),
        sources, m_output, m_canvas, m_cadence, m_audioSource, m_emission, m_latency, m_root);
    if (!workspace) return Result::failure(workspace.error());
    m_workspace.emplace(std::move(workspace).value());
    MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission> admissions(sources.size(), *batch.owner);
    for (std::size_t i = 0; i < sources.size(); ++i) {
        auto admission = MediaCompositionAdmissionMaterializer::create(sources[i], (*m_capacities)[i],
            m_workspace->sources[i], m_workspace->costs[i], m_root);
        if (!admission) return Result::failure(admission.error());
        admissions.push_back(std::move(admission).value());
    }
    // All retained search/result backing now exists. Candidate refresh neither
    // changes this root baseline nor re-counts source-zero batch arrays.
    partition = budgets.refresh();
    if (!partition) return Result::failure(partition.error());
    if (!partition.value().root || !m_root->usesBudget(*partition.value().root) ||
        partition.value().budget.capacity != capacity || partition.value().otherRetainedBytes > capacity ||
        batch.totalBytes > capacity - partition.value().otherRetainedBytes)
        return Result::failure(invalid("Composition workspace exceeds deployment capacity"));
    const auto available = capacity - partition.value().otherRetainedBytes - batch.totalBytes;
    const auto fits = [&]() -> ::media::Result<bool> {
        if (!m_workspace->valid()) return ::media::Result<bool>::failure(invalid("Composition search lost its candidate workspace"));
        for (std::size_t i = 0; i < sources.size(); ++i) {
            const auto batchCredit = i == 0 ? batch.totalBytes : 0;
            if (budgets.snapshots[i].retainedBytes < batchCredit ||
                m_workspace->costs[i].totalBytes < budgets.snapshots[i].retainedBytes - batchCredit)
                return ::media::Result<bool>::failure(invalid("Composition candidate lost live source reservations"));
        }
        return ::media::Result<bool>::success(m_workspace->sourceBytes() <= available);
    };
    auto feasible = fits();
    if (!feasible) return Result::failure(feasible.error());
    if (!feasible.value()) return Result::failure(invalid("Composition minimum legal capacity exceeds deployment storage"));
    std::size_t best = first;
    std::size_t hi = limit.value();
    if (first < hi) {
        std::size_t lo = first + 1;
        while (lo <= hi) {
            const auto mid = lo + (hi - lo) / 2;
            std::size_t next = mid;
            bool found = false;
            for (;;) {
                auto membership = select(sources, next);
                if (!membership) return Result::failure(membership.error());
                if (membership.value() == Membership::Legal) { found = true; break; }
                if (membership.value() == Membership::UpperBound || next == hi) break;
                ++next;
            }
            if (!found) { hi = mid - 1; continue; }
            if (auto status = m_workspace->rebind(m_prepared, m_capacities->view()); !status)
                return Result::failure(status.error());
            feasible = fits();
            if (!feasible) return Result::failure(feasible.error());
            if (feasible.value()) {
                best = next;
                if (next == hi) break;
                lo = next + 1;
            } else hi = mid - 1;
        }
    }
    auto selected = select(sources, best);
    if (!selected) return Result::failure(selected.error());
    if (selected.value() != Membership::Legal)
        return Result::failure(invalid("Composition selected capacity changed domain membership"));
    if (auto status = m_workspace->rebind(m_prepared, m_capacities->view()); !status)
        return Result::failure(status.error());
    feasible = fits();
    if (!feasible) return Result::failure(feasible.error());
    if (!feasible.value()) return Result::failure(invalid("Composition selected capacity changed storage cost"));
    for (std::size_t i = 0; i < sources.size(); ++i)
        if (auto status = MediaCompositionAdmissionMaterializer::rebind(sources[i], (*m_capacities)[i],
                m_workspace->sources[i], m_workspace->costs[i], admissions[i]); !status)
            return Result::failure(status.error());
    if (auto status = check(sources); !status) return Result::failure(status.error());
    m_selected = best;
    m_solved = true;
    return Result::success(std::move(admissions));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaFixedCapacityStorage<MediaPreparedRtpStorageAdmission>>::failure(
        ::media::ErrorInfo::allocationFailed("Composition storage negotiation workspace"));
}
}
