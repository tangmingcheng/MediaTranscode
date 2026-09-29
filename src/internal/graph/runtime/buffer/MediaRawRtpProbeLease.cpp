#include "internal/graph/runtime/buffer/MediaRawRtpProbeLease.h"

#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaRawRtpProbeLease::MediaRawRtpProbeLease(
    std::shared_ptr<MediaRawRtpPreparedByteBudget> budget,
    std::size_t reservedBytes) noexcept
    : m_budget(std::move(budget)), m_reservedBytes(reservedBytes)
{
}

MediaRawRtpProbeLease::~MediaRawRtpProbeLease()
{
    release();
}

MediaRawRtpProbeLease::MediaRawRtpProbeLease(
    MediaRawRtpProbeLease&& other) noexcept
    : m_budget(std::move(other.m_budget)),
      m_payload(std::move(other.m_payload)),
      m_datagrams(std::move(other.m_datagrams)),
      m_count(std::exchange(other.m_count, 0)),
      m_reservedBytes(std::exchange(other.m_reservedBytes, 0))
{
}

MediaRawRtpProbeLease& MediaRawRtpProbeLease::operator=(
    MediaRawRtpProbeLease&& other) noexcept
{
    if (this != &other) {
        release();
        m_budget = std::move(other.m_budget);
        m_payload = std::move(other.m_payload);
        m_datagrams = std::move(other.m_datagrams);
        m_count = std::exchange(other.m_count, 0);
        m_reservedBytes = std::exchange(other.m_reservedBytes, 0);
    }
    return *this;
}

std::span<const MediaRawRtpProbeDatagram>
MediaRawRtpProbeLease::datagrams() const noexcept
{
    return {m_datagrams.get(), m_count};
}

void MediaRawRtpProbeLease::release() noexcept
{
    m_datagrams.reset();
    m_payload.reset();
    m_count = 0;
    if (m_budget) {
        m_budget->releaseProbe(m_reservedBytes);
        m_budget.reset();
    }
    m_reservedBytes = 0;
}

::media::Result<MediaRawRtpProbeLease> MediaRawRtpProbeLease::capture(
    const std::deque<MediaPreparedRawRtpDatagram>& datagrams,
    const std::shared_ptr<MediaRawRtpPreparedByteBudget>& budget)
{
    using Result = ::media::Result<MediaRawRtpProbeLease>;
    if (!budget || datagrams.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "raw RTP probe requires retained datagrams and their byte budget"));
    }
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (datagrams.size() > maximum / sizeof(MediaRawRtpProbeDatagram)) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "raw RTP probe descriptor storage size overflow"));
    }
    std::size_t reservedBytes = datagrams.size() * sizeof(MediaRawRtpProbeDatagram);
    std::size_t payloadBytes = 0;
    for (const auto& entry : datagrams) {
        const auto bytes = entry.datagram.bytes.size();
        if (bytes == 0 || entry.observedAtNs <= 0) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP probe requires nonempty datagrams with arrival evidence"));
        }
        if (bytes > maximum - reservedBytes) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "raw RTP probe payload storage size overflow"));
        }
        reservedBytes += bytes;
        payloadBytes += bytes;
    }
    if (auto status = budget->reserveProbe(reservedBytes); !status) {
        return Result::failure(status.error());
    }
    MediaRawRtpProbeLease lease(budget, reservedBytes);
    try {
        lease.m_payload = std::make_unique<std::uint8_t[]>(payloadBytes);
        lease.m_datagrams = std::make_unique<MediaRawRtpProbeDatagram[]>(datagrams.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "raw RTP probe snapshot allocation failed"));
    }
    std::size_t offset = 0;
    for (const auto& entry : datagrams) {
        const auto& source = entry.datagram;
        auto* destination = lease.m_payload.get() + offset;
        std::copy(source.bytes.begin(), source.bytes.end(), destination);
        lease.m_datagrams[lease.m_count++] = {
            source.channel, {destination, source.bytes.size()}, entry.observedAtNs};
        offset += source.bytes.size();
    }
    return Result::success(std::move(lease));
}

} // namespace media::ffmpeg::graph
