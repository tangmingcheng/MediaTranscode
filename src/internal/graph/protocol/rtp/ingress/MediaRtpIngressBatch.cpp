#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressBatch.h"

#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaRtpIngressBatch::MediaRtpIngressBatch(
    std::shared_ptr<MediaRtpIngressStorageState> storage,
    std::span<const MediaRtpIngressBatchEntry> entries) noexcept
    : m_storage(std::move(storage)), m_entries(entries)
{
}

MediaRtpIngressBatch::~MediaRtpIngressBatch()
{
    release();
}

MediaRtpIngressBatch::MediaRtpIngressBatch(
    MediaRtpIngressBatch&& other) noexcept
    : m_storage(std::move(other.m_storage)),
      m_entries(std::exchange(
          other.m_entries,
          std::span<const MediaRtpIngressBatchEntry>{}))
{
}

MediaRtpIngressBatch& MediaRtpIngressBatch::operator=(
    MediaRtpIngressBatch&& other) noexcept
{
    if (this != &other) {
        release();
        m_storage = std::move(other.m_storage);
        m_entries = std::exchange(
            other.m_entries,
            std::span<const MediaRtpIngressBatchEntry>{});
    }
    return *this;
}

std::span<const MediaRtpIngressBatchEntry>
MediaRtpIngressBatch::entries() const noexcept
{
    return m_entries;
}

void MediaRtpIngressBatch::release() noexcept
{
    if (m_storage) m_storage->releaseBatch();
    m_storage.reset();
    m_entries = {};
}

} // namespace media::ffmpeg::graph
