#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace {

using namespace media::ffmpeg::graph;

enum class RunLoopProbeMode {
    Target,
    Idle
};

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

RunLoopProbeMode parseMode(const std::string& text, RunLoopProbeMode fallback)
{
    if (text == "target") {
        return RunLoopProbeMode::Target;
    }
    if (text == "idle") {
        return RunLoopProbeMode::Idle;
    }
    return fallback;
}

const char* modeName(RunLoopProbeMode mode) noexcept
{
    return mode == RunLoopProbeMode::Target ? "target" : "idle";
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
        std::cerr << "usage: media_transcode_graph_runloop_probe.exe <input-media-file> [target-packets] [target|idle] [max-iterations]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::size_t targetPackets = argc >= 3 ? parseSize(argv[2], 100) : 100;
    const RunLoopProbeMode defaultMode = targetPackets == 0 ? RunLoopProbeMode::Idle : RunLoopProbeMode::Target;
    const RunLoopProbeMode mode = argc >= 4 ? parseMode(argv[3], defaultMode) : defaultMode;

    std::size_t maxIterations = mode == RunLoopProbeMode::Target ? targetPackets * 8 + 256 : 100000;
    if (maxIterations < 512) {
        maxIterations = 512;
    }
    if (argc >= 5) {
        maxIterations = parseSize(argv[4], maxIterations);
        if (maxIterations == 0) {
            maxIterations = 1;
        }
    }

    if (mode == RunLoopProbeMode::Target && targetPackets == 0) {
        return fail("target mode requires target-packets > 0");
    }

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

    const std::size_t packetQueueCapacity = targetPackets > 0 ? targetPackets + 16 : 1024;
    graph.connect(fileInput, "format", demux, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(demux, "packet", packetSink, "packet", "demux-to-packet-sink", queuePolicy(packetQueueCapacity));

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

    MediaGraphRunLoopOptions options;
    options.maxIterations = maxIterations;
    options.idleThreshold = 32;
    options.stopOnIdle = true;

    MediaGraphRunLoopStopPredicate stopPredicate;
    if (mode == RunLoopProbeMode::Target) {
        stopPredicate = [demuxPacketChannel, targetPackets](const MediaGraphRunLoopResult&) {
            return demuxPacketChannel->metrics().popped >= targetPackets;
        };
    }

    auto runResult = runtime.runUntil(options, std::move(stopPredicate));
    if (!runResult) {
        return fail("runtime runUntil: " + runResult.error().describe());
    }

    const auto& metrics = demuxPacketChannel->metrics();
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("runtime stop", stopStatus);
    }

    if (metrics.pushed == 0) {
        return fail("runloop did not push any packets from demux");
    }
    if (metrics.popped == 0) {
        return fail("runloop packet sink did not consume any packets");
    }
    if (mode == RunLoopProbeMode::Target && metrics.popped < targetPackets) {
        return fail("target packets not reached before run loop stopped; increase max-iterations or use a longer input");
    }
    if (mode == RunLoopProbeMode::Idle && !runResult.value().stoppedBecauseIdle) {
        return fail("idle mode stopped before graph became idle; increase max-iterations");
    }

    std::cout << "graph runloop probe ok: "
              << "mode=" << modeName(mode)
              << ", iterations=" << runResult.value().iterations
              << ", target_packets=" << targetPackets
              << ", demux_pushed=" << metrics.pushed
              << ", sink_popped=" << metrics.popped
              << ", total_pushed=" << runResult.value().totalPushed
              << ", total_popped=" << runResult.value().totalPopped
              << ", queued_buffers=" << runResult.value().queuedBuffers
              << ", stopped_because_predicate=" << (runResult.value().stoppedBecausePredicate ? "true" : "false")
              << ", stopped_because_idle=" << (runResult.value().stoppedBecauseIdle ? "true" : "false")
              << ", stopped_because_max_iterations=" << (runResult.value().stoppedBecauseMaxIterations ? "true" : "false")
              << ", idle_iterations=" << runResult.value().idleIterations
              << ", queue_capacity=" << demuxPacketChannel->capacity()
              << ", max_iterations=" << maxIterations
              << '\n';

    return 0;
}
