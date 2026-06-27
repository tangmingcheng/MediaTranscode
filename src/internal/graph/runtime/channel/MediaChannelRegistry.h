#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/channel/MediaChannelId.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <unordered_map>

namespace media::ffmpeg::graph {

class MediaChannelRegistry final {
public:
    ::media::Result<MediaChannel*> createChannel(const MediaEdge& edge);

    MediaChannel* find(MediaChannelId id);
    const MediaChannel* find(MediaChannelId id) const;

    MediaChannel* findByEdge(MediaEdgeId edgeId);
    const MediaChannel* findByEdge(MediaEdgeId edgeId) const;

    bool remove(MediaChannelId id);
    bool removeByEdge(MediaEdgeId edgeId);
    void clear();

    std::size_t size() const noexcept;

private:
    MediaChannelId nextId();

private:
    uint32_t m_nextId = 1;
    std::unordered_map<uint32_t, std::unique_ptr<MediaChannel>> m_channels;
    std::unordered_map<uint32_t, uint32_t> m_edgeToChannel;
};

} // namespace media::ffmpeg::graph
