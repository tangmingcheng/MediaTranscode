#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaGraphValidation.h"

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

int main()
{
    TestContext ctx;
    MediaGraph graph;
    const auto source = graph.addNode(MediaNodeKind::DebugDump, "core.source");
    const auto sink = graph.addNode(MediaNodeKind::DebugDump, "core.sink");
    graph.addOutputPort(source, "packet", MediaStreamKind::Video,
                        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(sink, "packet", MediaStreamKind::Video,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, graph.connect(source, "packet", sink, "packet"));
    EXPECT_EQ(ctx, graph.nodeCount(), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, graph.edgeCount(), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, MediaGraphValidation::validate(graph).ok());
    return ctx.failures == 0 ? 0 : 1;
}
