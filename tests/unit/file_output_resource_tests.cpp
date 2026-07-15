#include "common/TestAssert.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/output/FileOutputNode.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"

#include <filesystem>
#include <string>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

struct OutputHarness final {
    MediaGraph graph;
    MediaNodeId output;
    MediaNodeId consumer;
    MediaGraphExecutionContext execution;

    bool compile(TestContext& ctx,
                 std::string resourceKind,
                 std::string url,
                 std::string format = {})
    {
        output = graph.addNode(MediaNodeKind::FileOutput, "output");
        consumer = graph.addNode(MediaNodeKind::DebugDump, "consumer");
        graph.setNodeOption(output, MediaTranscodeOptionKey::OutputResourceKind,
                            std::move(resourceKind));
        graph.setNodeOption(output, "url", std::move(url));
        if (!format.empty()) graph.setNodeOption(output, "format", std::move(format));
        graph.addOutputPort(output, "resource", MediaStreamKind::Metadata,
                            MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
        graph.addInputPort(consumer, "resource", MediaStreamKind::Metadata,
                           MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
        MediaEdgePolicy policy;
        policy.queuePolicy.bounded = true;
        policy.queuePolicy.capacity = 1;
        graph.connect(output, "resource", consumer, "resource", "resource", policy);
        EXPECT_TRUE(ctx, execution.compile(graph));
        return execution.compiled();
    }

    MediaBufferRef run(TestContext& ctx)
    {
        FileOutputNode runtime(output);
        EXPECT_TRUE(ctx, runtime.start(execution));
        EXPECT_TRUE(ctx, runtime.process(execution));
        MediaBufferRef popped;
        EXPECT_TRUE(ctx, execution.findInputChannel(
                             consumer, "resource")->tryPop(popped));
        return popped;
    }
};

void testMissingAndUnknownKindsFailBeforeIo(TestContext& ctx)
{
    for (const std::string kind : {std::string{}, std::string("unknown")}) {
        OutputHarness harness;
        if (!harness.compile(ctx, kind, "invalid-protocol://must-not-open")) continue;
        FileOutputNode runtime(harness.output);
        EXPECT_TRUE(ctx, runtime.start(harness.execution));
        EXPECT_FALSE(ctx, runtime.process(harness.execution));
    }
}

void testExplicitFfmpegFormatContext(TestContext& ctx)
{
    OutputHarness harness;
    if (!harness.compile(ctx, "ffmpeg_format_context", "ignored", "null")) return;
    auto buffer = harness.run(ctx);
    EXPECT_TRUE(ctx, dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get()) != nullptr);
}

void testExplicitByteSink(TestContext& ctx)
{
    const auto path = std::filesystem::temp_directory_path() /
        "media_transcode_file_output_resource_test.bin";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    OutputHarness harness;
    if (!harness.compile(ctx, "byte_sink", path.string(), "mpegts")) return;
    auto buffer = harness.run(ctx);
    auto* sinkBuffer = dynamic_cast<MediaOutputByteSinkBuffer*>(buffer.get());
    EXPECT_TRUE(ctx, sinkBuffer != nullptr);
    if (sinkBuffer) {
        auto sink = sinkBuffer->takeSink();
        EXPECT_TRUE(ctx, sink);
        if (sink) EXPECT_TRUE(ctx, sink.value()->close());
        EXPECT_FALSE(ctx, sinkBuffer->takeSink());
    }
    std::filesystem::remove(path, ignored);
}

} // namespace

void runFileOutputResourceTests(TestContext& ctx)
{
    testMissingAndUnknownKindsFailBeforeIo(ctx);
    testExplicitFfmpegFormatContext(ctx);
    testExplicitByteSink(ctx);
}
