#include "media_transcode/LocalVideoTranscode.h"

#include "common/TestAssert.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

using media_transcode::test::TestContext;

void expectFailure(TestContext& ctx, const media::ErrorInfo& error)
{
    EXPECT_TRUE(ctx, !error.ok());
    EXPECT_TRUE(ctx, !error.message.empty());
    std::cout << "missing input error: " << error.describe() << '\n';
}

} // namespace

int main()
{
    const fs::path missingInput = fs::temp_directory_path() / "media_transcode_missing_input_source.mp4";
    const fs::path output = fs::temp_directory_path() / "media_transcode_missing_input_output.mp4";

    std::error_code ec;
    fs::remove(missingInput, ec);
    fs::remove(output, ec);

    media::LocalVideoTranscodeConfig config;
    config.inputPath = missingInput.string();
    config.outputPath = output.string();
    config.videoCodec = media::VideoCodec::H264;
    config.videoBitrateKbps = 700;
    config.disableHardware = true;

    TestContext ctx;
    const auto startResult = media::startLocalVideoTranscodeAsync(config);

    if (!startResult) {
        expectFailure(ctx, startResult.error());
    }
    else {
        const auto waitResult = media::waitLocalVideoTranscode(startResult.value());
        EXPECT_FALSE(ctx, waitResult);
        if (!waitResult) {
            expectFailure(ctx, waitResult.error());
        }
        else {
            (void)media::stopLocalVideoTranscode(startResult.value());
        }
    }

    fs::remove(output, ec);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " missing-input test expectation(s) failed\n";
        return 1;
    }

    std::cout << "missing-input integration test passed\n";
    return 0;
}
