#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpUrl.h"

#include <cctype>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeOptionApplier";

std::string effectiveInputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return options.input.url;
}

std::string effectiveOutputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.output.url.empty()
        ? options.output.url
        : "rtp://" + (options.output.host.empty() ? std::string("127.0.0.1") : options.output.host) +
              ":" + std::to_string(options.output.basePort);
}

std::string effectiveSdpPath(const MediaRealtimeGraphBuilderOptions& options)
{
    return options.output.sdpPath;
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

} // namespace

::media::Result<void> MediaRealtimeOptionApplier::applyInputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "url", effectiveInputUrl(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.rtsp_transport", options.input.rtspTransport); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.open_timeout_ms", std::to_string(options.input.openTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.read_timeout_ms", std::to_string(options.input.readTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.analyze_duration_us", std::to_string(options.input.analyzeDurationUs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.probe_size_bytes", std::to_string(options.input.probeSizeBytes)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.low_latency", options.input.lowLatency ? "1" : "0"); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyOutputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "url", effectiveOutputUrl(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.packet_size", std::to_string(options.output.packetSize)); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applySdpWriterOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "path", effectiveSdpPath(options)); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
