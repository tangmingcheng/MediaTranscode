#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace media::ffmpeg::graph;

int fail(const std::string& message)
{
    std::cerr << "graph planner probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
}

std::size_t parseTopCount(const char* text, std::size_t fallback)
{
    if (!text) {
        return fallback;
    }

    const long value = std::strtol(text, nullptr, 10);
    return value > 0 ? static_cast<std::size_t>(value) : fallback;
}

std::string stageText(const MediaPipelineStagePlan& stage)
{
    std::string name = !stage.ffmpegName.empty() ? stage.ffmpegName : stage.filterName;
    if (name.empty()) {
        name = stage.componentName;
    }
    return name + "@" + mediaHardwareDeviceKindName(stage.deviceKind) +
           (stage.available ? ":available" : ":unavailable") +
           "(" + stage.availabilityReason + ")";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_planner_probe.exe <input-media-file> [output-media-file] [output-codec] [top-candidates]\n";
        return 2;
    }

    MediaPipelinePlannerOptions options;
    const std::string inputPath = argv[1];
    options.outputPath = argc >= 3 ? argv[2] : "planned-output.mp4";
    options.outputCodecName = argc >= 4 ? argv[3] : "h264";
    const std::size_t topCandidates = argc >= 5 ? parseTopCount(argv[4], 5) : 5;

    auto buildResult = MediaPipelinePlanner::buildPlannedVideoFileTranscodeGraph(inputPath, options);
    if (!buildResult) {
        return fail("build planned graph: " + buildResult.error().describe());
    }

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(buildResult.value().graph));
    if (!compileStatus) {
        return failStatus("compile planned graph", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    const MediaPipelinePlan& plan = buildResult.value().plan;
    const MediaPipelineChainPlan& selected = plan.selected;

    std::cout << "graph planner probe ok: "
              << "input_codec=" << plan.inputCodecName
              << ", output_codec=" << plan.outputCodecName
              << ", selected_chain=" << selected.label
              << ", selected_score=" << selected.score
              << ", selected_available=" << (selected.available ? "true" : "false")
              << ", all_hardware=" << (selected.allHardware ? "true" : "false")
              << ", same_hardware_device=" << (selected.sameHardwareDevice ? "true" : "false")
              << ", zero_copy=" << (selected.zeroCopy ? "true" : "false")
              << ", decoder=" << stageText(selected.decoder)
              << ", filter=" << stageText(selected.filter)
              << ", encoder=" << stageText(selected.encoder)
              << ", graph_nodes=" << runtime.graph()->nodeCount()
              << ", graph_edges=" << runtime.graph()->edgeCount()
              << '\n';

    const std::size_t count = std::min(topCandidates, plan.candidates.size());
    for (std::size_t i = 0; i < count; ++i) {
        const MediaPipelineChainPlan& candidate = plan.candidates[i];
        std::cout << "candidate[" << i << "]: "
                  << "chain=" << candidate.label
                  << ", score=" << candidate.score
                  << ", available=" << (candidate.available ? "true" : "false")
                  << ", all_hardware=" << (candidate.allHardware ? "true" : "false")
                  << ", zero_copy=" << (candidate.zeroCopy ? "true" : "false")
                  << ", decoder=" << stageText(candidate.decoder)
                  << ", filter=" << stageText(candidate.filter)
                  << ", encoder=" << stageText(candidate.encoder)
                  << ", reason=" << candidate.reason
                  << '\n';
    }

    return 0;
}
