#include "internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h"

#include <memory>
#include <new>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {

const MediaPreparedRtpDatagramAllocationFacts&
MediaRawRtpPreparedByteBudget::entryAllocationFacts() const noexcept
{
    return m_plan.entryAllocation;
}

MediaRawRtpPreparedByteBudget::MediaRawRtpPreparedByteBudget(
    MediaRawRtpPreparationStoragePlan plan,
    std::shared_ptr<MediaPreparationStorageBudget> storage,
    std::shared_ptr<MediaPreparationMemoryResource> resource) noexcept
    : m_plan(plan),
      m_storage(std::move(storage)), m_resource(std::move(resource))
{
}

::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>
MediaRawRtpPreparedByteBudget::create(const MediaRawRtpPreparationStoragePlan& plan,
    const MediaPreparationStorageScope& scope) try
{
    if (!plan.maximumObservedWireBytes || !plan.maximumDatagramsPerStream ||
        !plan.streamCount || !plan.entryAllocation.bytes || !plan.entryAllocation.alignment ||
        plan.maximumAllocationBytes < plan.maximumObservedWireBytes) {
        return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared byte capacity must be positive"));
    }
    auto storage = MediaPreparationStorageBudget::create(plan.maximumAllocationBytes, scope, MediaStorageReleaseUnobserved{});
    if (!storage) return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::failure(storage.error());
    auto resource = MediaPreparationMemoryResource::create(storage.value());
    if (!resource) return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::failure(resource.error());
    return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::success(
        std::shared_ptr<MediaRawRtpPreparedByteBudget>(new MediaRawRtpPreparedByteBudget(
            plan, std::move(storage).value(), std::move(resource).value())));
}

catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaRawRtpPreparedByteBudget>>::failure(
        ::media::ErrorInfo::allocationFailed("raw RTP prepared budget allocation failed"));
}

::media::Status MediaRawRtpPreparedByteBudget::observe(std::size_t bytes)
{
    std::scoped_lock lock(m_mutex);
    return observeLocked(bytes);
}

std::pmr::memory_resource& MediaRawRtpPreparedByteBudget::storageResource() const noexcept
{
    return *m_resource;
}

MediaPreparedRtpStorageReservation MediaRawRtpPreparedByteBudget::storageReservation() const
{
    return {m_storage, m_plan.maximumAllocationBytes, m_plan.streamCount,
        "prepared-raw-rtp-exact-allocation-capacity"};
}

::media::Status MediaRawRtpPreparedByteBudget::fail(
    ::media::ErrorInfo error)
{
    std::scoped_lock lock(m_mutex);
    return failLocked(std::move(error));
}

::media::Status MediaRawRtpPreparedByteBudget::acquireProbe()
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_phase != MediaRawRtpPreparationPhase::Capture || m_probeActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP probe requires an unsealed budget without another probe lease"));
    }
    m_probeActive = true;
    return ::media::Status::success();
}

void MediaRawRtpPreparedByteBudget::releaseProbe() noexcept
{
    std::scoped_lock lock(m_mutex);
    // Only the move-only lease can release this pin, exactly once.
    m_probeActive = false;
}

::media::Status MediaRawRtpPreparedByteBudget::sealPreflight()
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_phase != MediaRawRtpPreparationPhase::Capture) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared byte budget was already sealed"));
    }
    if (m_probeActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP preflight cannot seal while a probe snapshot is retained"));
    }
    m_phase = MediaRawRtpPreparationPhase::Sealed;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::requireSealed() const
{
    std::scoped_lock lock(m_mutex);
    if (m_error) return ::media::Status::failure(*m_error);
    return m_phase != MediaRawRtpPreparationPhase::Capture
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
    const auto storage = m_storage->snapshot();
    return {storage.capacity, m_plan.maximumObservedWireBytes, m_observedBytes, storage.actualRetainedBytes, m_phase,
        m_probeActive};
}

::media::Status MediaRawRtpPreparedByteBudget::observeLocked(
    std::size_t bytes)
{
    if (m_error) return ::media::Status::failure(*m_error);
    if (m_phase == MediaRawRtpPreparationPhase::Sealed) return ::media::Status::success();
    if (bytes > m_plan.maximumObservedWireBytes - m_observedBytes) {
        return failLocked(::media::ErrorInfo::allocationFailed(
            "raw RTP probe exceeded total byte capacity: observed_bytes=" +
            std::to_string(m_observedBytes) + " capacity=" +
            std::to_string(m_plan.maximumObservedWireBytes)));
    }
    m_observedBytes += bytes;
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedByteBudget::failLocked(
    ::media::ErrorInfo error)
{
    if (!m_error) m_error = std::move(error);
    return ::media::Status::failure(*m_error);
}

} // namespace media::ffmpeg::graph
