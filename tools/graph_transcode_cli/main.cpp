#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

using namespace media::ffmpeg::graph;

namespace {

std::string argValue(int argc, char** argv, const std::string& key, const std::string& fallback = {})
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return fallback;
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

std::optional<int> optionalIntArg(int argc, char** argv, const std::string& key)
{
    const std::string value = argValue(argc, argv, key);
    if (value.empty()) {
        return std::nullopt;
    }
    return std::atoi(value.c_str());
}

std::string optionalIntText(const std::optional<int>& value)
{
    return value ? std::to_string(*value) : std::string("source");
}

int failStatus(const char* action, const ::media::Status& status)
{
    std::cerr << "[CLI] " << action << " failed: " << status.error().describe() << '\n';
    return 1;
}

template <typename T>
int failResult(const char* action, const ::media::Result<T>& result)
{
    std::cerr << "[CLI] " << action << " failed: " << result.error().describe() << '\n';
    return 1;
}

LocalFileTranscodeOptions parseOptions(int argc, char** argv)
{
    LocalFileTranscodeOptions options;
    options.inputUrl = argValue(argc, argv, "--input");
    options.outputUrl = argValue(argc, argv, "--output");
    options.outputFormat = argValue(argc, argv, "--format");
    options.includeVideo = !hasArg(argc, argv, "--no-video");
    options.includeAudio = !hasArg(argc, argv, "--no-audio");
    options.audioTranscode = hasArg(argc, argv, "--audio-transcode");
    options.disableHardware = hasArg(argc, argv, "--disable-hw");
    options.useHardwareTransfer = !options.disableHardware;
    options.videoCodec = argValue(argc, argv, "--video-codec", options.videoCodec);
    options.videoEncoder = argValue(argc, argv, "--encoder", options.videoEncoder);
    options.rateControlMode = argValue(argc, argv, "--rc", options.rateControlMode);
    options.speedPreset = argValue(argc, argv, "--preset", options.speedPreset);
    options.profile = argValue(argc, argv, "--profile", options.profile);
    options.tune = argValue(argc, argv, "--tune", options.tune);
    options.level = argValue(argc, argv, "--level", options.level);
    options.width = optionalIntArg(argc, argv, "--width");
    options.height = optionalIntArg(argc, argv, "--height");
    if (auto fps = optionalIntArg(argc, argv, "--fps")) {
        options.fpsNum = fps;
        options.fpsDen = 1;
    }
    options.videoBitrateKbps = optionalIntArg(argc, argv, "--bitrate");
    options.crf = optionalIntArg(argc, argv, "--crf");
    options.quality = optionalIntArg(argc, argv, "--quality");
    options.gop = optionalIntArg(argc, argv, "--gop");
    options.maxBFrames = optionalIntArg(argc, argv, "--bframes");
    options.audioCodec = argValue(argc, argv, "--audio-codec", options.audioCodec);
    options.audioBitrateKbps = optionalIntArg(argc, argv, "--audio-bitrate");
    options.audioSampleRate = optionalIntArg(argc, argv, "--sample-rate");
    options.audioChannels = optionalIntArg(argc, argv, "--channels");
    options.diagnosticLogEnabled = !hasArg(argc, argv, "--quiet-graph");
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        std::cout << "Usage: media_transcode_graph_transcode_cli --input in.mp4 --output out.mp4 [options]\n";
        std::cout << "       add --quiet-graph to disable runtime graph diagnostics\n";
        return argc < 5 ? 2 : 0;
    }

    LocalFileTranscodeOptions options = parseOptions(argc, argv);
    std::cout << "[CLI] input=" << options.inputUrl
              << " output=" << options.outputUrl
              << " video=" << (options.includeVideo ? "on" : "off")
              << " audio=" << (options.includeAudio ? "on" : "off")
              << " width=" << optionalIntText(options.width)
              << " height=" << optionalIntText(options.height)
              << " fps=" << optionalIntText(options.fpsNum)
              << " bitrate_kbps=" << optionalIntText(options.videoBitrateKbps)
              << " rc=" << options.rateControlMode
              << " diagnostics=" << (options.diagnosticLogEnabled ? "on" : "off")
              << '\n';

    auto graphResult = LocalFileTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("graph build", graphResult);
    }

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(options.diagnosticLogEnabled);

    auto compileStatus = runtime.compile(std::move(graphResult).value());
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    auto runResult = runtime.run();
    if (!runResult) {
        return failResult("run", runResult);
    }

    const auto& result = runResult.value();
    std::cout << "[CLI] done: iterations=" << result.iterations
              << " idle_iterations=" << result.idleIterations
              << " total_pushed=" << result.totalPushed
              << " total_popped=" << result.totalPopped
              << " queued_buffers=" << result.queuedBuffers
              << " completed=" << (result.completed ? "true" : "false")
              << '\n';
    return 0;
}
