#include "internal/graph/runtime/network/MediaForwardOnlyDatagramPacer.h"

#include <algorithm>

namespace media::ffmpeg::graph {

::media::Result<MediaRunningTime> MediaForwardOnlyDatagramPacer::prepare(
    MediaRunningTime plannedEligibility,
    MediaRunningTime submissionDeadline,
    MediaRunningTime serviceDuration)
{
    using Result = ::media::Result<MediaRunningTime>;
    if (m_pending || serviceDuration.nanoseconds() <= 0 ||
        submissionDeadline < plannedEligibility ||
        m_previousPreSubmit.has_value() !=
            m_previousServiceDuration.has_value()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "forward-only datagram pacing facts are incomplete"));
    }
    MediaRunningTime eligibility = plannedEligibility;
    if (m_previousPreSubmit) {
        auto previousCompletion = m_previousPreSubmit->checkedAdd(
            *m_previousServiceDuration);
        if (!previousCompletion) {
            return Result::failure(previousCompletion.error());
        }
        eligibility = (std::max)(eligibility, previousCompletion.value());
    }
    m_pending = Pending{
        eligibility, submissionDeadline, serviceDuration};
    return Result::success(eligibility);
}

::media::Status MediaForwardOnlyDatagramPacer::commitSuccessfulSubmit(
    MediaRunningTime actualPreSubmit) noexcept
{
    if (!m_pending || actualPreSubmit < m_pending->eligibility) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "forward-only datagram submit conflicts with its reservation"));
    }
    m_previousPreSubmit = actualPreSubmit;
    m_previousServiceDuration = m_pending->serviceDuration;
    m_pending.reset();
    return ::media::Status::success();
}

void MediaForwardOnlyDatagramPacer::reset() noexcept
{
    m_previousPreSubmit.reset();
    m_previousServiceDuration.reset();
    m_pending.reset();
}

} // namespace media::ffmpeg::graph
