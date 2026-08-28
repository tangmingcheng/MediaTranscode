#include "internal/graph/runtime/network/MediaDatagramBatchPacingRateSelector.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>

namespace media::ffmpeg::graph {
namespace {

::media::Result<bool> feasibleAtRate(
    std::span<const MediaDatagramPacingReservationFact> reservations,
    MediaRunningTime now,
    std::optional<MediaRunningTime> physicalAvailable,
    std::optional<MediaRunningTime> sustainedDebtUntil,
    MediaRunningTime burstDebtDuration,
    std::uint64_t rate,
    bool requireTargetResidence)
{
    using Result = ::media::Result<bool>;
    for (const auto& reservation : reservations) {
        auto burstSlack = burstDebtDuration.checkedSubtract(
            reservation.sustainedDebtDuration);
        if (!burstSlack ||
            burstSlack.value() < MediaRunningTime::fromNanoseconds(0)) {
            return Result::failure(
                burstSlack ? ::media::ErrorInfo::invalidArgument(
                    "datagram pacing reservation exceeds burst debt")
                           : burstSlack.error());
        }

        MediaRunningTime eligibility = (std::max)(
            now, reservation.canonicalRelease);
        if (physicalAvailable) {
            eligibility = (std::max)(eligibility, *physicalAvailable);
        }
        if (sustainedDebtUntil) {
            auto debtEligibility = sustainedDebtUntil->checkedSubtract(
                burstSlack.value());
            if (!debtEligibility) {
                return Result::failure(debtEligibility.error());
            }
            eligibility = (std::max)(eligibility, debtEligibility.value());
        }

        const auto debtBase = sustainedDebtUntil
            ? (std::max)(*sustainedDebtUntil, eligibility)
            : eligibility;
        auto nextDebt = debtBase.checkedAdd(
            reservation.sustainedDebtDuration);
        auto durationNanoseconds =
            MediaCheckedArithmetic::ceilDurationNanoseconds(
                reservation.wireBytes, rate,
                "datagram selected pacing duration");
        if (!durationNanoseconds) {
            return Result::failure(durationNanoseconds.error());
        }
        auto completion = eligibility.checkedAdd(
            MediaRunningTime::fromNanoseconds(durationNanoseconds.value()));
        if (!nextDebt || !completion) {
            return Result::failure(
                !nextDebt ? nextDebt.error() : completion.error());
        }
        const auto deadline = requireTargetResidence
            ? reservation.targetCompletion
            : reservation.maximumCompletion;
        if (completion.value() > deadline) {
            return Result::success(false);
        }
        physicalAvailable = completion.value();
        sustainedDebtUntil = nextDebt.value();
    }
    return Result::success(true);
}

} // namespace

::media::Result<MediaDatagramBatchPacingRateSelection>
MediaDatagramBatchPacingRateSelector::selectMinimumFeasibleRate(
    std::span<const MediaDatagramPacingReservationFact> reservations,
    MediaRunningTime now,
    std::optional<MediaRunningTime> physicalAvailable,
    std::optional<MediaRunningTime> sustainedDebtUntil,
    MediaRunningTime burstDebtDuration,
    std::uint64_t sustainedWireBytesPerSecond,
    std::uint64_t peakWireBytesPerSecond)
{
    using Result =
        ::media::Result<MediaDatagramBatchPacingRateSelection>;
    if (reservations.empty() ||
        now < MediaRunningTime::fromNanoseconds(0) ||
        burstDebtDuration <= MediaRunningTime::fromNanoseconds(0) ||
        sustainedWireBytesPerSecond == 0 ||
        peakWireBytesPerSecond < sustainedWireBytesPerSecond) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram pacing rate selection requires a valid service envelope"));
    }
    for (const auto& reservation : reservations) {
        if (reservation.wireBytes == 0 ||
            reservation.targetCompletion < reservation.canonicalRelease ||
            reservation.maximumCompletion < reservation.targetCompletion ||
            reservation.sustainedDebtDuration <=
                MediaRunningTime::fromNanoseconds(0)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "datagram pacing reservation has invalid target or maximum completion"));
        }
    }

    auto sustainedMeetsTarget = feasibleAtRate(
        reservations, now, physicalAvailable, sustainedDebtUntil,
        burstDebtDuration, sustainedWireBytesPerSecond, true);
    if (!sustainedMeetsTarget) {
        return Result::failure(sustainedMeetsTarget.error());
    }
    if (sustainedMeetsTarget.value()) {
        return Result::success(MediaDatagramBatchPacingRateSelection{
            sustainedWireBytesPerSecond, true});
    }

    auto peakFeasible = feasibleAtRate(
        reservations, now, physicalAvailable, sustainedDebtUntil,
        burstDebtDuration, peakWireBytesPerSecond, false);
    if (!peakFeasible) return Result::failure(peakFeasible.error());
    if (!peakFeasible.value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "datagram batch cannot meet maximum residence at the planned peak rate"));
    }

    auto peakMeetsTarget = feasibleAtRate(
        reservations, now, physicalAvailable, sustainedDebtUntil,
        burstDebtDuration, peakWireBytesPerSecond, true);
    if (!peakMeetsTarget) return Result::failure(peakMeetsTarget.error());
    const bool requireTargetResidence = peakMeetsTarget.value();

    if (!requireTargetResidence) {
        auto sustainedMeetsMaximum = feasibleAtRate(
            reservations, now, physicalAvailable, sustainedDebtUntil,
            burstDebtDuration, sustainedWireBytesPerSecond, false);
        if (!sustainedMeetsMaximum) {
            return Result::failure(sustainedMeetsMaximum.error());
        }
        if (sustainedMeetsMaximum.value()) {
            return Result::success(MediaDatagramBatchPacingRateSelection{
                sustainedWireBytesPerSecond, false});
        }
    }

    std::uint64_t lower = sustainedWireBytesPerSecond + 1;
    std::uint64_t upper = peakWireBytesPerSecond;
    while (lower < upper) {
        const auto candidate = lower + (upper - lower) / 2;
        auto feasible = feasibleAtRate(
            reservations, now, physicalAvailable, sustainedDebtUntil,
            burstDebtDuration, candidate, requireTargetResidence);
        if (!feasible) return Result::failure(feasible.error());
        if (feasible.value()) {
            upper = candidate;
        } else {
            lower = candidate + 1;
        }
    }
    return Result::success(MediaDatagramBatchPacingRateSelection{
        lower, requireTargetResidence});
}

} // namespace media::ffmpeg::graph
