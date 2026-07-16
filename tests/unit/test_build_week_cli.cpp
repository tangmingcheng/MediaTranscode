#include "tools/build_week_cli/BuildWeekCli.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace media::ffmpeg::graph::build_week;

namespace {

BuildWeekCliOptions parse(std::initializer_list<const char*> arguments)
{
    std::vector<std::string> storage;
    storage.reserve(arguments.size());
    for (const char* argument : arguments) {
        storage.emplace_back(argument);
    }

    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (std::string& argument : storage) {
        argv.push_back(argument.data());
    }

    return parseBuildWeekCliOptions(static_cast<int>(argv.size()), argv.data());
}

void expectInvalid(std::initializer_list<const char*> arguments)
{
    bool threw = false;
    try {
        (void)parse(arguments);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testDemoDefaults()
{
    const BuildWeekCliOptions options = parse({ "build-week", "demo", "--input", "input.mp4" });
    assert(options.command == BuildWeekCommand::Demo);
    assert(options.input == "input.mp4");
    assert(options.output == "build-week-output.mp4");
    assert(options.videoCodec == "h264");
    assert(options.audioCodec == "aac");
    assert(options.videoBitrateKbps == 4000);
    assert(options.durationSeconds == 15);
    assert(options.includeAudio);
    assert(!options.disableHardware);
    assert(options.diagnosticLogEnabled);
    assert(!options.width.has_value());
    assert(!options.height.has_value());
}

void testInspectOverrides()
{
    const BuildWeekCliOptions options = parse({
        "build-week", "inspect",
        "--input", "clip.mov",
        "--output", "planned.mp4",
        "--video-codec", "hevc",
        "--audio-codec", "opus",
        "--width", "1280",
        "--height", "720",
        "--bitrate", "2500",
        "--disable-hw",
        "--no-audio",
        "--quiet-graph"
    });

    assert(options.command == BuildWeekCommand::Inspect);
    assert(options.output == "planned.mp4");
    assert(options.videoCodec == "hevc");
    assert(options.audioCodec == "opus");
    assert(options.width == 1280);
    assert(options.height == 720);
    assert(options.videoBitrateKbps == 2500);
    assert(!options.includeAudio);
    assert(options.disableHardware);
    assert(!options.diagnosticLogEnabled);
}

void testLiveDefaults()
{
    const BuildWeekCliOptions options = parse({
        "build-week", "live", "--input", "rtsp://camera/live"
    });

    assert(options.command == BuildWeekCommand::Live);
    assert(options.output == "udp://127.0.0.1:7354?pkt_size=1316");
    assert(options.durationSeconds == 15);
}

void testLiveDurationOverride()
{
    const BuildWeekCliOptions options = parse({
        "build-week", "live",
        "--input", "rtsp://camera/live",
        "--output", "udp://192.168.1.20:7354?pkt_size=1316",
        "--duration", "30"
    });

    assert(options.durationSeconds == 30);
    assert(options.output == "udp://192.168.1.20:7354?pkt_size=1316");
}

void testHelp()
{
    assert(parse({ "build-week", "--help" }).command == BuildWeekCommand::Help);
    assert(parse({ "build-week", "help" }).command == BuildWeekCommand::Help);
    const std::string usage = buildWeekUsage();
    assert(usage.find("demo") != std::string::npos);
    assert(usage.find("inspect") != std::string::npos);
    assert(usage.find("live") != std::string::npos);
}

void testInvalidArguments()
{
    expectInvalid({ "build-week", "demo" });
    expectInvalid({ "build-week", "unknown", "--input", "input.mp4" });
    expectInvalid({ "build-week", "demo", "--input", "input.mp4", "--width", "1280" });
    expectInvalid({ "build-week", "demo", "--input", "input.mp4", "--height", "720" });
    expectInvalid({ "build-week", "demo", "--input", "input.mp4", "--width", "0", "--height", "720" });
    expectInvalid({ "build-week", "demo", "--input", "input.mp4", "--bitrate", "0" });
    expectInvalid({ "build-week", "live", "--input", "rtsp://camera/live", "--duration", "0" });
    expectInvalid({ "build-week", "demo", "--input", "input.mp4", "--unexpected" });
}

} // namespace

int main()
{
    testDemoDefaults();
    testInspectOverrides();
    testLiveDefaults();
    testLiveDurationOverride();
    testHelp();
    testInvalidArguments();
    std::cout << "build week CLI tests passed\n";
    return 0;
}
