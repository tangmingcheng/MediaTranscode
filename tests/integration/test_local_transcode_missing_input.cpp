#include "media_transcode/LocalVideoTranscode.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

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
    const auto result = media::startLocalVideoTranscodeAsync(config);

    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_TRUE(ctx, !result.error().ok());
        EXPECT_TRUE(ctx, !result.error().message.empty());
        std::cout << "missing input error: " << result.error().describe() << '\n';
    }

    fs::remove(output, ec);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " missing-input test expectation(s) failed\n";
        return 1;
    }

    std::cout << "missing-input integration test passed\n";
    return 0;
}
