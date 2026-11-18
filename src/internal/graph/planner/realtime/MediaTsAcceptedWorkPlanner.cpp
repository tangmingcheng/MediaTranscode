#include "internal/graph/planner/realtime/MediaTsAcceptedWorkPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
#include <limits>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {
::media::Status advanceService(MediaRunningTime& available, MediaRunningTime release,
    MediaRunningTime deadline, MediaRunningTime duration, const char* stage,
    std::uint64_t publication)
{
    if (release.nanoseconds() < 0 || deadline < release || duration.nanoseconds() <= 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS accepted service timing is invalid"));
    auto finish = (std::max)(available, release).checkedAdd(duration);
    if (!finish) return ::media::Status::failure(finish.error());
    if (finish.value() > deadline) {
        std::ostringstream message;
        message << "TS accepted-work admission check exceeds original deadline"
            << " stage=" << stage << " publication=" << publication
            << " available_ns=" << available.nanoseconds() << " release_ns=" << release.nanoseconds()
            << " deadline_ns=" << deadline.nanoseconds() << " duration_ns=" << duration.nanoseconds();
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message.str()));
    }
    available = finish.value();
    return ::media::Status::success();
}

::media::Status preparedService(const MediaTsPreparedWireWork& work, std::size_t mediaSkip,
    bool reportPending, MediaRunningTime& available, const MediaTsPrefixServicePlan& service,
    std::uint64_t publication, MediaRunningTime protocolLimit)
{
    if (mediaSkip > work.media.count)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS committed media exceeds prepared work"));
    if (reportPending && work.leadingReport) {
        const auto& report = *work.leadingReport;
        auto duration = service.durationForSerialized(report.endpointId, report.payloadBytes, report.payloadBytes, 1);
        if (!duration) return ::media::Status::failure(duration.error());
        auto status = advanceService(available, report.release, (std::min)(report.deadline, protocolLimit),
            duration.value(), "actual_report", publication);
        if (!status) return status;
    }
    if (mediaSkip == work.media.count) return ::media::Status::success();
    const auto& media = work.media;
    auto duration = service.durationForSerialized(media.endpointId, media.regularPayloadBytes,
        media.lastPayloadBytes, media.count - mediaSkip);
    if (!duration) return ::media::Status::failure(duration.error());
    return advanceService(available, media.release, (std::min)(media.deadline, protocolLimit),
        duration.value(), "actual_media", publication);
}
}

::media::Result<MediaTsPublishedWorkService> MediaTsAcceptedWorkPlanner::plan(
    std::span<MediaTsAcceptedReservationObservation> records,
    const MediaWireGlobalSequenceSnapshot& wire, std::uint64_t generation,
    MediaRunningTime observedNow, const MediaTsPrefixServicePlan& service)
{
    using Result = ::media::Result<MediaTsPublishedWorkService>;
    if (!generation || wire.generation != generation || wire.poisoned || observedNow.nanoseconds() < 0)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS accepted-work observation lost its generation"));
    auto available = observedNow;
    if (wire.lastCommittedSequence) {
        if (!wire.lastSubmittedService || wire.lastSubmittedService->generation != generation ||
            wire.lastSubmittedService->globalSequence != *wire.lastCommittedSequence ||
            !wire.lastSubmittedService->scopeAvailableAt || wire.lastSubmittedService->scopeAvailableAt->nanoseconds() < 0)
            return Result::failure(::media::ErrorInfo::invalidArgument("TS submitted service differs from the observed watermark"));
        auto sender = wire.lastSubmittedService->serviceAvailableAt();
        if (!sender) return Result::failure(sender.error());
        available = (std::max)({available, sender.value(), *wire.lastSubmittedService->scopeAvailableAt});
    } else if (wire.lastSubmittedService) {
        return Result::failure(::media::ErrorInfo::invalidArgument("TS submitted service has no observed watermark"));
    }
    // Final TS topology seals a single producer/materializer FIFO. Publication
    // order is its pending order, not a general multi-producer wire order.
    std::sort(records.begin(), records.end(), [](const auto& left, const auto& right) {
        return left.identity.publicationIdentity < right.identity.publicationIdentity;
    });
    std::uint64_t previous = 0;
    for (const auto& record : records) {
        const auto publication = record.identity.publicationIdentity;
        const auto& protocol = record.protocol;
        if (!publication || publication <= previous || protocol.generation != generation ||
            !protocol.datagramCount || !protocol.packetCount || !protocol.maximumPacketsPerDatagram ||
            protocol.datagramCount > (std::numeric_limits<std::size_t>::max)() - protocol.protocolBegin ||
            protocol.packetCount / protocol.maximumPacketsPerDatagram +
                (protocol.packetCount % protocol.maximumPacketsPerDatagram != 0) != protocol.datagramCount)
            return Result::failure(::media::ErrorInfo::invalidArgument("TS observed reservation identity or range is invalid"));
        previous = publication;
        const auto protocolLimit = (std::min)(protocol.deadline, protocol.completionLimit);
        const auto end = protocol.protocolBegin + protocol.datagramCount;
        auto next = protocol.protocolBegin;
        if (record.bound) {
            const auto& bound = *record.bound;
            const auto& range = bound.range;
            if (range.generation != generation || !bound.work.media.count || range.protocolBegin < next || range.protocolEnd > end ||
                bound.work.protocolBegin != range.protocolBegin || bound.work.media.release != protocol.release ||
                range.protocolEnd < range.protocolBegin || range.protocolEnd - range.protocolBegin != bound.work.media.count ||
                range.firstMediaSequence < range.firstSequence ||
                range.firstMediaSequence - range.firstSequence != (bound.work.leadingReport ? 1u : 0u) ||
                !wire.lastMaterializedSequence || range.lastSequence > *wire.lastMaterializedSequence ||
                range.lastSequence < range.firstMediaSequence ||
                range.lastSequence - range.firstMediaSequence != bound.work.media.count - 1)
                return Result::failure(::media::ErrorInfo::invalidArgument("TS observed bound range is inconsistent"));
            std::size_t skip = 0;
            if (wire.lastCommittedSequence && *wire.lastCommittedSequence >= range.firstMediaSequence)
                skip = *wire.lastCommittedSequence >= range.lastSequence ? bound.work.media.count
                    : static_cast<std::size_t>(*wire.lastCommittedSequence - range.firstMediaSequence + 1);
            const bool reportPending = !wire.lastCommittedSequence || *wire.lastCommittedSequence < range.firstSequence;
            auto status = preparedService(bound.work, skip, reportPending, available, service, publication, protocolLimit);
            if (!status) return Result::failure(status.error());
            next = range.protocolEnd;
        }
        if (record.prepared) {
            const auto& prepared = *record.prepared;
            if (prepared.protocolBegin != next || prepared.media.release != protocol.release ||
                !prepared.media.count || prepared.media.count > end - next)
                return Result::failure(::media::ErrorInfo::invalidArgument("TS observed prepared range is inconsistent"));
            auto status = preparedService(prepared, 0, true, available, service, publication, protocolLimit);
            if (!status) return Result::failure(status.error());
            next += prepared.media.count;
        }
        if (next == end) continue;
        const auto consumedDatagrams = next - protocol.protocolBegin;
        auto consumedPackets = MediaCheckedArithmetic::multiply(consumedDatagrams,
            protocol.maximumPacketsPerDatagram, "TS accepted protocol prefix packets");
        if (!consumedPackets || consumedPackets.value() >= protocol.packetCount)
            return Result::failure(!consumedPackets ? consumedPackets.error() :
                ::media::ErrorInfo::invalidArgument("TS accepted protocol suffix has no packets"));
        auto bytes = MediaCheckedArithmetic::multiply(protocol.packetCount - consumedPackets.value(),
            std::size_t{188}, "TS accepted protocol suffix bytes");
        if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
            return Result::failure(!bytes ? bytes.error() : ::media::ErrorInfo::invalidArgument("TS protocol suffix overflows"));
        // All datagrams in this published reservation share protocol.release.
        // Keep a possible first report even when an earlier prepared segment
        // already charged one; separate segment bounds intentionally overcount.
        auto duration = service.durationForReleaseRange(static_cast<std::size_t>(bytes.value()),
            end - next, protocol.release, protocol.release);
        if (!duration) return Result::failure(duration.error());
        auto status = advanceService(available, protocol.release, protocolLimit,
            duration.value(), "protocol_conservative_possible_report_allowance", publication);
        if (!status) return Result::failure(status.error());
    }
    return Result::success(MediaTsPublishedWorkService{available,
        {generation, observedNow, wire.lastCommittedSequence, wire.lastSubmittedService, available}});
}

::media::Status MediaTsAcceptedWorkPlanner::validateTail(const MediaTsUnpublishedTail& tail,
    MediaRunningTime availableAt, const MediaTsPrefixServicePlan& service)
{
    // The emission schedule keeps all remaining prefix releases monotone and
    // within the original AU deadline. RTCP prepare uses canonical release,
    // not availableAt; published media is excluded from this unpublished tail.
    auto duration = service.durationForReleaseRange(tail.payloadBytes, tail.datagramCount,
        tail.release, tail.deadline);
    if (!duration) return ::media::Status::failure(duration.error());
    return advanceService(availableAt, tail.release, tail.deadline, duration.value(),
        "unpublished_AU_conservative_possible_report_allowance", 0);
}
} // namespace media::ffmpeg::graph
