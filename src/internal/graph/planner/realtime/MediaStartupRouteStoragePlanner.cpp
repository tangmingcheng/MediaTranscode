#include "internal/graph/planner/realtime/MediaStartupRouteStoragePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaStartupRouteStoragePlan> MediaStartupRouteStoragePlanner::plan(
    const MediaStartupRoutePlan& route, std::size_t videoUnits,
    std::optional<std::size_t> audioUnits, const MediaStartupBatchStorageCosts& batch)
{
    using Result = ::media::Result<MediaStartupRouteStoragePlan>;
    if (route.shape.readiness != MediaVideoStartupReadiness::DecodedPresentationSpan)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Startup route storage requires decoded video preparation"));
    auto media = MediaStartupRouteMediaPlanner::plan(route, videoUnits, audioUnits);
    if (!media) return Result::failure(media.error());

    // References bound distinct objects conservatively, including fanout aliases.
    // Each input has one producing/pending envelope; coordinator has one pending
    // envelope per stream. Its generation store holds media, not envelopes.
    constexpr std::uint64_t inputEnvelope = 1;
    constexpr std::uint64_t coordinatorEnvelope = 1;
    constexpr std::uint64_t binderTransaction = 1;
    // PassThrough reanchor retries can retain original, previous rebound, and
    // newly constructed rebound until assignment releases the previous object.
    constexpr std::uint64_t sequencerOriginal = 1;
    constexpr std::uint64_t sequencerRebound = 1;
    constexpr std::uint64_t sequencerReplacement = 1;
    constexpr std::uint64_t extractorPreparation = 1;
    constexpr std::uint64_t extractorBound = 1;
    constexpr std::uint64_t coordinatorRelease = 1;
    const std::uint64_t streams = audioUnits ? 2 : 1;
    std::uint64_t envelopes = streams * (inputEnvelope + coordinatorEnvelope);
    std::uint64_t transactions = binderTransaction + sequencerOriginal +
        sequencerRebound + sequencerReplacement + extractorPreparation + extractorBound;
    std::uint64_t releases = coordinatorRelease;
    for (const auto& event : media.value().events) {
        if (!event) continue;
        auto* count = &envelopes;
        if (event->form == MediaStartupEventMediaForm::Release) count = &releases;
        else if (event->form == MediaStartupEventMediaForm::Transaction) count = &transactions;
        auto sum = MediaCheckedArithmetic::add(*count, event->wrapperSlots, "startup route object bound");
        if (!sum) return Result::failure(sum.error());
        *count = sum.value();
    }
    // A transaction owns at most one release. Binder's unwrapped pending release
    // occupies its transaction allowance during construction, not an extra copy.
    auto releaseSum = MediaCheckedArithmetic::add(releases, transactions, "startup route releases");
    if (!releaseSum) return Result::failure(releaseSum.error());
    releases = releaseSum.value();
    MediaStartupRouteStoragePlan result{envelopes, releases, transactions, {}, 0, 0};
    const auto charge = [&](std::size_t bytes, std::uint64_t count,
                            std::size_t& target, std::size_t& domainBytes) -> ::media::Status {
        auto multiplied = MediaCheckedArithmetic::multiply(bytes, count, "startup route allocation bound");
        if (!multiplied) return ::media::Status::failure(multiplied.error());
        auto sum = MediaCheckedArithmetic::add(domainBytes, multiplied.value(), "startup route storage domain");
        if (!sum) return ::media::Status::failure(sum.error());
        if (sum.value() > (std::numeric_limits<std::size_t>::max)())
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed("Startup route storage exceeds address space"));
        target = static_cast<std::size_t>(multiplied.value());
        domainBytes = static_cast<std::size_t>(sum.value());
        return ::media::Status::success();
    };
    auto& costs = result.concurrent;
    const auto add = [&](std::size_t MediaStartupBatchStorageCosts::* field,
                         std::uint64_t count, std::size_t& domainBytes) {
        return charge(batch.*field, count, costs.*field, domainBytes);
    };
    using Costs = MediaStartupBatchStorageCosts;
    if (auto s = add(&Costs::envelopeWrapperBytes, envelopes, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::releaseWrapperBytes, releases, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::transactionWrapperBytes, transactions, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::releasedArraysBytes, releases, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::releaseGroupKeyBytes, releases, result.ordinaryBytes); !s) return Result::failure(s.error());
    // stageRelease resets its previous arrays before constructing replacements.
    constexpr std::uint64_t stagedBatch = 1;
    if (auto s = add(&Costs::stagedReferenceArraysBytes, stagedBatch, result.preparationBytes); !s) return Result::failure(s.error());
    // The old reservation still owns old audio wrappers while stageRelease creates
    // new anchored wrappers, before replacePendingBatches replaces references.
    constexpr std::uint64_t reservedAudioBatch = 1;
    if (auto s = add(&Costs::releasedAudioWrappersBytes, stagedBatch + reservedAudioBatch, result.ordinaryBytes); !s) return Result::failure(s.error());
    constexpr std::uint64_t preparationReservation = 1;
    constexpr std::uint64_t extractorReservation = 1;
    constexpr std::uint64_t authorizationOperation = 1;
    if (auto s = add(&Costs::preparationReservationBytes, preparationReservation, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::extractorReservationBytes, extractorReservation, result.preparationBytes); !s) return Result::failure(s.error());
    if (auto s = add(&Costs::authorizationScratchBytes, authorizationOperation, result.preparationBytes); !s) return Result::failure(s.error());
    return Result::success(std::move(result));
}
} // namespace media::ffmpeg::graph
