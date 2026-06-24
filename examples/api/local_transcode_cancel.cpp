#include "media_transcode/LocalVideoTranscode.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, char* argv[])
{
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <input-video> <output-video> [cancel-after-ms]\n";
        return 2;
    }

    const int cancelAfterMs = argc == 4 ? std::max(0, std::atoi(argv[3])) : 1000;

    media::LocalVideoTranscodeConfig config;
    config.inputPath = argv[1];
    config.outputPath = argv[2];
    config.videoCodec = media::VideoCodec::H264;
    config.videoBitrateKbps = 2500;
    config.disableHardware = true; // Keep the API example portable by default.

    auto jobResult = media::startLocalVideoTranscodeAsync(config);
    if (!jobResult) {
        std::cerr << "start failed: " << jobResult.error().describe() << '\n';
        return 1;
    }

    media::LocalVideoTranscodeJobHandle job = jobResult.value();
    std::this_thread::sleep_for(std::chrono::milliseconds(cancelAfterMs));

    const auto stopResult = media::stopLocalVideoTranscode(job);
    if (!stopResult) {
        std::cerr << "stop failed: " << stopResult.error().describe() << '\n';
        return 1;
    }

    const auto reportResult = media::waitLocalVideoTranscode(job);
    if (!reportResult) {
        std::cerr << "wait failed: " << reportResult.error().describe() << '\n';
        return 1;
    }

    const media::LocalVideoTranscodeReport& report = reportResult.value();
    std::cout << "transcode stopped=" << report.stopped
              << ", completed=" << report.completed
              << ", frame=" << report.lastProgress.frame << '\n';
    return 0;
}
