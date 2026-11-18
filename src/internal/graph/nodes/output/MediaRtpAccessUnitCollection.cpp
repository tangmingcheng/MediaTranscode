#include "internal/graph/nodes/output/MediaRtpAccessUnitCollection.h"
#include <array>
#include <cstring>
#include <limits>
#include <optional>

namespace media::ffmpeg::graph {
namespace {
struct Block final { std::size_t bytes; std::size_t alignment; std::size_t footprint; };
using Blocks = std::array<Block, 3>;
::media::Result<Blocks> blocks(std::size_t count, std::size_t bytes)
{
    using Result = ::media::Result<Blocks>;
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    if (!count || bytes <= 12 || count > maximum / bytes ||
        count > maximum / sizeof(MediaPacketizedRtpDatagramView))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP collection geometry is empty or overflows"));
    Blocks result{{{count * bytes, alignof(std::uint8_t), 0},
        {count * sizeof(MediaPacketizedRtpDatagramView), alignof(MediaPacketizedRtpDatagramView), 0},
        {bytes, alignof(std::uint8_t), 0}}};
    for (auto& block : result) {
        auto footprint = MediaNetworkMemoryResource::allocationFootprint(block.bytes, block.alignment);
        if (!footprint) return Result::failure(footprint.error());
        block.footprint = footprint.value();
    }
    return Result::success(result);
}
} // namespace

::media::Result<std::size_t> MediaRtpAccessUnitCollection::storageFootprint(
    std::size_t datagrams, std::size_t datagramBytes)
{
    using Result = ::media::Result<std::size_t>;
    auto layout = blocks(datagrams, datagramBytes);
    if (!layout) return Result::failure(layout.error());
    std::size_t total = 0;
    for (const auto& block : layout.value()) {
        if (block.footprint > (std::numeric_limits<std::size_t>::max)() - total)
            return Result::failure(::media::ErrorInfo::invalidArgument("RTP collection footprint overflows"));
        total += block.footprint;
    }
    return Result::success(total);
}

::media::Result<MediaRtpAccessUnitCollection> MediaRtpAccessUnitCollection::prepare(
    const std::shared_ptr<MediaNetworkMemoryResource>& resource,
    MediaNetworkStorageLease& reservation, std::size_t datagrams,
    std::size_t datagramBytes, MediaRunningTime presentation, MediaRunningTime release)
{
    using Result = ::media::Result<MediaRtpAccessUnitCollection>;
    if (!resource) return Result::failure(::media::ErrorInfo::invalidArgument("RTP collection has no memory resource"));
    auto layout = blocks(datagrams, datagramBytes);
    if (!layout) return Result::failure(layout.error());
    auto total = storageFootprint(datagrams, datagramBytes);
    if (!total) return Result::failure(total.error());
    if (reservation.bytes() < total.value()) return Result::failure(
        ::media::ErrorInfo::invalidArgument("RTP collection exceeds its prepaid reservation"));
    std::array<std::optional<MediaOwnedMemoryAllocation>, 3> allocations;
    for (std::size_t i = 0; i < allocations.size(); ++i) {
        const auto& block = layout.value()[i];
        auto lease = reservation.split(block.footprint);
        if (!lease) return Result::failure(lease.error());
        auto allocation = resource->allocateFromReservation(block.bytes, block.alignment, std::move(lease).value());
        if (!allocation) return Result::failure(allocation.error());
        allocations[i].emplace(std::move(allocation).value());
    }
    auto views = MediaFixedCapacityStorage<MediaPacketizedRtpDatagramView>::fromAllocation(std::move(*allocations[1]));
    if (!views) return Result::failure(views.error());
    return Result::success(MediaRtpAccessUnitCollection(std::move(*allocations[0]),
        std::move(views).value(), std::move(*allocations[2]), datagramBytes, presentation, release));
}

MediaRtpAccessUnitCollection::MediaRtpAccessUnitCollection(MediaOwnedMemoryAllocation bytes,
    MediaFixedCapacityStorage<MediaPacketizedRtpDatagramView> views,
    MediaOwnedMemoryAllocation scratch, std::size_t datagramBytes,
    MediaRunningTime presentation, MediaRunningTime release) noexcept
    : m_bytes(std::move(bytes)), m_views(std::move(views)), m_scratch(std::move(scratch)),
      m_datagramBytes(datagramBytes), m_presentation(presentation), m_release(release) {}

std::span<std::uint8_t> MediaRtpAccessUnitCollection::scratch() noexcept
{
    if (m_sealed || !m_scratch) return {};
    return {static_cast<std::uint8_t*>(m_scratch.data()), m_scratch.bytes()};
}

::media::Status MediaRtpAccessUnitCollection::append(
    std::span<const std::uint8_t> bytes, std::size_t payloadOctets)
{
    if (m_sealed || bytes.size() < 12 || bytes.size() > m_datagramBytes ||
        !payloadOctets || payloadOctets > bytes.size() - 12 ||
        m_views.size() == m_views.capacity())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP collection rejects datagram geometry, count, or sealed state"));
    auto* target = static_cast<std::uint8_t*>(m_bytes.data()) + m_views.size() * m_datagramBytes;
    std::memcpy(target, bytes.data(), bytes.size());
    m_views.push_back({{target, bytes.size()}, payloadOctets,
        m_presentation, m_release, MediaWireMediaBoundary::None});
    return ::media::Status::success();
}

::media::Status MediaRtpAccessUnitCollection::seal(MediaWireMediaBoundary finalBoundary)
{
    if (m_sealed || m_views.empty()) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RTP collection cannot seal an empty or already sealed AU"));
    m_views.back().mediaBoundary = finalBoundary;
    m_sealed = true;
    return ::media::Status::success();
}

::media::Result<std::span<const MediaPacketizedRtpDatagramView>> MediaRtpAccessUnitCollection::datagrams() const
{
    using Result = ::media::Result<std::span<const MediaPacketizedRtpDatagramView>>;
    if (!m_sealed) return Result::failure(::media::ErrorInfo::invalidArgument("RTP collection is not sealed"));
    return Result::success(m_views.view());
}
} // namespace media::ffmpeg::graph
