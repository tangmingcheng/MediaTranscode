#include "media_transcode/LocalVideoTranscode.h"

#include <chrono>
#include <iostream>
#include <thread>

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

    auto jobResult = media::startLocalVideoTranscodeAsync(
        config,
        [](const media::LocalVideoTranscodeProgress& progress) {
            std::cout << "callback: stage=" << progress.stage
                      << ", frame=" << progress.frame
                      << ", outTimeMs=" << progress.outTimeMs
                      << ", speed=" << progress.speed << "x\n";
        }
    );

    if (!jobResult) {
        std::cerr << "start failed: " << jobResult.error().describe() << '\n';
        return 1;
    }

    media::LocalVideoTranscodeJobHandle job = jobResult.value();
    while (media::isLocalVideoTranscodeRunning(job)) {
        const media::LocalVideoTranscodeProgress progress =
            media::getLocalVideoTranscodeLastProgress(job);
        std::cout << "poll: stage=" << progress.stage
                  << ", frame=" << progress.frame
                  << ", outTimeMs=" << progress.outTimeMs << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    const auto reportResult = media::waitLocalVideoTranscode(job);
    if (!reportResult) {
        std::cerr << "wait failed: " << reportResult.error().describe() << '\n';
        return 1;
    }

    const media::LocalVideoTranscodeReport& report = reportResult.value();
    std::cout << "transcode " << (report.completed ? "completed" : "ended")
              << ": frame=" << report.lastProgress.frame
              << ", outTimeMs=" << report.lastProgress.outTimeMs << '\n';
    return report.completed ? 0 : 1;
}
