#include "media_transcode/LocalVideoTranscode.h"
#include "media_transcode/Result.h"

#include <iostream>
#include <string>

namespace {

struct TestContext {
    int failures = 0;

    void expect(bool condition, const char* expression, const char* file, int line)
    {
        if (condition) {
            return;
        }

        ++failures;
        std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    }
};

#define EXPECT_TRUE(ctx, expr) (ctx).expect(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define EXPECT_FALSE(ctx, expr) (ctx).expect(!static_cast<bool>(expr), "!(" #expr ")", __FILE__, __LINE__)
#define EXPECT_EQ(ctx, lhs, rhs) (ctx).expect(((lhs) == (rhs)), #lhs " == " #rhs, __FILE__, __LINE__)

media::LocalVideoTranscodeConfig validShapeConfig()
{
    media::LocalVideoTranscodeConfig config;
    config.inputPath = "input.mp4";
    config.outputPath = "output.mp4";
    config.disableHardware = true;
    return config;
}

void expectInvalidConfig(TestContext& ctx,
                         const media::LocalVideoTranscodeConfig& config,
                         const std::string& caseName)
{
    const auto result = media::startLocalVideoTranscodeAsync(config);
    if (result) {
        std::cerr << caseName << ": unexpectedly started; stopping job\n";
        (void)media::stopLocalVideoTranscode(result.value());
        (void)media::waitLocalVideoTranscode(result.value());
    }

    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_EQ(ctx, result.error().code, media::ErrorCode::InvalidArgument);
        EXPECT_TRUE(ctx, !result.error().message.empty());
    }
}

void testErrorInfoAndResult(TestContext& ctx)
{
    const media::ErrorInfo ok = media::ErrorInfo::success();
    EXPECT_TRUE(ctx, ok.ok());
    EXPECT_EQ(ctx, ok.code, media::ErrorCode::None);

    const media::ErrorInfo error = media::ErrorInfo::invalidArgument("bad value");
    EXPECT_FALSE(ctx, error.ok());
    EXPECT_EQ(ctx, error.code, media::ErrorCode::InvalidArgument);
    EXPECT_TRUE(ctx, error.describe().find("bad value") != std::string::npos);

    const auto success = media::Result<int>::success(42);
    EXPECT_TRUE(ctx, success);
    EXPECT_EQ(ctx, success.value(), 42);

    const auto failure = media::Result<int>::failure(error);
    EXPECT_FALSE(ctx, failure);
    EXPECT_EQ(ctx, failure.error().code, media::ErrorCode::InvalidArgument);
}

void testInvalidConfigs(TestContext& ctx)
{
    expectInvalidConfig(ctx, media::LocalVideoTranscodeConfig{}, "empty input path");

    auto emptyOutput = validShapeConfig();
    emptyOutput.outputPath.clear();
    expectInvalidConfig(ctx, emptyOutput, "empty output path");

    auto copyCodec = validShapeConfig();
    copyCodec.videoCodec = media::VideoCodec::Copy;
    expectInvalidConfig(ctx, copyCodec, "copy video codec");

    auto negativeWidth = validShapeConfig();
    negativeWidth.width = -1;
    expectInvalidConfig(ctx, negativeWidth, "negative width");

    auto negativeHeight = validShapeConfig();
    negativeHeight.height = -1;
    expectInvalidConfig(ctx, negativeHeight, "negative height");

    auto negativeFps = validShapeConfig();
    negativeFps.fps = -1;
    expectInvalidConfig(ctx, negativeFps, "negative fps");

    auto negativeBitrate = validShapeConfig();
    negativeBitrate.videoBitrateKbps = -1;
    expectInvalidConfig(ctx, negativeBitrate, "negative video bitrate");

    auto invalidRange = validShapeConfig();
    invalidRange.minVideoBitrateKbps = 3000;
    invalidRange.maxVideoBitrateKbps = 1000;
    expectInvalidConfig(ctx, invalidRange, "invalid bitrate range");

    auto negativeQuality = validShapeConfig();
    negativeQuality.quality = -1;
    expectInvalidConfig(ctx, negativeQuality, "negative quality");
}

void testNullJobHandle(TestContext& ctx)
{
    const media::LocalVideoTranscodeJobHandle job;

    EXPECT_FALSE(ctx, media::isLocalVideoTranscodeRunning(job));

    const auto stopResult = media::stopLocalVideoTranscode(job);
    EXPECT_FALSE(ctx, stopResult);
    EXPECT_EQ(ctx, stopResult.error().code, media::ErrorCode::NotInitialized);

    const auto waitResult = media::waitLocalVideoTranscode(job);
    EXPECT_FALSE(ctx, waitResult);
    EXPECT_EQ(ctx, waitResult.error().code, media::ErrorCode::NotInitialized);

    const media::ErrorInfo lastError = media::getLocalVideoTranscodeLastError(job);
    EXPECT_FALSE(ctx, lastError.ok());
    EXPECT_EQ(ctx, lastError.code, media::ErrorCode::NotInitialized);

    const media::LocalVideoTranscodeProgress progress = media::getLocalVideoTranscodeLastProgress(job);
    EXPECT_EQ(ctx, progress.frame, 0);
    EXPECT_EQ(ctx, progress.outTimeMs, 0);
    EXPECT_EQ(ctx, progress.stage, std::string{});
}

} // namespace

int main()
{
    TestContext ctx;

    testErrorInfoAndResult(ctx);
    testInvalidConfigs(ctx);
    testNullJobHandle(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " unit test expectation(s) failed\n";
        return 1;
    }

    std::cout << "all unit tests passed\n";
    return 0;
}
