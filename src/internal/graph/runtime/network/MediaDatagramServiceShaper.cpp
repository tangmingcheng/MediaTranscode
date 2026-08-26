#include "internal/graph/runtime/network/MediaDatagramServiceShaper.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

bool sameRuntimeContract(const MediaDatagramShapingPlan& lhs,
                         const MediaDatagramShapingPlan& rhs) noexcept
{
    return lhs.sessionKey() == rhs.sessionKey() &&
           lhs.serviceScope() == rhs.serviceScope() &&
           lhs.endpoints() == rhs.endpoints() &&
           lhs.serviceCurve() == rhs.serviceCurve() &&
           lhs.backlog() == rhs.backlog() && lhs.batch() == rhs.batch() &&
           lhs.networkMemory() == rhs.networkMemory() &&
           lhs.submitMode() == rhs.submitMode() &&
           lhs.orderingMode() == rhs.orderingMode() &&
           lhs.pressureFailureMode() == rhs.pressureFailureMode() &&
           lhs.deadlineFailureMode() == rhs.deadlineFailureMode() &&
           lhs.persistentStateMode() == rhs.persistentStateMode();
}

struct EndpointUsage final {
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
};

} // namespace

MediaDatagramServiceShaper::MediaDatagramServiceShaper(
    MediaDatagramShapingPlan plan,
    MediaRunningTime burstDebtDuration) noexcept
    : m_plan(std::move(plan)), m_burstDebtDuration(burstDebtDuration)
{
}

::media::Result<std::unique_ptr<MediaDatagramServiceShaper>>
MediaDatagramServiceShaper::create(MediaDatagramShapingPlan plan)
{
    using Result =
        ::media::Result<std::unique_ptr<MediaDatagramServiceShaper>>;
    auto burstDurationNs = MediaCheckedArithmetic::ceilDurationNanoseconds(
        plan.serviceCurve().burstWireBytes,
        plan.serviceCurve().sustainedWireBytesPerSecond,
        "service shaper burst duration");
    if (!burstDurationNs) return Result::failure(burstDurationNs.error());
    auto result = std::unique_ptr<MediaDatagramServiceShaper>(
        new (std::nothrow) MediaDatagramServiceShaper(
            std::move(plan), MediaRunningTime::fromNanoseconds(
                                 burstDurationNs.value())));
    if (!result) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramServiceShaper"));
    }
    return Result::success(std::move(result));
}

::media::Status MediaDatagramServiceShaper::rebind(
    MediaDatagramShapingPlan plan)
{
    if (plan.generation() <= m_plan.generation() ||
        !sameRuntimeContract(m_plan, plan)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "service shaper rejects generation rollback or runtime contract change"));
    }
    m_plan = std::move(plan);
    return ::media::Status::success();
}

::media::Result<std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>
MediaDatagramServiceShaper::shape(
    MediaWireDatagramBatchBuffer& batch,
    MediaRunningTime now)
{
    using Result = ::media::Result<
        std::shared_ptr<MediaScheduledWireDatagramBatchBuffer>>;
    if (batch.sessionKey() != m_plan.sessionKey() ||
        batch.serviceScopeId() != m_plan.serviceScope().scopeId ||
        batch.generation() != m_plan.generation() ||
        now < MediaRunningTime::fromNanoseconds(0) ||
        (m_previousNow && now < *m_previousNow) ||
        batch.m_payload.empty() || batch.m_datagrams.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper requires matching service identity, monotonic time, and an unconsumed active-generation batch"));
    }

    std::deque<PendingReservation> pending;
    std::unordered_map<std::uint64_t, EndpointUsage> endpointUsage;
    std::vector<MediaScheduledWireDatagramDescriptor> descriptors;
    try {
        pending = m_pending;
        endpointUsage.reserve(m_plan.endpoints().size());
        descriptors.reserve(batch.m_datagrams.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper reservation scratch state"));
    }
    while (!pending.empty() && pending.front().completion <= now) {
        pending.pop_front();
    }

    std::uint64_t backlogDatagrams = 0;
    std::uint64_t backlogWireBytes = 0;
    std::uint64_t batchDatagrams = 0;
    std::uint64_t batchPayloadBytes = 0;
    std::uint64_t batchWireBytes = 0;
    std::int64_t maximumDebtDelayNanoseconds =
        m_telemetry.maximumDebtDelayNanoseconds;
    try {
        for (const auto& item : pending) {
            if (backlogDatagrams ==
                    (std::numeric_limits<std::uint64_t>::max)() ||
                item.wireBytes >
                    (std::numeric_limits<std::uint64_t>::max)() -
                        backlogWireBytes) {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "service shaper backlog accounting overflowed"));
            }
            ++backlogDatagrams;
            backlogWireBytes += item.wireBytes;
            auto& usage = endpointUsage[item.endpointId];
            ++usage.datagrams;
            usage.bytes += item.payloadBytes;
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper endpoint scratch state"));
    }

    auto peakAvailable = m_peakAvailable;
    auto sustainedDebtUntil = m_sustainedDebtUntil;
    auto previousRelease = m_previousCanonicalRelease;
    auto previousDeadline = m_previousCanonicalDeadline;
    auto previousSequence = m_previousGlobalSequence;
    for (const auto& datagram : batch.m_datagrams) {
        const auto& wire = datagram.m_descriptor;
        if ((previousSequence && wire.globalSequence <= *previousSequence) ||
            (previousRelease && wire.canonicalRelease < *previousRelease) ||
            (previousDeadline && wire.canonicalDeadline < *previousDeadline)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper requires globally canonical wire order"));
        }
        const auto* endpoint = m_plan.endpoint(wire.endpointId);
        auto cost = m_plan.plannedWireCost(wire.endpointId, wire.payloadSize);
        if (!endpoint || !cost || !datagram.hasCommitLease()) {
            return Result::failure(
                cost ? ::media::ErrorInfo::invalidArgument(
                           "service shaper rejects endpoint or lease mismatch")
                     : cost.error());
        }

        MediaRunningTime eligibility = (std::max)(now, wire.canonicalRelease);
        if (peakAvailable) eligibility = (std::max)(eligibility, *peakAvailable);
        auto burstSlack = m_burstDebtDuration.checkedSubtract(
            cost.value().sustainedDebtDuration);
        if (!burstSlack ||
            burstSlack.value() < MediaRunningTime::fromNanoseconds(0)) {
            if (m_telemetry.serviceCurveViolations ==
                (std::numeric_limits<std::uint64_t>::max)()) {
                m_telemetry.counterSaturated = true;
            } else {
                ++m_telemetry.serviceCurveViolations;
            }
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper datagram exceeds the burst envelope"));
        }
        if (sustainedDebtUntil) {
            auto debtEligibility = sustainedDebtUntil->checkedSubtract(
                burstSlack.value());
            if (!debtEligibility) return Result::failure(debtEligibility.error());
            eligibility = (std::max)(eligibility, debtEligibility.value());
        }
        const auto debtBase = sustainedDebtUntil
            ? (std::max)(*sustainedDebtUntil, eligibility)
            : eligibility;
        auto nextDebt = debtBase.checkedAdd(cost.value().sustainedDebtDuration);
        auto completion = eligibility.checkedAdd(
            cost.value().peakServiceDuration);
        auto endpointDeadline = wire.canonicalRelease.checkedAdd(
            endpoint->maximumResidence);
        auto backlogDeadline = wire.canonicalRelease.checkedAdd(
            m_plan.backlog().maximumResidence);
        if (!nextDebt || !completion || !endpointDeadline || !backlogDeadline) {
            return Result::failure(
                !nextDebt ? nextDebt.error()
                          : (!completion ? completion.error()
                                         : (!endpointDeadline
                                                ? endpointDeadline.error()
                                                : backlogDeadline.error())));
        }
        const auto enqueueNotAfter = (std::min)(
            wire.canonicalDeadline,
            (std::min)(endpointDeadline.value(), backlogDeadline.value()));
        if (eligibility > enqueueNotAfter) {
            if (m_telemetry.deadlineMisses ==
                (std::numeric_limits<std::uint64_t>::max)()) {
                m_telemetry.counterSaturated = true;
            } else {
                ++m_telemetry.deadlineMisses;
            }
            std::ostringstream message;
            message
                << "service shaper reservation misses its immutable deadline"
                << " global_sequence=" << wire.globalSequence
                << " endpoint_id=" << wire.endpointId
                << " payload_bytes=" << wire.payloadSize
                << " wire_bytes=" << cost.value().wireBytes
                << " canonical_release_ns="
                << wire.canonicalRelease.nanoseconds()
                << " canonical_deadline_ns="
                << wire.canonicalDeadline.nanoseconds()
                << " now_ns=" << now.nanoseconds()
                << " peak_available_ns="
                << (peakAvailable ? peakAvailable->nanoseconds() : -1)
                << " debt_eligibility_ns=";
            if (sustainedDebtUntil) {
                auto debtEligibility = sustainedDebtUntil->checkedSubtract(
                    burstSlack.value());
                message << (debtEligibility
                    ? debtEligibility.value().nanoseconds() : -1);
            } else {
                message << -1;
            }
            message
                << " selected_eligibility_ns=" << eligibility.nanoseconds()
                << " endpoint_deadline_ns="
                << endpointDeadline.value().nanoseconds()
                << " backlog_deadline_ns="
                << backlogDeadline.value().nanoseconds()
                << " enqueue_not_after_ns="
                << enqueueNotAfter.nanoseconds();
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(message.str()));
        }

        EndpointUsage* endpointPending = nullptr;
        try {
            endpointPending = &endpointUsage[wire.endpointId];
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "service shaper endpoint pressure state"));
        }
        if (backlogDatagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            cost.value().wireBytes >
                (std::numeric_limits<std::uint64_t>::max)() - backlogWireBytes ||
            endpointPending->datagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            wire.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() -
                    endpointPending->bytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper pressure accounting overflowed"));
        }
        ++backlogDatagrams;
        backlogWireBytes += cost.value().wireBytes;
        ++endpointPending->datagrams;
        endpointPending->bytes += wire.payloadSize;
        if (backlogDatagrams > m_plan.backlog().maximumDatagrams ||
            backlogWireBytes > m_plan.backlog().maximumBytes ||
            endpointPending->datagrams > endpoint->maximumPendingDatagrams ||
            endpointPending->bytes > endpoint->maximumPendingBytes) {
            if (m_telemetry.pressureFailures ==
                (std::numeric_limits<std::uint64_t>::max)()) {
                m_telemetry.counterSaturated = true;
            } else {
                ++m_telemetry.pressureFailures;
            }
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper terminates on backlog or endpoint pressure"));
        }
        auto debtDelay = eligibility.checkedSubtract(wire.canonicalRelease);
        if (!debtDelay || debtDelay.value().nanoseconds() < 0 ||
            batchDatagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            wire.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() -
                    batchPayloadBytes ||
            cost.value().wireBytes >
                (std::numeric_limits<std::uint64_t>::max)() - batchWireBytes) {
            return Result::failure(
                !debtDelay ? debtDelay.error() :
                ::media::ErrorInfo::internalError(
                    "service shaper admitted telemetry is not representable"));
        }
        ++batchDatagrams;
        batchPayloadBytes += wire.payloadSize;
        batchWireBytes += cost.value().wireBytes;
        maximumDebtDelayNanoseconds = (std::max)(
            maximumDebtDelayNanoseconds,
            debtDelay.value().nanoseconds());
        try {
            pending.push_back({wire.endpointId, wire.payloadSize,
                               cost.value().wireBytes, completion.value()});
            descriptors.push_back({wire, eligibility, enqueueNotAfter,
                                   cost.value().peakServiceDuration});
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "service shaper scheduled reservation"));
        }
        peakAvailable = completion.value();
        sustainedDebtUntil = nextDebt.value();
        previousRelease = wire.canonicalRelease;
        previousDeadline = wire.canonicalDeadline;
        previousSequence = wire.globalSequence;
    }

    auto output = MediaScheduledWireDatagramBatchBuffer::create(
        m_plan, batch, std::move(descriptors), now);
    if (!output) return Result::failure(output.error());

    const bool telemetryOverflow =
        m_telemetry.admittedBatches ==
            (std::numeric_limits<std::uint64_t>::max)() ||
        batchDatagrams >
            (std::numeric_limits<std::uint64_t>::max)() -
                m_telemetry.admittedDatagrams ||
        batchPayloadBytes >
            (std::numeric_limits<std::uint64_t>::max)() -
                m_telemetry.admittedPayloadBytes ||
        batchWireBytes >
            (std::numeric_limits<std::uint64_t>::max)() -
                m_telemetry.admittedWireBytes;
    if (telemetryOverflow) {
        m_telemetry.counterSaturated = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "service shaper admitted telemetry overflowed"));
    }
    ++m_telemetry.admittedBatches;
    m_telemetry.admittedDatagrams += batchDatagrams;
    m_telemetry.admittedPayloadBytes += batchPayloadBytes;
    m_telemetry.admittedWireBytes += batchWireBytes;
    m_telemetry.maximumDebtDelayNanoseconds = maximumDebtDelayNanoseconds;

    m_pending = std::move(pending);
    m_peakAvailable = peakAvailable;
    m_sustainedDebtUntil = sustainedDebtUntil;
    m_previousCanonicalRelease = previousRelease;
    m_previousCanonicalDeadline = previousDeadline;
    m_previousGlobalSequence = previousSequence;
    m_previousNow = now;
    return output;
}

} // namespace media::ffmpeg::graph
