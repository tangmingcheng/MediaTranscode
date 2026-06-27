#include "internal/graph/runtime/channel/MediaChannelRegistry.h"

namespace media::ffmpeg::graph {

::media::Result<MediaChannel*> MediaChannelRegistry::createChannel(const MediaEdge& edge)
{
    if (!edge.isValid()) {
        return ::media::Result<MediaChannel*>::failure(
            ::media::ErrorInfo::invalidArgument("MediaChannelRegistry createChannel failed: edge is invalid"));
    }

    if (m_edgeToChannel.find(edge.id.value) != m_edgeToChannel.end()) {
        return ::media::Result<MediaChannel*>::failure(
            ::media::ErrorInfo::invalidArgument("MediaChannelRegistry createChannel failed: edge already has channel"));
    }

    MediaChannelId id = nextId();
    auto channel = std::make_unique<MediaChannel>(id, edge);
    MediaChannel* raw = channel.get();

    m_edgeToChannel[edge.id.value] = id.value;
    m_channels[id.value] = std::move(channel);

    return ::media::Result<MediaChannel*>::success(raw);
}

MediaChannel* MediaChannelRegistry::find(MediaChannelId id)
{
    const auto it = m_channels.find(id.value);
    return it == m_channels.end() ? nullptr : it->second.get();
}

const MediaChannel* MediaChannelRegistry::find(MediaChannelId id) const
{
    const auto it = m_channels.find(id.value);
    return it == m_channels.end() ? nullptr : it->second.get();
}

MediaChannel* MediaChannelRegistry::findByEdge(MediaEdgeId edgeId)
{
    const auto it = m_edgeToChannel.find(edgeId.value);
    if (it == m_edgeToChannel.end()) {
        return nullptr;
    }

    return find(MediaChannelId::fromValue(it->second));
}

const MediaChannel* MediaChannelRegistry::findByEdge(MediaEdgeId edgeId) const
{
    const auto it = m_edgeToChannel.find(edgeId.value);
    if (it == m_edgeToChannel.end()) {
        return nullptr;
    }

    return find(MediaChannelId::fromValue(it->second));
}

bool MediaChannelRegistry::remove(MediaChannelId id)
{
    auto it = m_channels.find(id.value);
    if (it == m_channels.end()) {
        return false;
    }

    m_edgeToChannel.erase(it->second->edgeId().value);
    m_channels.erase(it);
    return true;
}

bool MediaChannelRegistry::removeByEdge(MediaEdgeId edgeId)
{
    const auto it = m_edgeToChannel.find(edgeId.value);
    if (it == m_edgeToChannel.end()) {
        return false;
    }

    return remove(MediaChannelId::fromValue(it->second));
}

void MediaChannelRegistry::clear()
{
    m_channels.clear();
    m_edgeToChannel.clear();
    m_nextId = 1;
}

std::size_t MediaChannelRegistry::size() const noexcept
{
    return m_channels.size();
}

MediaChannelId MediaChannelRegistry::nextId()
{
    return MediaChannelId::fromValue(m_nextId++);
}

} // namespace media::ffmpeg::graph
