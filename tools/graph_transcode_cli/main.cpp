#include "internal/graph/preset/MediaPipelinePreset.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <iostream>
#include <string>
#include <unordered_map>

using namespace media::ffmpeg::graph;

static std::string getArg(int argc, char** argv, const std::string& key, const std::string& def = "")
{
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }
    return def;
}

static bool hasArg(int argc, char** argv, const std::string& key)
{
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == key) return true;
    }
    return false;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "Usage:\n"
                     "  --input xxx\n"
                     "  --output xxx\n"
                     "  --width xxx\n"
                     "  --height xxx\n"
                     "  --fps xxx\n"
                     "  --bitrate xxx\n"
                     "  --rc cbr/vbr/crf\n"
                     "  --no-audio\n";
        return -1;
    }

    MediaPipelinePresetOptions opt;
    opt.inputUrl = getArg(argc, argv, "--input");
    opt.outputUrl = getArg(argc, argv, "--output");

    opt.includeAudio = !hasArg(argc, argv, "--no-audio");
    opt.includeVideo = true;

    auto graphRes = MediaPipelinePreset::create(MediaPipelinePresetKind::LocalFileTranscodeSkeleton, opt);

    if (!graphRes) {
        std::cout << "[CLI] graph build failed\n";
        return -2;
    }

    MediaGraphRuntime runtime;

    auto c = runtime.compile(std::move(graphRes.value()));
    if (!c) {
        std::cout << "[CLI] compile failed\n";
        return -3;
    }

    runtime.registerDefaultRuntimeNodes();
    runtime.start();

    auto r = runtime.runUntilIdle({});

    std::cout << "[CLI] done\n";
    return 0;
}
