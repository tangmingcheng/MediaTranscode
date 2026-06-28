#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

using namespace media::ffmpeg::graph;

namespace {

std::string getArg(int argc, char** argv, const std::string& key, const std::string& def = "")
{
    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return def;
}

bool hasArg(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return true;
        }
    }
    return false;
}

int parseInt(int argc, char** argv, const std::string& key, int def)
{
    const std::string value = getArg(argc, argv, key);
    if (value.empty()) {
        return def;
    }
    return std::atoi(value.c_str());
}

void printUsage()
{
    std::cout << "Usage:\n"
                 "  media_transcode_graph_transcode_cli --input input.mp4 --output out.mp4 [options]\n\n"
                 "Options:\n"
                 "  --format mp4|flv|mpegts|...\n"
                 "  --video-codec h264|hevc|mpeg4|...\n"
                 "  --encoder libx264|h264_mf|h264_nvenc|auto\n"
                 "  --width 1920\n"
                 "  --height 1080\n"
                 "  --fps 30\n"
                 "  --bitrate 8000              video bitrate in kbps\n"
                 "  --rc auto|cbr|vbr|crf\n"
                 "  --crf 23\n"
                 "  --quality 23\n"
                 "  --gop 60\n"
                 "  --bframes 0\n"
                 "  --preset veryfast\n"
                 "  --profile high\n"
                 "  --level 4.1\n"
                 "  --tune zerolatency\n"
                 "  --audio-codec aac|copy|auto\n"
                 "  --audio-bitrate 128         audio bitrate in kbps\n"
                 "  --sample-rate 48000\n"
                 "  --channels 2\n"
                 "  --no-video\n"
                 "  --no-audio\n"
                 "  --audio-transcode\n"
                 "  --disable-hw\n"
                 "  --max-iterations 1024\n"
                 "  --idle-threshold 16\n";
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    std::cerr << "[CLI] " << action << " failed: " << status.error().describe() << '\n';
    return 1;
}

template <typename T>
int failResult(const std::string& action, const ::media::Result<T>& result)
{
    std::cerr << "[CLI] " << action << " failed: " << result.error().describe() << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        printUsage();
        return argc < 5 ? 2 : 0;
    }

    LocalFileTranscodeOptions options;
    options.inputUrl = getArg(argc, argv, "--input");
    options.outputUrl = getArg(argc, argv, "--output");
    options.outputFormat = getArg(argc, argv, "--format");

    options.includeVideo = !hasArg(argc, argv, "--no-video");
    options.includeAudio = !hasArg(argc, argv, "--no-audio");
    options.audioTranscode = hasArg(argc, argv, "--audio-transcode");
    options.disableHardware = hasArg(argc, argv, "--disable-hw");
    options.useHardwareTransfer = !options.disableHardware;

    options.videoCodec = getArg(argc, argv, "--video-codec", options.videoCodec);
    options.videoEncoder = getArg(argc, argv, "--encoder", options.videoEncoder);
    options.rateControlMode = getArg(argc, argv, "--rc", options.rateControlMode);
    options.speedPreset = getArg(argc, argv, "--preset", options.speedPreset);
    options.profile = getArg(argc, argv, "--profile", options.profile);
    options.tune = getArg(argc, argv, "--tune", options.tune);
    options.level = getArg(argc, argv, "--level", options.level);

    options.width = parseInt(argc, argv, "--width", options.width);
    options.height = parseInt(argc, argv, "--height", options.height);
    options.fpsNum = parseInt(argc, argv, "--fps", options.fpsNum);
    options.videoBitrateKbps = parseInt(argc, argv, "--bitrate", options.videoBitrateKbps);
    options.crf = parseInt(argc, argv, "--crf", options.crf);
    options.quality = parseInt(argc, argv, "--quality", options.quality);
    options.gop = parseInt(argc, argv, "--gop", options.gop);
    options.maxBFrames = parseInt(argc, argv, "--bframes", options.maxBFrames);

    options.audioCodec = getArg(argc, argv, "--audio-codec", options.audioCodec);
    options.audioBitrateKbps = parseInt(argc, argv, "--audio-bitrate", options.audioBitrateKbps);
    options.audioSampleRate = parseInt(argc, argv, "--sample-rate", options.audioSampleRate);
    options.audioChannels = parseInt(argc, argv, "--channels", options.audioChannels);

    const std::size_t maxIterations = static_cast<std::size_t>(parseInt(argc, argv, "--max-iterations", 1024));
    const std::size_t idleThreshold = static_cast<std::size_t>(parseInt(argc, argv, "--idle-threshold", 16));

    std::cout << "[CLI] input=" << options.inputUrl
              << " output=" << options.outputUrl
              << " video=" << (options.includeVideo ? "on" : "off")
              << " audio=" << (options.includeAudio ? "on" : "off")
              << " width=" << options.width
              << " height=" << options.height
              << " fps=" << options.fpsNum
              << " bitrate_kbps=" << options.videoBitrateKbps
              << " rc=" << options.rateControlMode
              << '\n';

    auto graphResult = LocalFileTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("graph build", graphResult);
    }

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(graphResult).value());
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    auto startStatus = runtime.start();
    if (!startStatus) {
        return failStatus("start", startStatus);
    }

    MediaGraphRunLoopOptions runOptions;
    runOptions.maxIterations = maxIterations;
    runOptions.idleThreshold = idleThreshold;
    runOptions.stopOnIdle = true;

    auto runResult = runtime.runUntilIdle(runOptions);
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("stop", stopStatus);
    }
    if (!runResult) {
        return failResult("runUntilIdle", runResult);
    }

    const auto& result = runResult.value();
    std::cout << "[CLI] done: iterations=" << result.iterations
              << " idle_iterations=" << result.idleIterations
              << " total_pushed=" << result.totalPushed
              << " total_popped=" << result.totalPopped
              << " queued_buffers=" << result.queuedBuffers
              << " stopped_idle=" << (result.stoppedBecauseIdle ? "true" : "false")
              << " stopped_max_iterations=" << (result.stoppedBecauseMaxIterations ? "true" : "false")
              << '\n';

    return 0;
}
