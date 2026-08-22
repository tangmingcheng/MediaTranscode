#include "internal/graph/runtime/buffer/MediaScheduledDatagramBatchBuilder.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaScheduledDatagramBatchBuilder::MediaScheduledDatagramBatchBuilder(
    std::uint64_t generation,
    std::uint64_t maximumPayloadBytes) noexcept
    : m_generation(generation),
      m_maximumPayloadBytes(maximumPayloadBytes)
{
}

::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuilder>>
MediaScheduledDatagramBatchBuilder::create(
    std::uint64_t generation,
    std::uint64_t maximumPayloadBytes)
{
    using Result = ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuilder>>;
    if (generation == 0 || maximumPayloadBytes == 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch builder requires a generation and payload byte bound"));
    }
    try {
        return Result::success(std::shared_ptr<MediaScheduledDatagramBatchBuilder>(
            new MediaScheduledDatagramBatchBuilder(
                generation, maximumPayloadBytes)));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaScheduledDatagramBatchBuilder"));
    }
}

::media::Status MediaScheduledDatagramBatchBuilder::append(
    std::span<const std::uint8_t> bytes,
    MediaRunningTime enqueueNotBefore,
    MediaRunningTime enqueueDeadline,
    MediaRunningTime serviceDuration)
{
    if (!m_active) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch builder cannot append after release"));
    }
    auto completion = enqueueNotBefore.checkedAdd(serviceDuration);
    if (bytes.empty() ||
        enqueueNotBefore < MediaRunningTime::fromNanoseconds(0) ||
        enqueueDeadline < enqueueNotBefore ||
        serviceDuration <= MediaRunningTime::fromNanoseconds(0) ||
        !completion || m_payload.size() > m_maximumPayloadBytes ||
        bytes.size() > m_maximumPayloadBytes - m_payload.size()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch requires valid timing within its payload byte bound"));
    }
    if (!m_descriptors.empty()) {
        auto previousCompletion =
            m_descriptors.back().enqueueNotBefore.checkedAdd(
                m_descriptors.back().serviceDuration);
        if (!previousCompletion ||
            enqueueNotBefore < previousCompletion.value() ||
            enqueueDeadline < m_descriptors.back().enqueueDeadline) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "scheduled datagram batch builder rejects overlapping or deadline-regressing reservations"));
        }
    }
    const auto offset = m_payload.size();
    try {
        m_payload.insert(m_payload.end(), bytes.begin(), bytes.end());
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "scheduled datagram bytes"));
    }
    try {
        m_descriptors.push_back(MediaScheduledDatagramDescriptor{
            static_cast<std::uint64_t>(offset),
            static_cast<std::uint64_t>(bytes.size()),
            enqueueNotBefore,
            enqueueDeadline,
            serviceDuration});
    } catch (const std::bad_alloc&) {
        m_payload.resize(offset);
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "scheduled datagram batch entries"));
    }
    return ::media::Status::success();
}

::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>>
MediaScheduledDatagramBatchBuilder::release()
{
    if (!m_active || m_descriptors.empty()) {
        return ::media::Result<std::shared_ptr<MediaScheduledDatagramBatchBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled datagram batch builder requires one unreleased batch"));
    }
    auto batch = MediaScheduledDatagramBatchBuffer::create(
        m_generation, std::move(m_payload), std::move(m_descriptors));
    if (!batch) return batch;
    m_payload.clear();
    m_descriptors.clear();
    m_active = false;
    return batch;
}

::media::Status MediaScheduledDatagramBatchBuilder::beginNextBatch()
{
    if (m_active || !m_descriptors.empty() || !m_payload.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "scheduled datagram batch builder cannot begin before release"));
    }
    m_active = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
