#include "internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h"

#include <memory>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

MediaRawRtpPreparedByteBudget::MediaRawRtpPreparedByteBudget(
    std::size_t capacity) noexcept
    : m_capacity(capacity)
{
}

::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>
MediaRawRtpPreparedByteBudget::create(std::size_t capacity)
{
    if (capacity == 0) {
        return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared byte capacity must be positive"));
    }
    return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::success(
        std::shared_ptr<MediaRawRtpPreparedByteBudget>(
            new MediaRawRtpPreparedByteBudget(capacity)));
}

::media::Status MediaRawRtpPreparedByteBudget::observe(std::size_t bytes)
{
    std::scoped_lock lock(m_mutex);
    return observeLocked(bytes);
}

::media::Status MediaRawRtpPreparedByteBudget::retain(
    std::size_t bytes, std::string_view stream)
{
    std::scoped_lock lock(m_mutex);
    return retainLocked(bytes, stream);
}

::media::Status MediaRawRtpPreparedByteBudget::observeAndRetain(
    std::size_t bytes, std::string_view stream)
{
    std::scoped_lock lock(m_mutex);
    if (auto status = observeLocked(bytes); !status) return status;
    return retainLocked(bytes, stream);
}

::media::Status MediaRawRtpPreparedByteBudget::release(std::size_t bytes)
{
    std::scoped_lock lock(m_mutex);
    if (bytes > m_retainedBytes) {
        return failLocked(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared aggregate byte reservation underflow"));
    }
    m_retainedBytes -= bytes;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::fail(
    ::media::ErrorInfo error)
{
    std::scoped_lock lock(m_mutex);
    return failLocked(std::move(error));
}

::media::Status MediaRawRtpPreparedByteBudget::sealPreflight()
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_runtimeActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared byte budget was already sealed"));
    }
    m_runtimeActive = true;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::requireSealed() const
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    return m_runtimeActive
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "raw RTP prepared byte budget was not sealed by preflight"));
}

::media::Status MediaRawRtpPreparedByteBudget::validate() const
{
    std::scoped_lock lock(m_mutex);
    return m_error ? ::media::Status::failure(*m_error)
                   : ::media::Status::success();
}

MediaRawRtpPreparedByteBudgetSnapshot
MediaRawRtpPreparedByteBudget::snapshot() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return {m_capacity, m_observedBytes, m_retainedBytes, m_runtimeActive};
}

::media::Status MediaRawRtpPreparedByteBudget::observeLocked(
    std::size_t bytes)
{
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_runtimeActive) return ::media::Status::success();
    if (bytes > m_capacity - m_observedBytes) {
        return failLocked(::media::ErrorInfo::allocationFailed(
            "raw RTP probe exceeded total byte capacity: observed_bytes=" +
            std::to_string(m_observedBytes) + " capacity=" +
            std::to_string(m_capacity)));
    }
    m_observedBytes += bytes;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::retainLocked(
    std::size_t bytes, std::string_view stream)
{
    if (m_error) return ::media::Status::failure(*m_error);
    if (bytes > m_capacity - m_retainedBytes) {
        return failLocked(::media::ErrorInfo::allocationFailed(
            "raw RTP prepared A/V capture exceeded aggregate byte capacity: stream=" +
            std::string(stream) + " retained_bytes=" +
            std::to_string(m_retainedBytes) + " capacity=" +
            std::to_string(m_capacity)));
    }
    m_retainedBytes += bytes;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::failLocked(
    ::media::ErrorInfo error)
{
    if (!m_error) m_error = std::move(error);
    return ::media::Status::failure(*m_error);
}

} // namespace media::ffmpeg::graph
