#pragma once

#include <optional>
#include <string>

namespace media::ffmpeg::graph {
class MediaGraph;
}

namespace media::ffmpeg::graph::build_week {

enum class BuildWeekCommand {
    Demo,
    Inspect,
    Live,
    Help
};

struct BuildWeekCliOptions {
    BuildWeekCommand command = BuildWeekCommand::Help;
    std::string input;
    std::string output;
    std::string videoCodec = "h264";
    std::string audioCodec = "aac";
    std::optional<int> width;
    std::optional<int> height;
    int videoBitrateKbps = 4000;
    int durationSeconds = 15;
    bool includeAudio = true;
    bool disableHardware = false;
    bool diagnosticLogEnabled = true;
};

BuildWeekCliOptions parseBuildWeekCliOptions(int argc, char** argv);
std::string buildWeekUsage();
std::string formatBuildWeekGraph(const MediaGraph& graph);
std::string formatBuildWeekPlan(const MediaGraph& graph);
int runBuildWeekCli(int argc, char** argv);

} // namespace media::ffmpeg::graph::build_week