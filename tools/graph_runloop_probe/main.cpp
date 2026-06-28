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
    std::cerr << "graph runloop probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
}

std::size_t parseSize(const char* text, std::size_t fallback)
{
    if (!text) {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text) {
        return fallback;
    }
    return static_cast<std::size_t>(value);
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
        std::cerr << "usage: media_transcode_graph_runloop_probe.exe <input-media-file> [minimum-packets]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::size_t minimumPackets = argc >= 3 ? parseSize(argv[2], 1) : 1;

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
    graph.connect(demux, "packet", packetSink, "packet", "demux-to-packet-sink", queuePolicy(1024));

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

    auto runResult = runtime.runUntilIdle();
    if (!runResult) {
        runtime.stop();
        return fail("runtime runUntilIdle: " + runResult.error().describe());
    }

    const auto metrics = demuxPacketChannel->metrics();
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("runtime stop", stopStatus);
    }

    if (metrics.pushed == 0) {
        return fail("demux did not push any packets");
    }
    if (metrics.popped < minimumPackets) {
        return fail("packet sink consumed fewer packets than expected");
    }
    if (!runResult.value().stoppedBecauseIdle) {
        return fail("runtime did not naturally reach idle");
    }

    std::cout << "graph runloop probe ok: "
              << "iterations=" << runResult.value().iterations
              << ", idle_iterations=" << runResult.value().idleIterations
              << ", demux_pushed=" << metrics.pushed
              << ", sink_popped=" << metrics.popped
              << ", total_pushed=" << runResult.value().totalPushed
              << ", total_popped=" << runResult.value().totalPopped
              << ", queued_buffers=" << runResult.value().queuedBuffers
              << '\n';

    return 0;
}
