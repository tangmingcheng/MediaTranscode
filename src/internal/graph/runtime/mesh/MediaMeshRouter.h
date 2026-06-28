#pragma once

#include "internal/graph/runtime/mesh/MediaMeshRoute.h"
#include "media_transcode/Result.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace media::ffmpeg::graph {

class MediaMeshRouter final {
public:
    ::media::Status addRoute(MediaMeshRoute route);
    bool removeRoute(const std::string& routeId);
    const MediaMeshRoute* findRoute(const std::string& routeId) const;
    std::vector<MediaMeshRoute> routes() const;
    void clear();
    std::size_t size() const noexcept;

private:
    std::unordered_map<std::string, MediaMeshRoute> m_routes;
};

} // namespace media::ffmpeg::graph
