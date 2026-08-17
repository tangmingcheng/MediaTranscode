#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"

#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isPowerOfTwo(std::size_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

::media::ErrorInfo invalidStorage(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(reason);
}

bool isKnownChannel(MediaRtpUdpChannel channel) noexcept
{
    switch (channel) {
    case MediaRtpUdpChannel::Rtp:
    case MediaRtpUdpChannel::Rtcp:
        return true;
    }
    return false;
}

} // namespace

MediaRtpIngressStorageState::~MediaRtpIngressStorageState()
{
    if (arena) {
        ::operator delete[](arena, std::align_val_t(alignmentBytes));
    }
}

MediaRtpIngressStorage::MediaRtpIngressStorage(
    std::shared_ptr<MediaRtpIngressStorageState> state) noexcept
    : m_state(std::move(state))
{
}

::media::Result<MediaRtpIngressStorage> MediaRtpIngressStorage::create(
    std::size_t byteCapacity,
    std::size_t maximumDatagramBytes,
    std::size_t descriptorCapacity,
    std::size_t alignmentBytes)
{
    if (byteCapacity == 0 || maximumDatagramBytes == 0 ||
        descriptorCapacity == 0 || !isPowerOfTwo(alignmentBytes) ||
        alignmentBytes < alignof(std::max_align_t) ||
        descriptorCapacity >
            (std::numeric_limits<std::size_t>::max)() /
                maximumDatagramBytes ||
        byteCapacity != descriptorCapacity * maximumDatagramBytes) {
        return ::media::Result<MediaRtpIngressStorage>::failure(
            invalidStorage(
                "RTP ingress storage requires exact bounded byte and descriptor geometry"));
    }
    try {
        auto state = std::make_shared<MediaRtpIngressStorageState>();
        if (descriptorCapacity > state->entries.max_size()) {
            return ::media::Result<MediaRtpIngressStorage>::failure(
                invalidStorage(
                    "RTP ingress descriptor capacity exceeds the storage limit"));
        }
        state->arena = static_cast<std::byte*>(::operator new[](
            byteCapacity, std::align_val_t(alignmentBytes)));
        state->byteCapacity = byteCapacity;
        state->maximumDatagramBytes = maximumDatagramBytes;
        state->descriptorCapacity = descriptorCapacity;
        state->alignmentBytes = alignmentBytes;
        state->entries = std::vector<MediaRtpIngressBatchEntry>(
            descriptorCapacity,
            MediaRtpIngressBatchEntry{
                MediaRtpUdpChannel::Rtp, {}, 0});
        return ::media::Result<MediaRtpIngressStorage>::success(
            MediaRtpIngressStorage(std::move(state)));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaRtpIngressStorage>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP ingress storage allocation failed"));
    }
}

std::byte* MediaRtpIngressStorage::data() noexcept
{
    return m_state ? m_state->arena : nullptr;
}

std::size_t MediaRtpIngressStorage::byteCapacity() const noexcept
{
    return m_state ? m_state->byteCapacity : 0;
}

std::size_t MediaRtpIngressStorage::maximumDatagramBytes() const noexcept
{
    return m_state ? m_state->maximumDatagramBytes : 0;
}

std::size_t MediaRtpIngressStorage::descriptorCapacity() const noexcept
{
    return m_state ? m_state->descriptorCapacity : 0;
}

std::size_t MediaRtpIngressStorage::committedEntries() const noexcept
{
    return m_state ? m_state->committedEntries : 0;
}

::media::Result<std::span<std::byte>>
MediaRtpIngressStorage::writableSlot(std::size_t index)
{
    if (!m_state || !m_state->arena || m_state->leased ||
        index != m_state->committedEntries ||
        index >= m_state->descriptorCapacity) {
        return ::media::Result<std::span<std::byte>>::failure(
            invalidStorage(
                "RTP ingress storage slot is unavailable or out of bounds"));
    }
    return ::media::Result<std::span<std::byte>>::success(
        std::span<std::byte>(
            m_state->arena + index * m_state->maximumDatagramBytes,
            m_state->maximumDatagramBytes));
}

::media::Status MediaRtpIngressStorage::commit(
    std::size_t index,
    MediaRtpUdpChannel channel,
    std::size_t byteCount,
    std::int64_t observedAtNanoseconds)
{
    if (!m_state || m_state->leased ||
        index != m_state->committedEntries ||
        index >= m_state->descriptorCapacity || byteCount == 0 ||
        byteCount > m_state->maximumDatagramBytes || !isKnownChannel(channel) ||
        observedAtNanoseconds <= 0) {
        return ::media::Status::failure(invalidStorage(
            "RTP ingress storage commit is out of order or out of bounds"));
    }
    m_state->entries[index] = MediaRtpIngressBatchEntry{
        channel,
        std::span<const std::byte>(
            m_state->arena + index * m_state->maximumDatagramBytes,
            byteCount),
        observedAtNanoseconds};
    ++m_state->committedEntries;
    return ::media::Status::success();
}

::media::Result<MediaRtpIngressBatch>
MediaRtpIngressStorage::seal(std::size_t entryCount)
{
    if (!m_state || m_state->leased || entryCount == 0 ||
        entryCount != m_state->committedEntries) {
        return ::media::Result<MediaRtpIngressBatch>::failure(
            invalidStorage(
                "RTP ingress batch seal requires every committed descriptor exactly once"));
    }
    m_state->leased = true;
    return ::media::Result<MediaRtpIngressBatch>::success(
        MediaRtpIngressBatch(
            m_state,
            std::span<const MediaRtpIngressBatchEntry>(
                m_state->entries.data(), entryCount)));
}

::media::Status MediaRtpIngressStorage::reset()
{
    if (!m_state || !m_state->arena || m_state->leased) {
        return ::media::Status::failure(invalidStorage(
            "RTP ingress storage cannot reset while a batch lease is active"));
    }
    m_state->committedEntries = 0;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
