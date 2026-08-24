#include "internal/graph/runtime/network/MediaDatagramServiceShaper.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

struct UInt128 final {
    std::uint64_t high = 0;
    std::uint64_t low = 0;
    friend constexpr bool operator>=(UInt128 lhs, UInt128 rhs) noexcept
    {
        return lhs.high > rhs.high ||
               (lhs.high == rhs.high && lhs.low >= rhs.low);
    }
};

constexpr UInt128 multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
    const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhs);
    const std::uint64_t lhsHigh = lhs >> 32;
    const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhs);
    const std::uint64_t rhsHigh = rhs >> 32;
    const std::uint64_t lowProduct = lhsLow * rhsLow;
    const std::uint64_t firstCross = lhsHigh * rhsLow + (lowProduct >> 32);
    const std::uint64_t secondCross =
        lhsLow * rhsHigh + static_cast<std::uint32_t>(firstCross);
    return {lhsHigh * rhsHigh + (firstCross >> 32) + (secondCross >> 32),
            (secondCross << 32) + static_cast<std::uint32_t>(lowProduct)};
}

::media::Result<MediaRunningTime> ceilingDuration(
    std::uint64_t bytes, std::uint64_t bytesPerSecond)
{
    using Result = ::media::Result<MediaRunningTime>;
    if (bytes == 0 || bytesPerSecond == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper requires nonzero wire facts"));
    }
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
    const auto required = multiply(bytes, NanosecondsPerSecond);
    if (!(multiply(static_cast<std::uint64_t>(
                       (std::numeric_limits<std::int64_t>::max)()),
                   bytesPerSecond) >= required)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper burst duration is not representable"));
    }
    std::uint64_t first = 1;
    std::uint64_t last = static_cast<std::uint64_t>(
        (std::numeric_limits<std::int64_t>::max)());
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        if (multiply(middle, bytesPerSecond) >= required) last = middle;
        else first = middle + 1;
    }
    return Result::success(MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(first)));
}

bool sameRuntimeContract(const MediaDatagramShapingPlan& lhs,
                         const MediaDatagramShapingPlan& rhs) noexcept
{
    return lhs.sessionKey() == rhs.sessionKey() &&
           lhs.serviceScope() == rhs.serviceScope() &&
           lhs.endpoints() == rhs.endpoints() &&
           lhs.serviceCurve() == rhs.serviceCurve() &&
           lhs.backlog() == rhs.backlog() && lhs.batch() == rhs.batch() &&
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
    auto burstDuration = ceilingDuration(
        plan.serviceCurve().burstWireBytes,
        plan.serviceCurve().sustainedWireBytesPerSecond);
    if (!burstDuration) return Result::failure(burstDuration.error());
    auto result = std::unique_ptr<MediaDatagramServiceShaper>(
        new (std::nothrow) MediaDatagramServiceShaper(
            std::move(plan), burstDuration.value()));
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
    if (batch.generation() != m_plan.generation() ||
        batch.m_payload.empty() || batch.m_datagrams.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "service shaper requires an unconsumed batch for its active generation"));
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
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper reservation misses its immutable deadline"));
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
            endpointPending->bytes > endpoint->maximumPendingBytes ||
            endpointPending->bytes > endpoint->socketHardBoundBytes) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "service shaper terminates on backlog or endpoint pressure"));
        }
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

    std::vector<MediaScheduledWireDatagramBatchEntry> entries;
    try {
        entries.reserve(batch.m_datagrams.size());
        for (std::size_t index = 0; index < batch.m_datagrams.size(); ++index) {
            entries.push_back({descriptors[index],
                               batch.m_datagrams[index].takeCommitLease()});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "service shaper output entries"));
    }
    auto output = MediaScheduledWireDatagramBatchBuffer::create(
        m_plan, std::move(batch.m_payload), std::move(entries));
    if (!output) return Result::failure(output.error());

    m_pending = std::move(pending);
    m_peakAvailable = peakAvailable;
    m_sustainedDebtUntil = sustainedDebtUntil;
    m_previousCanonicalRelease = previousRelease;
    m_previousCanonicalDeadline = previousDeadline;
    m_previousGlobalSequence = previousSequence;
    return output;
}

} // namespace media::ffmpeg::graph
