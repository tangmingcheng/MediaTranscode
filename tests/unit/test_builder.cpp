#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"

#include <optional>
#include <type_traits>

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
    MediaPacketCopyBranchOptions packetCopy;
    EXPECT_FALSE(ctx, packetCopy.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(packetCopy.normalizePackets), std::optional<bool>>);
    packetCopy.normalizePackets = false;
    EXPECT_FALSE(ctx, *packetCopy.normalizePackets);
    packetCopy.normalizePackets = true;
    EXPECT_TRUE(ctx, *packetCopy.normalizePackets);
    MediaVideoBranchSegmentOptions videoSegment;
    EXPECT_FALSE(ctx, videoSegment.normalizePacketCopy.has_value());
    static_assert(std::is_same_v<decltype(videoSegment.normalizePacketCopy), std::optional<bool>>);
    MediaVideoPacketCopyBranchOptions videoCopy;
    EXPECT_FALSE(ctx, videoCopy.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(videoCopy.normalizePackets), std::optional<bool>>);
    MediaAudioBranchSegmentOptions audioSegment;
    EXPECT_FALSE(ctx, audioSegment.normalizeInputPackets.has_value());
    static_assert(std::is_same_v<decltype(audioSegment.normalizeInputPackets), std::optional<bool>>);
    MediaAudioEncodeBranchOptions audioEncode;
    EXPECT_FALSE(ctx, audioEncode.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(audioEncode.normalizePackets), std::optional<bool>>);
    return ctx.failures == 0 ? 0 : 1;
}
