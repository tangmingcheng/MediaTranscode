#include "media_transcode/LocalVideoTranscode.h"

#include "common/MediaProbe.h"
#include "common/TestAssert.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

using media_transcode::test::TestContext;

constexpr int kSkipTest = 77;

fs::path uniqueOutputPath(const std::string& name)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / ("media_transcode_" + name + "_" + std::to_string(now) + ".mp4");
}

media::LocalVideoTranscodeConfig baseConfig(const fs::path& input, const fs::path& output)
{
    media::LocalVideoTranscodeConfig config;
    config.inputPath = input.string();
    config.outputPath = output.string();
    config.videoCodec = media::VideoCodec::H264;
    config.videoBitrateKbps = 700;
    config.width = 160;
    config.height = 120;
    config.noAudio = true;
    config.disableHardware = true; // Default integration tests must not require GPU/hardware codecs.
    return config;
}

bool hasNonEmptyFile(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec) && !ec && fs::file_size(path, ec) > 0 && !ec;
}

void removeIfExists(const fs::path& path)
{
    std::error_code ec;
    fs::remove(path, ec);
}

void expectOutputMatchesConfig(TestContext& ctx,
                               const fs::path& output,
                               const media::LocalVideoTranscodeConfig& config)
{
    EXPECT_TRUE(ctx, hasNonEmptyFile(output));

    media_transcode::test::MediaProbeInfo probe;
    std::string probeError;
    const bool probed = media_transcode::test::probeMediaFile(output.string(), probe, probeError);
    EXPECT_TRUE(ctx, probed);
    if (!probed) {
        std::cerr << "probe failed for " << output.string() << ": " << probeError << '\n';
        return;
    }

    EXPECT_TRUE(ctx, probe.hasVideo);
    EXPECT_EQ(ctx, probe.videoStreamCount, 1);
    EXPECT_EQ(ctx, probe.videoCodecName, std::string("h264"));
    EXPECT_EQ(ctx, probe.videoWidth, config.width);
    EXPECT_EQ(ctx, probe.videoHeight, config.height);
    EXPECT_FALSE(ctx, probe.hasAudio);
    EXPECT_EQ(ctx, probe.audioStreamCount, 0);
    EXPECT_TRUE(ctx, probe.durationSeconds > 0.0);
    EXPECT_TRUE(ctx, probe.videoAverageFps > 0.0);
    EXPECT_TRUE(ctx, probe.videoFrameCount > 0);
}

void testSyncTranscode(TestContext& ctx, const fs::path& input)
{
    const fs::path output = uniqueOutputPath("sync");
    auto config = baseConfig(input, output);

    const auto result = media::startLocalVideoTranscodeSync(config);
    EXPECT_TRUE(ctx, result);
    if (result) {
        EXPECT_TRUE(ctx, result.value().completed);
        EXPECT_FALSE(ctx, result.value().stopped);
        expectOutputMatchesConfig(ctx, output, config);
    }
    else {
        std::cerr << "sync transcode failed: " << result.error().describe() << '\n';
    }

    removeIfExists(output);
}

void testAsyncTranscode(TestContext& ctx, const fs::path& input)
{
    const fs::path output = uniqueOutputPath("async");
    auto config = baseConfig(input, output);

    auto jobResult = media::startLocalVideoTranscodeAsync(config);
    EXPECT_TRUE(ctx, jobResult);
    if (!jobResult) {
        std::cerr << "async start failed: " << jobResult.error().describe() << '\n';
        removeIfExists(output);
        return;
    }

    const auto reportResult = media::waitLocalVideoTranscode(jobResult.value());
    EXPECT_TRUE(ctx, reportResult);
    if (reportResult) {
        EXPECT_TRUE(ctx, reportResult.value().completed);
        EXPECT_FALSE(ctx, reportResult.value().stopped);
        expectOutputMatchesConfig(ctx, output, config);
    }
    else {
        std::cerr << "async wait failed: " << reportResult.error().describe() << '\n';
    }

    removeIfExists(output);
}

void testCancelTranscodeSmoke(TestContext& ctx, const fs::path& input)
{
    const fs::path output = uniqueOutputPath("cancel_smoke");
    auto config = baseConfig(input, output);

    auto jobResult = media::startLocalVideoTranscodeAsync(config);
    EXPECT_TRUE(ctx, jobResult);
    if (!jobResult) {
        std::cerr << "cancel smoke start failed: " << jobResult.error().describe() << '\n';
        removeIfExists(output);
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto stopResult = media::stopLocalVideoTranscode(jobResult.value());
    EXPECT_TRUE(ctx, stopResult);
    if (!stopResult) {
        std::cerr << "cancel smoke stop failed: " << stopResult.error().describe() << '\n';
    }

    const auto reportResult = media::waitLocalVideoTranscode(jobResult.value());
    EXPECT_TRUE(ctx, reportResult);
    if (reportResult) {
        EXPECT_TRUE(ctx, reportResult.value().stopped || reportResult.value().completed);
    }
    else {
        std::cerr << "cancel smoke wait failed: " << reportResult.error().describe() << '\n';
    }

    removeIfExists(output);
}

void testStrictCancelTranscode(TestContext& ctx, const fs::path& input)
{
    if (!fs::exists(input)) {
        std::cout << "SKIP subtest: strict cancel sample not found: " << input.string() << '\n';
        return;
    }

    const fs::path output = uniqueOutputPath("cancel_strict");
    auto config = baseConfig(input, output);

    auto jobResult = media::startLocalVideoTranscodeAsync(config);
    EXPECT_TRUE(ctx, jobResult);
    if (!jobResult) {
        std::cerr << "strict cancel start failed: " << jobResult.error().describe() << '\n';
        removeIfExists(output);
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto stopResult = media::stopLocalVideoTranscode(jobResult.value());
    EXPECT_TRUE(ctx, stopResult);

    const auto reportResult = media::waitLocalVideoTranscode(jobResult.value());
    EXPECT_TRUE(ctx, reportResult);
    if (reportResult) {
        EXPECT_TRUE(ctx, reportResult.value().stopped);
        EXPECT_FALSE(ctx, reportResult.value().completed);
    }
    else {
        std::cerr << "strict cancel wait failed: " << reportResult.error().describe() << '\n';
    }

    removeIfExists(output);
}

} // namespace

int main(int argc, char* argv[])
{
    const fs::path sampleDir = argc > 1 ? fs::path(argv[1]) : fs::path("tests/samples");
    const fs::path input = sampleDir / "sample_h264_aac_2560x1440.mp4";
    const fs::path strictCancelInput = sampleDir / "sample_h264_aac_320x240_25s.mp4";

    if (!fs::exists(input)) {
        std::cout << "SKIP: sample file not found: " << input.string() << '\n';
        return kSkipTest;
    }

    TestContext ctx;
    testSyncTranscode(ctx, input);
    testAsyncTranscode(ctx, input);
    testCancelTranscodeSmoke(ctx, input);
    testStrictCancelTranscode(ctx, strictCancelInput);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " integration test expectation(s) failed\n";
        return 1;
    }

    std::cout << "all integration tests passed\n";
    return 0;
}
