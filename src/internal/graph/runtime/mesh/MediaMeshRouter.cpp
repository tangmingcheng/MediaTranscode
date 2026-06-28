#include "internal/graph/runtime/mesh/MediaMeshRouter.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaMeshRouter::addRoute(MediaMeshRoute route)
{
    if (!route.valid()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaMeshRouter addRoute failed: route is invalid"));
    }

    m_routes[route.routeId] = std::move(route);
    return ::media::Status::success();
}

bool MediaMeshRouter::removeRoute(const std::string& routeId)
{
    return m_routes.erase(routeId) > 0;
}

const MediaMeshRoute* MediaMeshRouter::findRoute(const std::string& routeId) const
{
    auto it = m_routes.find(routeId);
    return it == m_routes.end() ? nullptr : &it->second;
}

std::vector<MediaMeshRoute> MediaMeshRouter::routes() const
{
    std::vector<MediaMeshRoute> result;
    result.reserve(m_routes.size());
    for (const auto& item : m_routes) {
        result.push_back(item.second);
    }
    return result;
}

void MediaMeshRouter::clear()
{
    m_routes.clear();
}

std::size_t MediaMeshRouter::size() const noexcept
{
    return m_routes.size();
}

} // namespace media::ffmpeg::graph
