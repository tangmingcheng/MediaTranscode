#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

int main()
{
    TestContext ctx;
    MediaGraph graph;
    const auto node = graph.addNode(MediaNodeKind::DebugDump, "builder.node");
    EXPECT_TRUE(ctx, MediaGraphBuildSupport::setNodeOptionChecked(
                         graph, "builder test", node, "required.option", "value"));
    const MediaNode* built = graph.findNode(node);
    EXPECT_TRUE(ctx, built != nullptr);
    if (built) EXPECT_EQ(ctx, built->options.value("required.option"), std::string("value"));
    EXPECT_FALSE(ctx, MediaGraphBuildSupport::setNodeOptionChecked(
                          graph, "builder test", MediaNodeId::invalid(), "invalid", "value"));
    return ctx.failures == 0 ? 0 : 1;
}
