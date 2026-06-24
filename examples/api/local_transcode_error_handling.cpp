#include "media_transcode/LocalVideoTranscode.h"

#include <iostream>
#include <string>

namespace {

void showStartFailure(const std::string& name, media::LocalVideoTranscodeConfig config)
{
    const auto result = media::startLocalVideoTranscodeAsync(config);
    if (result) {
        std::cerr << name << ": unexpectedly started; stopping job\n";
        (void)media::stopLocalVideoTranscode(result.value());
        (void)media::waitLocalVideoTranscode(result.value());
        return;
    }

    std::cout << name << ": " << result.error().describe() << '\n';
}

} // namespace

int main()
{
    media::LocalVideoTranscodeConfig validShape;
    validShape.inputPath = "input.mp4";
    validShape.outputPath = "output.mp4";

    showStartFailure("empty input path", media::LocalVideoTranscodeConfig{});

    auto emptyOutput = validShape;
    emptyOutput.outputPath.clear();
    showStartFailure("empty output path", emptyOutput);

    auto copyCodec = validShape;
    copyCodec.videoCodec = media::VideoCodec::Copy;
    showStartFailure("unsupported copy codec", copyCodec);

    auto negativeSize = validShape;
    negativeSize.width = -1;
    showStartFailure("negative width", negativeSize);

    auto invalidBitrateRange = validShape;
    invalidBitrateRange.minVideoBitrateKbps = 3000;
    invalidBitrateRange.maxVideoBitrateKbps = 1000;
    showStartFailure("invalid bitrate range", invalidBitrateRange);

    return 0;
}
