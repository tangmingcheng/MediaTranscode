#include "internal/graph/builder/segments/MediaBranchEndpointValidator.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

std::string message(const char* owner, const std::string& suffix)
{
    return std::string(owner ? owner : "MediaBranchEndpointValidator") + " " + suffix;
}

} // namespace

::media::Result<void> validateMediaBranchEndpoints(const char* owner,
                                                   const MediaBranchEndpointSet& endpoints)
{
    if (!endpoints.formatSourceNode.isValid() || endpoints.formatSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(message(owner, "requires format source endpoint")));
    }
    if (!endpoints.packetSourceNode.isValid() || endpoints.packetSourcePort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(message(owner, "requires packet source endpoint")));
    }
    if (!endpoints.muxNode.isValid() || endpoints.muxCodecPort.empty() || endpoints.muxPacketPort.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(message(owner, "requires mux endpoints")));
    }
    if (endpoints.queues.metadata == 0 || endpoints.queues.packet == 0 ||
        endpoints.queues.frame == 0 || endpoints.queues.mux == 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(message(owner, "queue capacities must be greater than 0")));
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
