#include "internal/graph/runtime/buffer/MediaRawRtpProbeLease.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"
#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaRawRtpProbeLease::MediaRawRtpProbeLease(
    std::shared_ptr<MediaRawRtpPreparedByteBudget> budget,
    MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner> entries,
    std::uint64_t frontier,
    std::shared_ptr<const MediaRawRtpPreparedInputBuffer> owner) noexcept
    : m_budget(std::move(budget)), m_owner(std::move(owner)),
      m_entries(std::move(entries)), m_frontier(frontier) {}

MediaRawRtpProbeLease::~MediaRawRtpProbeLease() { release(); }

MediaRawRtpProbeLease::MediaRawRtpProbeLease(MediaRawRtpProbeLease&& other) noexcept
    : m_budget(std::move(other.m_budget)), m_owner(std::move(other.m_owner)),
      m_entries(std::move(other.m_entries)),
      m_frontier(std::exchange(other.m_frontier, 0)) {}

MediaRawRtpProbeLease& MediaRawRtpProbeLease::operator=(MediaRawRtpProbeLease&& other) noexcept
{
    if (this != &other) {
        release();
        m_budget = std::move(other.m_budget);
        m_owner = std::move(other.m_owner);
        m_entries = std::move(other.m_entries);
        m_frontier = std::exchange(other.m_frontier, 0);
    }
    return *this;
}

std::size_t MediaRawRtpProbeLease::datagramCount() const noexcept { return m_entries.size(); }
std::uint64_t MediaRawRtpProbeLease::ingressFrontier() const noexcept { return m_frontier; }

::media::Result<MediaRawRtpProbeDatagram>
MediaRawRtpProbeLease::datagram(std::size_t index) const
{
    using Result = ::media::Result<MediaRawRtpProbeDatagram>;
    if (!m_owner || !m_budget || index >= m_entries.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP probe index requires an active lease frontier"));
    if (auto status = m_budget->validate(); !status) return Result::failure(status.error());
    const auto& entry = *m_entries[index];
    return Result::success({entry.datagram.channel, entry.datagram.bytes, entry.observedAtNs});
}

void MediaRawRtpProbeLease::release() noexcept
{
    // Free the table and its last entry references before allowing another
    // snapshot: the planner admits only one table per shared AV budget.
    m_entries = {};
    m_frontier = 0;
    if (m_budget) {
        m_budget->releaseProbe();
        m_budget.reset();
    }
    m_owner.reset();
}

::media::Result<MediaRawRtpProbeLease> MediaRawRtpProbeLease::capture(
    const MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner>& entries,
    std::uint64_t frontier,
    const std::shared_ptr<MediaRawRtpPreparedByteBudget>& budget,
    std::shared_ptr<const MediaRawRtpPreparedInputBuffer> owner) try
{
    using Result = ::media::Result<MediaRawRtpProbeLease>;
    if (!budget || !owner || entries.empty() || !frontier)
        return Result::failure(::media::ErrorInfo::notInitialized(
            "raw RTP probe requires retained datagrams and their byte budget"));
    if (auto status = budget->acquireProbe(); !status) return Result::failure(status.error());
    // Own the pin before any allocation; failure unwinding releases it.
    MediaRawRtpProbeLease lease(budget, {}, frontier, std::move(owner));
    lease.m_entries = MediaFixedCapacityStorage<MediaPreparedRtpDatagramOwner>(
        entries.size(), budget->storageResource());
    std::uint64_t previous = 0;
    for (const auto& entry : entries) {
        if (!entry || entry->datagram.bytes.empty() || entry->observedAtNs <= 0 ||
            entry->ordinal <= previous || entry->ordinal > frontier)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP probe entries require ordered immutable arrival evidence"));
        previous = entry->ordinal;
        lease.m_entries.push_back(entry);
    }
    return Result::success(std::move(lease));
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaRawRtpProbeLease>::failure(
        ::media::ErrorInfo::allocationFailed("raw RTP probe snapshot allocation failed"));
}

} // namespace media::ffmpeg::graph
