#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

using namespace media::ffmpeg::graph;

int fail(const std::string& message)
{
    std::cerr << "graph autorun probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
}

template <typename T>
int failResult(const std::string& action, const ::media::Result<T>& result)
{
    return fail(action + ": " + result.error().describe());
}

std::size_t parsePositiveSize(const char* text, std::size_t fallback)
{
    if (!text) {
        return fallback;
    }

    const long value = std::strtol(text, nullptr, 10);
    return value > 0 ? static_cast<std::size_t>(value) : fallback;
}

MediaEdgePolicy queuePolicy(std::size_t capacity = 1024)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    return policy;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_autorun_probe.exe <input-media-file> [target-packets]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::size_t targetPackets = argc >= 3 ? parsePositiveSize(argv[2], 100) : 100;

    MediaGraph graph;
    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "file-input");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "demux");
    const MediaNodeId packetSink = graph.addNode(MediaNodeKind::DebugDump, "packet-sink");

    graph.setNodeOption(fileInput, "path", inputPath);

    graph.addOutputPort(fileInput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        false);
    graph.addInputPort(demux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addOutputPort(demux,
                        "packet",
                        MediaStreamKind::Any,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        true);
    graph.addInputPort(packetSink,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.connect(fileInput, "format", demux, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(demux, "packet", packetSink, "packet", "demux-to-packet-sink", queuePolicy(targetPackets + 16));

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(graph));
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    MediaChannel* demuxPacketChannel = runtime.context().findOutputChannel(demux, "packet");
    if (!demuxPacketChannel) {
        return fail("demux packet output channel not found");
    }

    auto startStatus = runtime.start();
    if (!startStatus) {
        return failStatus("runtime start", startStatus);
    }

    auto runResult = runtime.run();
    const auto& metrics = demuxPacketChannel->metrics();

    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("runtime stop", stopStatus);
    }

    if (!runResult) {
        return failResult("runtime run", runResult);
    }

    if (metrics.pushed == 0) {
        return fail("autorun did not push any packets from demux");
    }
    if (metrics.popped == 0) {
        return fail("autorun packet sink did not consume any packets");
    }
    if (metrics.popped < targetPackets) {
        return fail("target packets not reached; use a longer input or lower target-packets");
    }

    const auto& run = runResult.value();
    std::cout << "graph autorun probe ok: "
              << "run_iterations=" << run.iterations
              << ", target_packets=" << targetPackets
              << ", demux_pushed=" << metrics.pushed
              << ", sink_popped=" << metrics.popped
              << ", channel_size=" << demuxPacketChannel->size()
              << ", queue_capacity=" << demuxPacketChannel->capacity()
              << '\n';

    return 0;
}
