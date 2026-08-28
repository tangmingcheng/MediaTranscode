#include "internal/graph/runtime/network/MediaDatagramServiceShaper.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
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
    const auto maximumPendingDatagrams = plan.backlog().maximumDatagrams;
    const auto maximumBatchDatagrams = plan.batch().maximumDatagrams;
    std::vector<std::uint64_t> endpointIds;
    try {
        endpointIds.reserve(plan.endpoints().size());
        for (const auto& endpoint : plan.endpoints()) {
            endpointIds.push_back(endpoint.endpointId);
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper endpoint ledger identity"));
    }
    auto result = std::unique_ptr<MediaDatagramServiceShaper>(
        new (std::nothrow) MediaDatagramServiceShaper(
            std::move(plan), MediaRunningTime::fromNanoseconds(
                                 burstDurationNs.value())));
    if (!result) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramServiceShaper"));
    }
    if (maximumPendingDatagrams > result->m_pending.max_size() ||
        maximumBatchDatagrams > result->m_newPending.max_size() ||
        maximumBatchDatagrams > result->m_preparedReservations.max_size() ||
        maximumBatchDatagrams > result->m_pacingFacts.max_size()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper planner backlog is not representable"));
    }
    try {
        result->m_pending.resize(
            static_cast<std::size_t>(maximumPendingDatagrams));
        result->m_newPending.reserve(
            static_cast<std::size_t>(maximumBatchDatagrams));
        result->m_preparedReservations.reserve(
            static_cast<std::size_t>(maximumBatchDatagrams));
        result->m_pacingFacts.reserve(
            static_cast<std::size_t>(maximumBatchDatagrams));
        result->m_pendingByEndpoint.reserve(endpointIds.size());
        result->m_batchByEndpoint.reserve(endpointIds.size());
        result->m_expiredByEndpoint.reserve(endpointIds.size());
        for (const auto endpointId : endpointIds) {
            result->m_pendingByEndpoint.emplace(endpointId, EndpointUsage{});
            result->m_batchByEndpoint.emplace(endpointId, EndpointUsage{});
            result->m_expiredByEndpoint.emplace(endpointId, EndpointUsage{});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper incremental backlog ledger"));
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
        batch.m_payload.empty() || batch.m_datagrams.empty() ||
        !batch.m_commitSlice.valid() ||
        batch.m_datagrams.size() > m_plan.batch().maximumDatagrams) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper requires matching service identity, monotonic time, and an unconsumed active-generation batch"));
    }

    std::vector<MediaScheduledWireDatagramDescriptor> descriptors;
    try {
        descriptors.reserve(batch.m_datagrams.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper reservation scratch state"));
    }
    m_newPending.clear();
    m_preparedReservations.clear();
    m_pacingFacts.clear();
    for (auto& [endpointId, usage] : m_batchByEndpoint) {
        static_cast<void>(endpointId);
        usage = EndpointUsage{};
    }
    for (auto& [endpointId, usage] : m_expiredByEndpoint) {
        static_cast<void>(endpointId);
        usage = EndpointUsage{};
    }

    std::uint64_t backlogDatagrams = m_pendingDatagrams;
    std::uint64_t backlogWireBytes = m_pendingWireBytes;
    std::size_t expiredCount = 0;
    while (expiredCount < m_pendingCount) {
        const auto index =
            (m_pendingHead + expiredCount) % m_pending.size();
        const auto& slot = m_pending[index];
        if (!slot) {
            return Result::failure(::media::ErrorInfo::internalError(
                "service shaper ring ledger contains an empty active slot"));
        }
        const auto& expired = *slot;
        if (expired.completion > now) break;
        auto endpoint = m_pendingByEndpoint.find(expired.endpointId);
        auto endpointExpired = m_expiredByEndpoint.find(expired.endpointId);
        if (endpoint == m_pendingByEndpoint.end() ||
            endpointExpired == m_expiredByEndpoint.end() ||
            backlogDatagrams == 0 ||
            backlogWireBytes < expired.wireBytes ||
            endpoint->second.datagrams <= endpointExpired->second.datagrams ||
            endpoint->second.bytes < endpointExpired->second.bytes ||
            endpoint->second.bytes - endpointExpired->second.bytes <
                expired.payloadBytes) {
            return Result::failure(::media::ErrorInfo::internalError(
                "service shaper incremental backlog ledger underflowed"));
        }
        --backlogDatagrams;
        backlogWireBytes -= expired.wireBytes;
        ++endpointExpired->second.datagrams;
        endpointExpired->second.bytes += expired.payloadBytes;
        ++expiredCount;
    }
    std::uint64_t batchDatagrams = 0;
    std::uint64_t batchPayloadBytes = 0;
    std::uint64_t batchWireBytes = 0;
    std::int64_t maximumDebtDelayNanoseconds =
        m_telemetry.maximumDebtDelayNanoseconds;
    auto physicalAvailable = m_physicalAvailable;
    auto sustainedDebtUntil = m_sustainedDebtUntil;
    auto previousRelease = m_previousCanonicalRelease;
    auto previousDeadline = m_previousCanonicalDeadline;
    auto previousSequence = m_previousGlobalSequence;
    const auto& batchFirst = batch.m_datagrams.front().m_descriptor;
    const auto& batchLast = batch.m_datagrams.back().m_descriptor;
    auto targetServiceWindow =
        m_plan.serviceCurve().targetResidence.checkedSubtract(
            m_plan.serviceCurve().maximumReleaseJitter);
    if (!targetServiceWindow) {
        return Result::failure(targetServiceWindow.error());
    }
    for (const auto& datagram : batch.m_datagrams) {
        const auto& wire = datagram.m_descriptor;
        const auto* endpoint = m_plan.endpoint(wire.endpointId);
        auto cost = m_plan.plannedWireCost(wire.endpointId, wire.payloadSize);
        if (!endpoint || !cost) {
            return Result::failure(
                cost ? ::media::ErrorInfo::invalidArgument(
                           "service shaper rejects endpoint or lease mismatch")
                     : cost.error());
        }
        auto targetCompletion = wire.canonicalRelease.checkedAdd(
            targetServiceWindow.value());
        auto endpointDeadline = wire.canonicalRelease.checkedAdd(
            endpoint->maximumResidence);
        auto backlogDeadline = wire.canonicalRelease.checkedAdd(
            m_plan.backlog().maximumResidence);
        if (!targetCompletion || !endpointDeadline || !backlogDeadline) {
            return Result::failure(
                !targetCompletion ? targetCompletion.error()
                                  : (!endpointDeadline
                                         ? endpointDeadline.error()
                                         : backlogDeadline.error()));
        }
        const auto hardDeadline = (std::min)(
            wire.canonicalDeadline,
            (std::min)(endpointDeadline.value(), backlogDeadline.value()));
        auto reservedHardDeadline = hardDeadline.checkedSubtract(
            m_plan.serviceCurve().maximumReleaseJitter);
        if (!reservedHardDeadline) {
            return Result::failure(reservedHardDeadline.error());
        }
        const auto selectedTarget = (std::min)(
            targetCompletion.value(), reservedHardDeadline.value());
        if (selectedTarget < wire.canonicalRelease ||
            reservedHardDeadline.value() < selectedTarget) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper target completion precedes canonical release"));
        }
        try {
            m_preparedReservations.push_back(PreparedReservation{
                cost.value(), endpointDeadline.value(),
                backlogDeadline.value(), hardDeadline,
                endpoint->maximumPendingDatagrams,
                endpoint->maximumPendingBytes});
            m_pacingFacts.push_back(MediaDatagramPacingReservationFact{
                cost.value().wireBytes, wire.canonicalRelease, selectedTarget,
                reservedHardDeadline.value(),
                cost.value().sustainedDebtDuration});
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "service shaper prepared reservation"));
        }
    }
    auto selectedPacingRate =
        MediaDatagramBatchPacingRateSelector::selectMinimumFeasibleRate(
            m_pacingFacts, now, physicalAvailable, sustainedDebtUntil,
            m_burstDebtDuration,
            m_plan.serviceCurve().sustainedWireBytesPerSecond,
            m_plan.serviceCurve().peakWireBytesPerSecond);
    if (!selectedPacingRate) {
        return Result::failure(selectedPacingRate.error());
    }
    for (std::size_t datagramIndex = 0;
         datagramIndex < batch.m_datagrams.size(); ++datagramIndex) {
        const auto& datagram = batch.m_datagrams[datagramIndex];
        const auto& wire = datagram.m_descriptor;
        const auto& prepared = m_preparedReservations[datagramIndex];
        const auto& cost = prepared.cost;
        const auto arrivalAfterRelease = now.checkedSubtract(
            wire.canonicalRelease);
        if (arrivalAfterRelease &&
            arrivalAfterRelease.value().nanoseconds() >
                m_telemetry.maximumArrivalAfterReleaseNanoseconds) {
            m_telemetry.maximumArrivalAfterReleaseNanoseconds =
                arrivalAfterRelease.value().nanoseconds();
            m_telemetry.worstArrivalGlobalSequence = wire.globalSequence;
            m_telemetry.worstArrivalReleaseNanoseconds =
                wire.canonicalRelease.nanoseconds();
            m_telemetry.worstArrivalDeadlineNanoseconds =
                wire.canonicalDeadline.nanoseconds();
            m_telemetry.worstArrivalNowNanoseconds = now.nanoseconds();
        }
        if ((previousSequence && wire.globalSequence <= *previousSequence) ||
            (previousRelease && wire.canonicalRelease < *previousRelease) ||
            (previousDeadline && wire.canonicalDeadline < *previousDeadline)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper requires globally canonical wire order"));
        }
        MediaRunningTime eligibility = (std::max)(now, wire.canonicalRelease);
        if (physicalAvailable) {
            eligibility = (std::max)(eligibility, *physicalAvailable);
        }
        auto burstSlack = m_burstDebtDuration.checkedSubtract(
            cost.sustainedDebtDuration);
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
        auto nextDebt = debtBase.checkedAdd(cost.sustainedDebtDuration);
        auto serviceDurationNanoseconds =
            MediaCheckedArithmetic::ceilDurationNanoseconds(
                cost.wireBytes,
                selectedPacingRate.value().wireBytesPerSecond,
                "selected datagram pacing duration");
        if (!serviceDurationNanoseconds) {
            return Result::failure(serviceDurationNanoseconds.error());
        }
        const auto serviceDuration = MediaRunningTime::fromNanoseconds(
            serviceDurationNanoseconds.value());
        auto completion = eligibility.checkedAdd(serviceDuration);
        if (!nextDebt || !completion) {
            return Result::failure(
                !nextDebt ? nextDebt.error()
                          : completion.error());
        }
        const auto enqueueNotAfter = prepared.enqueueNotAfter;
        if (eligibility > enqueueNotAfter ||
            completion.value() > enqueueNotAfter) {
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
                << " wire_bytes=" << cost.wireBytes
                << " canonical_release_ns="
                << wire.canonicalRelease.nanoseconds()
                << " canonical_deadline_ns="
                << wire.canonicalDeadline.nanoseconds()
                << " now_ns=" << now.nanoseconds()
                << " physical_available_ns="
                << (physicalAvailable ? physicalAvailable->nanoseconds() : -1)
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
                << " selected_completion_ns="
                << completion.value().nanoseconds()
                << " selected_pacing_wire_bytes_per_second="
                << selectedPacingRate.value().wireBytesPerSecond
                << " endpoint_deadline_ns="
                << prepared.endpointDeadline.nanoseconds()
                << " backlog_deadline_ns="
                << prepared.backlogDeadline.nanoseconds()
                << " enqueue_not_after_ns="
                << enqueueNotAfter.nanoseconds()
                << " batch_datagrams=" << batch.m_datagrams.size()
                << " batch_first_sequence=" << batchFirst.globalSequence
                << " batch_last_sequence=" << batchLast.globalSequence
                << " batch_first_release_ns="
                << batchFirst.canonicalRelease.nanoseconds()
                << " batch_last_release_ns="
                << batchLast.canonicalRelease.nanoseconds()
                << " batch_first_deadline_ns="
                << batchFirst.canonicalDeadline.nanoseconds()
                << " batch_last_deadline_ns="
                << batchLast.canonicalDeadline.nanoseconds();
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(message.str()));
        }

        auto endpointPending = m_pendingByEndpoint.find(wire.endpointId);
        auto endpointBatch = m_batchByEndpoint.find(wire.endpointId);
        auto endpointExpired = m_expiredByEndpoint.find(wire.endpointId);
        if (endpointPending == m_pendingByEndpoint.end() ||
            endpointBatch == m_batchByEndpoint.end() ||
            endpointExpired == m_expiredByEndpoint.end() ||
            endpointPending->second.datagrams <
                endpointExpired->second.datagrams ||
            endpointPending->second.bytes < endpointExpired->second.bytes) {
            return Result::failure(::media::ErrorInfo::internalError(
                "service shaper endpoint ledger differs from its activated plan"));
        }
        const auto activeEndpointDatagrams =
            endpointPending->second.datagrams -
            endpointExpired->second.datagrams;
        const auto activeEndpointBytes =
            endpointPending->second.bytes - endpointExpired->second.bytes;
        if (endpointBatch->second.datagrams >
                (std::numeric_limits<std::uint64_t>::max)() -
                    activeEndpointDatagrams ||
            endpointBatch->second.bytes >
                (std::numeric_limits<std::uint64_t>::max)() -
                    activeEndpointBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper endpoint pressure accounting overflowed"));
        }
        const auto endpointPendingDatagrams =
            activeEndpointDatagrams + endpointBatch->second.datagrams;
        const auto endpointPendingBytes =
            activeEndpointBytes + endpointBatch->second.bytes;
        if (backlogDatagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            cost.wireBytes >
                (std::numeric_limits<std::uint64_t>::max)() - backlogWireBytes ||
            endpointPendingDatagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            wire.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() -
                    endpointPendingBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper pressure accounting overflowed"));
        }
        ++backlogDatagrams;
        backlogWireBytes += cost.wireBytes;
        ++endpointBatch->second.datagrams;
        endpointBatch->second.bytes += wire.payloadSize;
        const auto prospectiveEndpointDatagrams =
            endpointPendingDatagrams + 1;
        const auto prospectiveEndpointBytes =
            endpointPendingBytes + wire.payloadSize;
        if (backlogDatagrams > m_plan.backlog().maximumDatagrams ||
            backlogWireBytes > m_plan.backlog().maximumBytes ||
            prospectiveEndpointDatagrams >
                prepared.maximumPendingDatagrams ||
            prospectiveEndpointBytes > prepared.maximumPendingBytes) {
            if (m_telemetry.pressureFailures ==
                (std::numeric_limits<std::uint64_t>::max)()) {
                m_telemetry.counterSaturated = true;
            } else {
                ++m_telemetry.pressureFailures;
            }
            std::ostringstream message;
            message
                << "service shaper terminates on backlog or endpoint pressure"
                << " global_sequence=" << wire.globalSequence
                << " endpoint_id=" << wire.endpointId
                << " backlog_datagrams=" << backlogDatagrams
                << " maximum_backlog_datagrams="
                << m_plan.backlog().maximumDatagrams
                << " backlog_wire_bytes=" << backlogWireBytes
                << " maximum_backlog_bytes="
                << m_plan.backlog().maximumBytes
                << " endpoint_pending_datagrams="
                << prospectiveEndpointDatagrams
                << " maximum_endpoint_pending_datagrams="
                << prepared.maximumPendingDatagrams
                << " endpoint_pending_bytes=" << prospectiveEndpointBytes
                << " maximum_endpoint_pending_bytes="
                << prepared.maximumPendingBytes;
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(message.str()));
        }
        auto debtDelay = eligibility.checkedSubtract(wire.canonicalRelease);
        if (!debtDelay || debtDelay.value().nanoseconds() < 0 ||
            batchDatagrams ==
                (std::numeric_limits<std::uint64_t>::max)() ||
            wire.payloadSize >
                (std::numeric_limits<std::uint64_t>::max)() -
                    batchPayloadBytes ||
            cost.wireBytes >
                (std::numeric_limits<std::uint64_t>::max)() - batchWireBytes) {
            return Result::failure(
                !debtDelay ? debtDelay.error() :
                ::media::ErrorInfo::internalError(
                    "service shaper admitted telemetry is not representable"));
        }
        ++batchDatagrams;
        batchPayloadBytes += wire.payloadSize;
        batchWireBytes += cost.wireBytes;
        maximumDebtDelayNanoseconds = (std::max)(
            maximumDebtDelayNanoseconds,
            debtDelay.value().nanoseconds());
        try {
            m_newPending.push_back({wire.endpointId, wire.payloadSize,
                                    cost.wireBytes,
                                    completion.value()});
            descriptors.push_back({wire, eligibility, enqueueNotAfter,
                                   cost.peakServiceDuration});
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "service shaper scheduled reservation"));
        }
        physicalAvailable = completion.value();
        sustainedDebtUntil = nextDebt.value();
        previousRelease = wire.canonicalRelease;
        previousDeadline = wire.canonicalDeadline;
        previousSequence = wire.globalSequence;
    }

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
                m_telemetry.admittedWireBytes ||
        (!selectedPacingRate.value().targetResidenceSatisfied &&
         m_telemetry.targetResidenceMissedBatches ==
             (std::numeric_limits<std::uint64_t>::max)());
    if (telemetryOverflow) {
        m_telemetry.counterSaturated = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "service shaper admitted telemetry overflowed"));
    }
    if (m_pendingCount != m_pendingDatagrams ||
        expiredCount > m_pendingCount ||
        m_pendingCount - expiredCount > m_pending.size() ||
        m_newPending.size() >
            m_pending.size() - (m_pendingCount - expiredCount)) {
        return Result::failure(::media::ErrorInfo::internalError(
            "service shaper incremental backlog exceeds reserved planner capacity"));
    }

    auto output = MediaScheduledWireDatagramBatchBuffer::create(
        m_plan, batch, std::move(descriptors), now);
    if (!output) return Result::failure(output.error());

    ++m_telemetry.admittedBatches;
    m_telemetry.admittedDatagrams += batchDatagrams;
    m_telemetry.admittedPayloadBytes += batchPayloadBytes;
    m_telemetry.admittedWireBytes += batchWireBytes;
    m_telemetry.maximumDebtDelayNanoseconds = maximumDebtDelayNanoseconds;
    m_telemetry.lastAdmittedBatchFirstSequence = batchFirst.globalSequence;
    m_telemetry.lastAdmittedBatchLastSequence = batchLast.globalSequence;
    m_telemetry.lastAdmittedBatchFirstReleaseNanoseconds =
        batchFirst.canonicalRelease.nanoseconds();
    m_telemetry.lastAdmittedBatchLastReleaseNanoseconds =
        batchLast.canonicalRelease.nanoseconds();
    m_telemetry.lastAdmittedBatchFirstDeadlineNanoseconds =
        batchFirst.canonicalDeadline.nanoseconds();
    m_telemetry.lastAdmittedBatchLastDeadlineNanoseconds =
        batchLast.canonicalDeadline.nanoseconds();
    m_telemetry.lastAdmittedBatchArrivalNanoseconds = now.nanoseconds();
    m_telemetry.lastSelectedPacingWireBytesPerSecond =
        selectedPacingRate.value().wireBytesPerSecond;
    if (m_telemetry.minimumSelectedPacingWireBytesPerSecond == 0) {
        m_telemetry.minimumSelectedPacingWireBytesPerSecond =
            selectedPacingRate.value().wireBytesPerSecond;
    } else {
        m_telemetry.minimumSelectedPacingWireBytesPerSecond = (std::min)(
            m_telemetry.minimumSelectedPacingWireBytesPerSecond,
            selectedPacingRate.value().wireBytesPerSecond);
    }
    m_telemetry.maximumSelectedPacingWireBytesPerSecond = (std::max)(
        m_telemetry.maximumSelectedPacingWireBytesPerSecond,
        selectedPacingRate.value().wireBytesPerSecond);
    if (!selectedPacingRate.value().targetResidenceSatisfied) {
        ++m_telemetry.targetResidenceMissedBatches;
    }

    for (std::size_t offset = 0; offset < expiredCount; ++offset) {
        const auto index = (m_pendingHead + offset) % m_pending.size();
        m_pending[index].reset();
    }
    m_pendingHead = (m_pendingHead + expiredCount) % m_pending.size();
    m_pendingCount -= expiredCount;
    for (const auto& reservation : m_newPending) {
        const auto index =
            (m_pendingHead + m_pendingCount) % m_pending.size();
        m_pending[index].emplace(reservation);
        ++m_pendingCount;
    }
    m_pendingDatagrams = backlogDatagrams;
    m_pendingWireBytes = backlogWireBytes;
    for (const auto& [endpointId, expired] : m_expiredByEndpoint) {
        auto endpoint = m_pendingByEndpoint.find(endpointId);
        endpoint->second.datagrams -= expired.datagrams;
        endpoint->second.bytes -= expired.bytes;
    }
    for (const auto& [endpointId, usage] : m_batchByEndpoint) {
        auto endpoint = m_pendingByEndpoint.find(endpointId);
        endpoint->second.datagrams += usage.datagrams;
        endpoint->second.bytes += usage.bytes;
    }
    m_physicalAvailable = physicalAvailable;
    m_sustainedDebtUntil = sustainedDebtUntil;
    m_previousCanonicalRelease = previousRelease;
    m_previousCanonicalDeadline = previousDeadline;
    m_previousGlobalSequence = previousSequence;
    m_previousNow = now;
    return output;
}

} // namespace media::ffmpeg::graph
