#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"
#include "../common/VideoCliTranscodeOptions.h"

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

using namespace media::ffmpeg::graph;
using namespace media::ffmpeg::graph::cli;

namespace {

std::string optionalIntText(const std::optional<int>& value)
{
    return value ? std::to_string(*value) : std::string("source");
}

std::string frameRateText(const MediaFrameRateParameters& frameRate)
{
    if (!frameRate.numerator) {
        return "source";
    }
    return std::to_string(*frameRate.numerator) + "/" + std::to_string(frameRate.denominator.value_or(1));
}

void rejectUnknownLocalArgs(int argc, char** argv)
{
    std::vector<std::string> valueArgs = commonVideoTranscodeValueArgs();
    valueArgs.push_back("--input");
    valueArgs.push_back("--output");

    rejectUnknownArgs(argc,
                      argv,
                      valueArgs,
                      commonVideoTranscodeFlagArgs());
}

LocalFileTranscodeOptions parseOptions(int argc, char** argv)
{
    rejectUnknownLocalArgs(argc, argv);

    LocalFileTranscodeOptions options;
    options.inputUrl = requiredArg(argc, argv, "--input");
    options.outputUrl = requiredArg(argc, argv, "--output");
    parseCommonVideoTranscodeOptions(argc, argv, options.parameters);
    return options;
}

int runLocalVideoCli(int argc, char** argv)
{
    const bool helpRequested = hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h");
    if (argc < 5 || helpRequested) {
        std::cout << "Usage: media_transcode_local_video_cli --input in.mp4 --output out.mp4 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 [--hardware-backend auto|rkmpp] [options]\n";
        return helpRequested ? 0 : 2;
    }

    LocalFileTranscodeOptions options = parseOptions(argc, argv);
    const MediaTranscodeParameterSet& parameters = options.parameters;
    std::cout << "[CLI] input=" << redactUrlUserInfo(options.inputUrl)
              << " output=" << options.outputUrl
              << " audio="
              << (parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo
                      ? "on"
                      : "off")
              << " width=" << optionalIntText(parameters.video.width)
              << " height=" << optionalIntText(parameters.video.height)
              << " fps=" << frameRateText(parameters.video.frameRate)
              << " bitrate_kbps=" << optionalIntText(parameters.video.bitrateKbps)
              << " rc=" << mediaRateControlModeName(parameters.video.rateControl)
              << " hw="
              << (parameters.execution.disableHardware
                      ? "disabled"
                      : mediaHardwareBackendRequestName(
                            parameters.execution.hardwareBackend))
              << " diagnostics=" << (parameters.execution.diagnosticLogEnabled ? "on" : "off")
              << '\n';

    auto graphResult = LocalFileTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("local video graph build", graphResult);
    }

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(parameters.execution.diagnosticLogEnabled);
    auto compileStatus = runtime.compile(std::move(graphResult).value());
    if (!compileStatus) {
        return failStatus("compile local video graph", compileStatus);
    }
    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register local video runtime nodes", registerStatus);
    }
    auto runResult = runtime.run();
    if (!runResult) {
        return failResult("run local video graph", runResult);
    }

    const auto& result = runResult.value();
    const MediaGraphRuntimeReport report = MediaGraphRuntimeReporter::capture(runtime);
    std::cout << "[CLI] final " << report.summary() << '\n';
    std::cout << "[CLI] done: iterations=" << result.iterations
              << " total_pushed=" << result.totalPushed
              << " total_popped=" << result.totalPopped
              << " completed=" << (result.completed ? "true" : "false")
              << '\n';
    return result.completed ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return runLocalVideoCli(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[CLI] fatal exception: " << e.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "[CLI] fatal unknown exception\n";
        return 2;
    }
}
