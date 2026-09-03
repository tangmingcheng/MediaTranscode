#include "internal/graph/runtime/buffer/MediaRtpAccessUnitPlayoutBuffer.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <new>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpAccessUnitPlayoutBuffer>
MediaRtpAccessUnitPlayoutBuffer::create(
    MediaRtpInputPlayoutPlan plan,
    int clockRate)
{
    using Result = ::media::Result<MediaRtpAccessUnitPlayoutBuffer>;
    if (auto status = plan.validate(); !status) {
        return Result::failure(status.error());
    }
    if (clockRate <= 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout requires a positive RTP clock rate"));
    }
    auto unwrapper = MediaTimestampUnwrapper::create(
        MediaTimestampCounterKind::Rtp32, 1);
    return unwrapper
        ? Result::success(MediaRtpAccessUnitPlayoutBuffer(
              std::move(plan), clockRate, std::move(unwrapper).value()))
        : Result::failure(unwrapper.error());
}

MediaRtpAccessUnitPlayoutBuffer::MediaRtpAccessUnitPlayoutBuffer(
    MediaRtpInputPlayoutPlan plan,
    int clockRate,
    MediaTimestampUnwrapper unwrapper) noexcept
    : m_plan(std::move(plan)),
      m_clockRate(clockRate),
      m_unwrapper(std::move(unwrapper))
{
}

::media::Status MediaRtpAccessUnitPlayoutBuffer::push(
    MediaBufferRef accessUnit,
    std::uint32_t rtpTimestamp)
{
    if (!accessUnit) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout requires an access unit"));
    }
    const auto payloadBytes = accessUnit->payloadFootprintBytes();
    if (!payloadBytes || *payloadBytes == 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout requires an authoritative access-unit payload footprint"));
    }
    auto mediaTime = unwrapMediaTime(rtpTimestamp);
    if (!mediaTime) return ::media::Status::failure(mediaTime.error());
    if (!m_entries.empty() &&
        mediaTime.value() < m_entries.back().mediaTimeNanoseconds) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "RTP input transmission order has non-monotonic media time"));
    }
    auto retainedBytes = MediaCheckedArithmetic::add(
        m_retainedPayloadBytes, *payloadBytes,
        "RTP input playout retained payload bytes");
    if (!retainedBytes) {
        return ::media::Status::failure(retainedBytes.error());
    }
    if (m_entries.size() >= m_plan.maximumRetainedAccessUnits) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "RTP input playout item capacity exceeded: retained=" +
            std::to_string(m_entries.size()) + " maximum=" +
            std::to_string(m_plan.maximumRetainedAccessUnits)));
    }
    if (retainedBytes.value() > m_plan.maximumRetainedPayloadBytes) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "RTP input playout byte capacity exceeded: retained=" +
            std::to_string(m_retainedPayloadBytes) + " incoming=" +
            std::to_string(*payloadBytes) + " maximum=" +
            std::to_string(m_plan.maximumRetainedPayloadBytes)));
    }
    const auto observedAt = MediaRunningTime::fromNanoseconds(
        mediaSteadyClockNowNs());
    MediaRunningTime release = MediaRunningTime::fromNanoseconds(0);
    if (m_firstReadyAt) {
        auto calculated = calculateReadyAt(
            MediaRunningTime::fromNanoseconds(mediaTime.value()));
        if (!calculated) {
            return ::media::Status::failure(calculated.error());
        }
        if (calculated.value() < observedAt) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "RTP input access unit arrived after its fixed playout deadline"));
        }
        release = calculated.value();
    }
    try {
        m_entries.push_back(Entry{
            std::move(accessUnit), mediaTime.value(), release,
            *payloadBytes});
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "RTP input playout access-unit queue"));
    }
    m_retainedPayloadBytes = retainedBytes.value();
    return activateIfReady(observedAt);
}

::media::Result<std::optional<MediaBufferRef>>
MediaRtpAccessUnitPlayoutBuffer::popReady()
{
    using Result = ::media::Result<std::optional<MediaBufferRef>>;
    if (m_entries.empty() || !m_firstReadyAt ||
        MediaRunningTime::fromNanoseconds(mediaSteadyClockNowNs()) <
            m_entries.front().readyAt) {
        return Result::success(std::nullopt);
    }
    Entry entry = std::move(m_entries.front());
    m_entries.pop_front();
    if (entry.payloadBytes > m_retainedPayloadBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout payload accounting underflow"));
    }
    m_retainedPayloadBytes -= entry.payloadBytes;
    return Result::success(std::move(entry.accessUnit));
}

std::optional<MediaRunningTime>
MediaRtpAccessUnitPlayoutBuffer::nextReadyAt() const noexcept
{
    return m_entries.empty() || !m_firstReadyAt
        ? std::nullopt
        : std::optional<MediaRunningTime>(m_entries.front().readyAt);
}

void MediaRtpAccessUnitPlayoutBuffer::reset() noexcept
{
    m_entries.clear();
    m_retainedPayloadBytes = 0;
    m_firstMediaTime.reset();
    m_firstReadyAt.reset();
    ++m_generation;
    m_unwrapper.reset(m_generation);
}

::media::Result<std::int64_t>
MediaRtpAccessUnitPlayoutBuffer::unwrapMediaTime(
    std::uint32_t rtpTimestamp)
{
    auto raw = MediaProtocolTimestamp::create(rtpTimestamp, 1, m_clockRate);
    if (!raw) return ::media::Result<std::int64_t>::failure(raw.error());
    const auto unwrapped = m_unwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value ||
        !unwrapped.timestamp) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::unsupported(
                "RTP input timestamp cannot continue the playout media timeline"));
    }
    auto mediaTime = MediaRunningTime::checkedFromTicks(
        unwrapped.timestamp->ticks(), 1, m_clockRate);
    return mediaTime
        ? ::media::Result<std::int64_t>::success(
              mediaTime.value().nanoseconds())
        : ::media::Result<std::int64_t>::failure(mediaTime.error());
}

::media::Result<MediaRunningTime>
MediaRtpAccessUnitPlayoutBuffer::calculateReadyAt(
    MediaRunningTime mediaTime) const
{
    using Result = ::media::Result<MediaRunningTime>;
    if (!m_firstMediaTime || !m_firstReadyAt) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RTP input playout timeline has not started"));
    }
    auto offset = mediaTime.checkedSubtract(*m_firstMediaTime);
    if (!offset) return Result::failure(offset.error());
    auto release = m_firstReadyAt->checkedAdd(offset.value());
    return release
        ? Result::success(release.value())
        : Result::failure(release.error());
}

::media::Status MediaRtpAccessUnitPlayoutBuffer::activateIfReady(
    MediaRunningTime observedAt)
{
    if (m_firstReadyAt ||
        m_entries.size() < m_plan.startupAccessUnits) {
        return ::media::Status::success();
    }
    m_firstMediaTime = MediaRunningTime::fromNanoseconds(
        m_entries.front().mediaTimeNanoseconds);
    m_firstReadyAt = observedAt;
    for (auto& entry : m_entries) {
        auto release = calculateReadyAt(
            MediaRunningTime::fromNanoseconds(entry.mediaTimeNanoseconds));
        if (!release) {
            return ::media::Status::failure(release.error());
        }
        entry.readyAt = release.value();
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
