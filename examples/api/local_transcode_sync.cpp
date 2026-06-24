#include "media_transcode/LocalVideoTranscode.h"

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input-video> <output-video>\n";
        return 2;
    }

    media::LocalVideoTranscodeConfig config;
    config.inputPath = argv[1];
    config.outputPath = argv[2];
    config.videoCodec = media::VideoCodec::H264;
    config.videoBitrateKbps = 2500;
    config.disableHardware = true; // Keep the API example portable by default.

    const auto result = media::startLocalVideoTranscodeSync(
        config,
        [](const media::LocalVideoTranscodeProgress& progress) {
            std::cout << "progress: stage=" << progress.stage
                      << ", frame=" << progress.frame
                      << ", outTimeMs=" << progress.outTimeMs
                      << ", speed=" << progress.speed << "x\n";
        }
    );

    if (!result) {
        std::cerr << "transcode failed: " << result.error().describe() << '\n';
        return 1;
    }

    const media::LocalVideoTranscodeReport& report = result.value();
    std::cout << "transcode " << (report.completed ? "completed" : "ended")
              << ": frame=" << report.lastProgress.frame
              << ", outTimeMs=" << report.lastProgress.outTimeMs << '\n';
    return report.completed ? 0 : 1;
}
