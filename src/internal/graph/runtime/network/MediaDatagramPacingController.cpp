#include "internal/graph/runtime/network/MediaDatagramPacingController.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool validContract(const MediaDatagramPacingContract& contract) noexcept
{
    return !contract.sessionKey.empty() && !contract.serviceScopeId.empty() &&
           contract.generation != 0 && contract.wireBytesPerSecond != 0;
}

bool samePersistentService(const MediaDatagramPacingContract& left,
                           const MediaDatagramPacingContract& right) noexcept
{
    return left.sessionKey == right.sessionKey &&
           left.serviceScopeId == right.serviceScopeId &&
           left.wireBytesPerSecond == right.wireBytesPerSecond;
}

} // namespace

MediaDatagramPacingController::MediaDatagramPacingController(
    MediaDatagramPacingContract contract) noexcept
    : m_contract(std::move(contract))
{
}

::media::Result<std::unique_ptr<MediaDatagramPacingController>>
MediaDatagramPacingController::create(MediaDatagramPacingContract contract)
{
    using Result =
        ::media::Result<std::unique_ptr<MediaDatagramPacingController>>;
    if (!validContract(contract)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing requires one complete service-scope contract"));
    }
    auto controller = std::unique_ptr<MediaDatagramPacingController>(
        new (std::nothrow) MediaDatagramPacingController(std::move(contract)));
    if (!controller) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramPacingController"));
    }
    return Result::success(std::move(controller));
}

::media::Status MediaDatagramPacingController::rebind(
    MediaDatagramPacingContract contract)
{
    if (!validContract(contract) || m_pending ||
        contract.generation <= m_contract.generation ||
        !samePersistentService(m_contract, contract)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing rejects an active reservation, generation rollback, or service-curve change"));
    }
    m_contract = std::move(contract);
    return ::media::Status::success();
}

::media::Result<MediaDatagramPacingReservation>
MediaDatagramPacingController::reserve(
    const MediaDatagramPacingJob& job,
    MediaRunningTime now)
{
    using Result = ::media::Result<MediaDatagramPacingReservation>;
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    if (m_pending || job.generation != m_contract.generation ||
        job.endpointId == 0 || job.globalSequence == 0 || job.wireBytes == 0 ||
        job.canonicalRelease < zero ||
        job.canonicalDeadline <= job.canonicalRelease || now < zero ||
        (m_lastObservedTime && now < *m_lastObservedTime) ||
        (m_lastSubmittedSequence &&
         job.globalSequence <= *m_lastSubmittedSequence)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing job violates service identity, time, or global order"));
    }

    auto durationNanoseconds = MediaCheckedArithmetic::ceilDurationNanoseconds(
        job.wireBytes, m_contract.wireBytesPerSecond,
        "Datagram GBRA maximum-rate service increment");
    if (!durationNanoseconds || durationNanoseconds.value() <= 0) {
        return Result::failure(
            durationNanoseconds
                ? ::media::ErrorInfo::invalidArgument(
                      "Datagram GBRA maximum-rate service increment is not positive")
                : durationNanoseconds.error());
    }
    const auto duration =
        MediaRunningTime::fromNanoseconds(durationNanoseconds.value());
    const auto notBefore = m_theoreticalArrivalTime
        ? (std::max)(job.canonicalRelease, *m_theoreticalArrivalTime)
        : job.canonicalRelease;
    auto notAfter = job.canonicalDeadline.checkedSubtract(duration);
    const auto effectiveStart = (std::max)(now, notBefore);
    if (!notAfter || effectiveStart > notAfter.value()) {
        return Result::failure(
            notAfter
                ? ::media::ErrorInfo::ioFailure(
                      "Datagram GBRA reservation misses its immutable completion deadline")
                : notAfter.error());
    }
    if (m_telemetry.reservedDatagrams ==
        (std::numeric_limits<std::uint64_t>::max)()) {
        m_telemetry.counterSaturated = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "Datagram pacing reservation telemetry overflowed"));
    }

    MediaDatagramPacingReservation reservation{
        job.globalSequence, notBefore, notAfter.value(), duration};
    m_pending = PendingReservation{reservation};
    m_lastObservedTime = now;
    ++m_telemetry.reservedDatagrams;
    return Result::success(reservation);
}

::media::Status MediaDatagramPacingController::markSubmitted(
    std::uint64_t globalSequence,
    MediaRunningTime submitStartedAt,
    MediaRunningTime submitCompletedAt) noexcept
{
    if (!m_pending ||
        globalSequence != m_pending->value.globalSequence ||
        submitStartedAt < m_pending->value.notBefore ||
        submitStartedAt > m_pending->value.notAfter ||
        submitCompletedAt < submitStartedAt ||
        (m_lastObservedTime && submitStartedAt < *m_lastObservedTime)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing submit differs from its active GBRA reservation"));
    }
    auto nextTheoreticalArrival = submitCompletedAt.checkedAdd(
        m_pending->value.serviceDuration);
    if (!nextTheoreticalArrival ||
        m_telemetry.submittedDatagrams ==
            (std::numeric_limits<std::uint64_t>::max)()) {
        m_telemetry.counterSaturated = true;
        return ::media::Status::failure(
            nextTheoreticalArrival
                ? ::media::ErrorInfo::internalError(
                      "Datagram pacing submit telemetry overflowed")
                : nextTheoreticalArrival.error());
    }
    auto lateness = submitStartedAt.checkedSubtract(m_pending->value.notBefore);
    if (!lateness) return ::media::Status::failure(lateness.error());
    if (lateness.value().nanoseconds() >
        m_telemetry.maximumSubmitLatenessNanoseconds) {
        m_telemetry.maximumSubmitLatenessNanoseconds =
            lateness.value().nanoseconds();
        m_telemetry.worstLateGlobalSequence = globalSequence;
    }

    m_theoreticalArrivalTime = nextTheoreticalArrival.value();
    m_lastObservedTime = submitCompletedAt;
    m_lastSubmittedSequence = globalSequence;
    m_pending.reset();
    ++m_telemetry.submittedDatagrams;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
