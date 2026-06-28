#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/execution/MediaGraphExecutionEngine.h"

#include <iostream>
#include <utility>

int main()
{
    using namespace media::ffmpeg::graph;

    MediaGraph graph;

    const MediaNodeId source = graph.addNode(MediaNodeKind::DebugDump, "dag-source");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "dag-sink");

    graph.addOutputPort(source,
                        "packet",
                        MediaStreamKind::Any,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        false);
    graph.addInputPort(sink,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       false);

    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = 4;

    graph.connect(source,
                  "packet",
                  sink,
                  "packet",
                  "dag-probe-edge",
                  policy);

    MediaGraphExecutionOptions options;
    options.mode = MediaGraphExecutionMode::SingleThreaded;
    options.stopOnCompletion = true;

    auto result = MediaGraphExecutionEngine::execute(std::move(graph), options);
    if (!result) {
        std::cerr << "graph dag probe failed: " << result.error().describe() << '\n';
        return 1;
    }

    const auto& run = result.value().run;
    std::cout << "graph dag probe ok: iterations=" << run.iterations
              << ", idle_iterations=" << run.idleIterations
              << ", completed=" << (run.completed ? "true" : "false")
              << '\n';
    return 0;
}
