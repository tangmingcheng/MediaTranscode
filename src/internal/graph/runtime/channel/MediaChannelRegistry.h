#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaChannelId.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <ranges>

namespace media::ffmpeg::graph {

class MediaChannelRegistry final {
public:
    ::media::Result<MediaChannel*> createChannel(const MediaEdge& edge);

    std::shared_ptr<MediaChannel> retainByEdge(MediaEdgeId edgeId) const;
    ::media::Status adopt(std::shared_ptr<MediaChannel> channel);
    MediaChannel* find(MediaChannelId id);
    const MediaChannel* find(MediaChannelId id) const;

    MediaChannel* findByEdge(MediaEdgeId edgeId);
    const MediaChannel* findByEdge(MediaEdgeId edgeId) const;

    // Borrowed iteration: callers retain the registry owner and must not mutate
    // its membership until traversal ends. Channel state changes are permitted.
    auto channels() noexcept {
        return m_channels | std::views::values | std::views::transform(
            [](const auto& channel) noexcept -> MediaChannel* { return channel.get(); });
    }
    auto channels() const noexcept {
        return m_channels | std::views::values | std::views::transform(
            [](const auto& channel) noexcept -> const MediaChannel* { return channel.get(); });
    }

    bool remove(MediaChannelId id);
    bool removeByEdge(MediaEdgeId edgeId);
    void clear();

    std::size_t size() const noexcept;
    bool empty() const noexcept;

private:
    MediaChannelId nextId();

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, std::shared_ptr<MediaChannel>> m_channels;
    std::unordered_map<uint32_t, uint32_t> m_edgeToChannel;
};

} // namespace media::ffmpeg::graph
