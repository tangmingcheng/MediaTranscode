#include "internal/graph/runtime/network/MediaDatagramPacingController.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
::media::Status validateMediaDatagramPacingContract(
    const MediaDatagramPacingContract& contract) noexcept
{
    if (contract.sessionKey.empty() || contract.serviceScopeId.empty() ||
        contract.generation == 0 || contract.wireBytesPerSecond == 0 ||
        contract.maximumWireBytesPerSecond < contract.wireBytesPerSecond ||
        contract.queueTimeLimit <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing requires one complete service-scope contract"));
    }
    return ::media::Status::success();
}

bool mediaDatagramPacingContractsDescribeSamePersistentService(
    const MediaDatagramPacingContract& left,
    const MediaDatagramPacingContract& right) noexcept
{
    return left.sessionKey == right.sessionKey &&
           left.serviceScopeId == right.serviceScopeId &&
           left.wireBytesPerSecond == right.wireBytesPerSecond &&
           left.maximumWireBytesPerSecond ==
               right.maximumWireBytesPerSecond &&
           left.queueTimeLimit == right.queueTimeLimit;
}

MediaDatagramPacingController::MediaDatagramPacingController(
    MediaDatagramPacingContract contract) noexcept
    : m_contract(std::move(contract))
{
    m_telemetry.maximumWireBytesPerSecond =
        m_contract.wireBytesPerSecond;
}

::media::Result<std::unique_ptr<MediaDatagramPacingController>>
MediaDatagramPacingController::create(MediaDatagramPacingContract contract)
{
    using Result =
        ::media::Result<std::unique_ptr<MediaDatagramPacingController>>;
    auto valid = validateMediaDatagramPacingContract(contract);
    if (!valid) return Result::failure(valid.error());
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
    auto valid = validateMediaDatagramPacingContract(contract);
    if (!valid || m_pending ||
        contract.generation <= m_contract.generation ||
        !mediaDatagramPacingContractsDescribeSamePersistentService(
            m_contract, contract)) {
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
        job.endpointId == 0 || job.pacingSequence == 0 || job.wireBytes == 0 ||
        job.queue.wireBytes < job.wireBytes ||
        job.queue.averageResidence < zero ||
        job.queue.averageResidence >= m_contract.queueTimeLimit ||
        job.canonicalRelease < zero ||
        job.canonicalDeadline <= job.canonicalRelease || now < zero ||
        (m_lastObservedTime && now < *m_lastObservedTime) ||
        (m_lastSubmittedPacingSequence &&
         job.pacingSequence <= *m_lastSubmittedPacingSequence)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram pacing job violates service identity, time, or sender order"));
    }

    auto remainingQueueTime = m_contract.queueTimeLimit.checkedSubtract(
        job.queue.averageResidence);
    auto requiredQueueRate = remainingQueueTime
        ? MediaCheckedArithmetic::ceilScale(
              job.queue.wireBytes, 1'000'000'000,
              static_cast<std::uint64_t>(
                  remainingQueueTime.value().nanoseconds()),
              "Datagram WebRTC queue-time drain rate")
        : ::media::Result<std::uint64_t>::failure(
              remainingQueueTime.error());
    if (!requiredQueueRate) {
        return Result::failure(requiredQueueRate.error());
    }
    const auto candidateRate = (std::max)(
        m_contract.wireBytesPerSecond, requiredQueueRate.value());
    if (candidateRate > m_contract.maximumWireBytesPerSecond) {
        std::ostringstream message;
        message << "Datagram WebRTC queue-time adaptation exceeds the managed service capacity"
                << " pacing_sequence=" << job.pacingSequence
                << " queue_wire_bytes=" << job.queue.wireBytes
                << " average_queue_residence_ns="
                << job.queue.averageResidence.nanoseconds()
                << " queue_time_limit_ns="
                << m_contract.queueTimeLimit.nanoseconds()
                << " required_rate=" << candidateRate
                << " maximum_rate="
                << m_contract.maximumWireBytesPerSecond;
        return Result::failure(::media::ErrorInfo::ioFailure(message.str()));
    }

    const auto adjustedRate = candidateRate;

    auto durationNanoseconds = MediaCheckedArithmetic::ceilDurationNanoseconds(
        job.wireBytes, adjustedRate,
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
    if (!notAfter || effectiveStart >= notAfter.value()) {
        if (!notAfter) return Result::failure(notAfter.error());
        std::ostringstream message;
        message << "Datagram GBRA reservation misses its immutable completion deadline"
                << " pacing_sequence=" << job.pacingSequence
                << " now_ns=" << now.nanoseconds()
                << " release_ns=" << job.canonicalRelease.nanoseconds()
                << " deadline_ns=" << job.canonicalDeadline.nanoseconds()
                << " not_before_ns=" << notBefore.nanoseconds()
                << " not_after_ns=" << notAfter.value().nanoseconds()
                << " duration_ns=" << duration.nanoseconds()
                << " rate=" << adjustedRate
                << " queue_wire_bytes=" << job.queue.wireBytes
                << " average_queue_residence_ns="
                << job.queue.averageResidence.nanoseconds();
        return Result::failure(::media::ErrorInfo::ioFailure(message.str()));
    }
    if (m_telemetry.reservedDatagrams ==
            (std::numeric_limits<std::uint64_t>::max)() ||
        (adjustedRate > m_contract.wireBytesPerSecond &&
         m_telemetry.rateAdaptations ==
             (std::numeric_limits<std::uint64_t>::max)())) {
        m_telemetry.counterSaturated = true;
        return Result::failure(::media::ErrorInfo::internalError(
            "Datagram pacing reservation telemetry overflowed"));
    }

    MediaDatagramPacingReservation reservation{
        job.pacingSequence, notBefore, notAfter.value(), duration,
        adjustedRate};
    m_pending = PendingReservation{reservation};
    m_lastObservedTime = now;
    if (adjustedRate > m_contract.wireBytesPerSecond) {
        ++m_telemetry.rateAdaptations;
    }
    m_telemetry.maximumWireBytesPerSecond = (std::max)(
        m_telemetry.maximumWireBytesPerSecond, adjustedRate);
    ++m_telemetry.reservedDatagrams;
    return Result::success(reservation);
}

::media::Status MediaDatagramPacingController::markSubmitted(
    std::uint64_t pacingSequence,
    MediaRunningTime submitStartedAt,
    MediaRunningTime submitCompletedAt) noexcept
{
    if (!m_pending ||
        pacingSequence != m_pending->value.pacingSequence ||
        submitStartedAt < m_pending->value.notBefore ||
        submitStartedAt >= m_pending->value.notAfter ||
        submitCompletedAt < submitStartedAt ||
        submitCompletedAt >= m_pending->value.notAfter ||
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
        m_telemetry.worstLatePacingSequence = pacingSequence;
    }

    m_theoreticalArrivalTime = nextTheoreticalArrival.value();
    m_lastObservedTime = submitCompletedAt;
    m_lastSubmittedPacingSequence = pacingSequence;
    m_pending.reset();
    ++m_telemetry.submittedDatagrams;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
