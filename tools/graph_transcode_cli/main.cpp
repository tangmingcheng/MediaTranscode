#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <iostream>
#include <string>
#include <utility>

using namespace media::ffmpeg::graph;

namespace {

std::string argValue(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == key) {
            return argv[i + 1];
        }
    }
    return {};
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

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5 || hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        std::cout << "Usage: media_transcode_graph_transcode_cli --input in.mp4 --output out.mp4\n";
        return argc < 5 ? 2 : 0;
    }

    LocalFileTranscodeOptions options;
    options.inputUrl = argValue(argc, argv, "--input");
    options.outputUrl = argValue(argc, argv, "--output");

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

    auto runResult = runtime.runUntilIdle();
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
              << '\n';
    return 0;
}
