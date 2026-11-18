#include "internal/graph/runtime/buffer/MediaRawRtpPreparedByteBudget.h"
#include "internal/graph/runtime/resource/MediaPreparedAllocationCost.h"

#include <array>
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

MediaLineageAllocationScope MediaRawRtpPreparedByteBudget::allocationScope() const noexcept
{
    return MediaPreparedLineageAllocation{m_resource};
}

MediaPreparedRtpStorageReservation MediaRawRtpPreparedByteBudget::storageReservation() const
{
    return {m_storage, m_storage->snapshot().capacity, m_plan.streamCount,
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

::media::Status MediaRawRtpPreparedByteBudget::sealPreflight(
    std::optional<MediaRawRtpRuntimeStoragePlan> runtimeStorage)
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
    if (runtimeStorage) {
        if (!runtimeStorage->futureAllocationBytes || runtimeStorage->streamCount != m_plan.streamCount)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Raw RTP runtime storage does not cover its shared transport set"));
        // The caller holds every Buffer lock. The planner also covers the lazy
        // scratch and owned result of each ordinary receive outside those locks.
        // Preserve all held prefix/worker backing until its actual destruction.
        auto capacity = MediaPreparedAllocationCost::sum({
            ::media::Result<std::size_t>::success(m_storage->snapshot().actualRetainedBytes),
            ::media::Result<std::size_t>::success(runtimeStorage->futureAllocationBytes)});
        if (!capacity) return ::media::Status::failure(capacity.error());
        const std::array children{m_storage};
        const std::array capacities{capacity.value()};
        if (auto sealed = MediaPreparationStorageBudget::sealPartition(children, capacities); !sealed)
            return sealed;
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
