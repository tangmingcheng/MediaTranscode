#include "common/TestAssert.h"

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

int main()
{
    TestContext ctx;
    MediaInputAudioStreamInfo source;
    source.streamIndex = 0;
    source.codecName = "aac";
    source.sampleRate = 48000;
    source.channels = 2;
    source.bitrateBitsPerSecond = 320000;

    MediaAudioPipelinePlannerOptions copyOptions(true);
    copyOptions.requestedCodecName = "aac";
    copyOptions.requestedSampleRate = 48000;
    copyOptions.requestedChannels = 2;
    copyOptions.requestedBitrateKbps = 320;
    const auto copy = MediaAudioPipelinePlanner::planKnownAudioTranscode(source, copyOptions);
    EXPECT_TRUE(ctx, copy);
    if (copy) EXPECT_EQ(ctx, copy.value().branchMode, MediaBranchMode::CopyPacket);

    auto transcodeOptions = copyOptions;
    transcodeOptions.requestedSampleRate = 44100;
    const auto transcode = MediaAudioPipelinePlanner::planKnownAudioTranscode(source, transcodeOptions);
    EXPECT_TRUE(ctx, transcode);
    if (transcode) EXPECT_EQ(ctx, transcode.value().branchMode, MediaBranchMode::TranscodeFrame);
    return ctx.failures == 0 ? 0 : 1;
}
