#include "BuildWeekCli.h"

#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphDump.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace media::ffmpeg::graph::build_week {
namespace {

constexpr std::size_t kMetadataQueue = 1;
constexpr std::size_t kPacketQueue = 256;
constexpr std::size_t kFrameQueue = 128;
constexpr std::size_t kMuxQueue = 256;
constexpr int kAudioBitrateKbps = 160;
constexpr int kLivePollIntervalMs = 500;

std::string valueFor(int argc, char** argv, const std::string& key)
{
    for (int index = 2; index + 1 < argc; ++index) {
        if (std::string(argv[index]) == key) {
            return argv[index + 1];
        }
    }
    return {};
}

bool hasFlag(int argc, char** argv, const std::string& key)
{
    for (int index = 1; index < argc; ++index) {
        if (std::string(argv[index]) == key) {
            return true;
        }
    }
    return false;
}

int positiveInt(const std::string& value, const std::string& key)
{
    if (value.empty()) {
        throw std::invalid_argument("missing value for " + key);
    }
    std::size_t parsed = 0;
    const int result = std::stoi(value, &parsed, 10);
    if (parsed != value.size() || result <= 0) {
        throw std::invalid_argument(key + " must be a positive integer");
    }
    return result;
}

bool isValueOption(const std::string& key)
{
    return key == "--input" || key == "--output" || key == "--video-codec" ||
           key == "--audio-codec" || key == "--width" || key == "--height" ||
           key == "--bitrate" || key == "--duration";
}

bool isFlagOption(const std::string& key)
{
    return key == "--disable-hw" || key == "--no-audio" || key == "--quiet-graph";
}

BuildWeekCommand parseCommand(const std::string& command)
{
    if (command == "demo") return BuildWeekCommand::Demo;
    if (command == "inspect") return BuildWeekCommand::Inspect;
    if (command == "live") return BuildWeekCommand::Live;
    if (command == "help" || command == "--help" || command == "-h") return BuildWeekCommand::Help;
    throw std::invalid_argument("unsupported command: " + command);
}

MediaTranscodeParameterSet makeParameters(const BuildWeekCliOptions& options)
{
    MediaTranscodeParameterSet parameters;
    parameters.execution.includeAudio = options.includeAudio;
    parameters.execution.disableHardware = options.disableHardware;
    parameters.execution.diagnosticLogEnabled = options.diagnosticLogEnabled;
    parameters.queues.metadata = kMetadataQueue;
    parameters.queues.packet = kPacketQueue;
    parameters.queues.frame = kFrameQueue;
    parameters.queues.mux = kMuxQueue;
    parameters.video.codecName = options.videoCodec;
    parameters.video.width = options.width;
    parameters.video.height = options.height;
    parameters.video.bitrateKbps = options.videoBitrateKbps;
    parameters.audio.codecName = options.audioCodec;
    parameters.audio.bitrateKbps = kAudioBitrateKbps;
    return parameters;
}

const MediaNode* findVideoEncoder(const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoEncode) {
            return &node;
        }
    }
    return nullptr;
}

std::string optionText(const MediaNodeOptions& options,
                       const std::string& key,
                       const std::string& missing = "n/a")
{
    const std::string value = options.value(key);
    return value.empty() ? missing : value;
}

void printBanner(const char* mode)
{
    std::cout << "\n============================================================\n"
              << " MediaTranscode | OpenAI Build Week | " << mode << "\n"
              << " Intelligent DAG planning. Real FFmpeg execution.\n"
              << "============================================================\n";
}

void printRequest(const BuildWeekCliOptions& options)
{
    std::cout << "Request\n"
              << "  input          " << redactUrlUserInfo(options.input) << '\n'
              << "  output         " << options.output << '\n'
              << "  video codec    " << options.videoCodec << '\n'
              << "  audio          " << (options.includeAudio ? options.audioCodec : "disabled") << '\n'
              << "  hardware       " << (options.disableHardware ? "disabled" : "planner auto-select") << '\n'
              << "  target size    ";
    if (options.width && options.height) {
        std::cout << *options.width << 'x' << *options.height;
    } else {
        std::cout << "source";
    }
    std::cout << "\n  bitrate        " << options.videoBitrateKbps << " kbps\n\n";
}

LocalFileTranscodeOptions makeLocalOptions(const BuildWeekCliOptions& options)
{
    LocalFileTranscodeOptions request;
    request.inputUrl = options.input;
    request.outputUrl = options.output;
    request.parameters = makeParameters(options);
    return request;
}

int fail(const char* action, const ::media::ErrorInfo& error)
{
    std::cerr << "[Build Week] " << action << " failed: " << error.describe() << '\n';
    return 1;
}

int runInspect(const BuildWeekCliOptions& options)
{
    printBanner("INSPECT");
    printRequest(options);
    auto graphResult = LocalFileTranscodeGraphBuilder::build(makeLocalOptions(options));
    if (!graphResult) {
        return fail("graph planning", graphResult.error());
    }
    const MediaGraph& graph = graphResult.value();
    std::cout << formatBuildWeekGraph(graph) << '\n' << formatBuildWeekPlan(graph) << '\n';
    std::cout << "Inspect complete: no media was executed.\n";
    return 0;
}

int runDemo(const BuildWeekCliOptions& options)
{
    printBanner("DEMO");
    printRequest(options);
    auto graphResult = LocalFileTranscodeGraphBuilder::build(makeLocalOptions(options));
    if (!graphResult) {
        return fail("graph planning", graphResult.error());
    }
    MediaGraph graph = std::move(graphResult).value();
    std::cout << formatBuildWeekGraph(graph) << '\n' << formatBuildWeekPlan(graph) << '\n';

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(options.diagnosticLogEnabled);
    if (auto status = runtime.compile(std::move(graph)); !status) {
        return fail("runtime compile", status.error());
    }
    if (auto status = runtime.registerDefaultRuntimeNodes(); !status) {
        return fail("runtime node registration", status.error());
    }

    const auto startedAt = std::chrono::steady_clock::now();
    auto runResult = runtime.run();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt).count();
    if (!runResult) {
        return fail("media execution", runResult.error());
    }

    const MediaGraphRunResult& result = runResult.value();
    std::cout << "Result\n"
              << "  completed      " << (result.completed ? "yes" : "no") << '\n'
              << "  elapsed        " << elapsedMs << " ms\n"
              << "  iterations     " << result.iterations << '\n'
              << "  buffers pushed " << result.totalPushed << '\n'
              << "  buffers popped " << result.totalPopped << '\n'
              << "  output         " << options.output << '\n';
    return result.completed ? 0 : 1;
}

MediaRealtimeRtpTranscodeRequest makeLiveRequest(const BuildWeekCliOptions& options)
{
    MediaRealtimeRtpTranscodeRequest request;
    request.input.type = RealtimeInputType::Url;
    request.input.streamLayout = RealtimeInputStreamLayout::SessionDescribed;
    request.input.url = options.input;
    request.input.rtspTransport = "tcp";
    request.input.openTimeoutMs = 5000;
    request.input.readTimeoutMs = 5000;
    request.input.analyzeDurationUs = 500000;
    request.input.probeSizeBytes = 524288;
    request.input.lowLatency = true;
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = options.output;
    request.parameters = makeParameters(options);
    request.mediaId = "openai-build-week";
    return request;
}

class RuntimeStopGuard final {
public:
    explicit RuntimeStopGuard(MediaGraphRuntime& runtime) : m_runtime(runtime) {}
    ~RuntimeStopGuard()
    {
        if (m_armed) {
            (void)m_runtime.stop();
        }
    }
    void disarm() noexcept { m_armed = false; }

private:
    MediaGraphRuntime& m_runtime;
    bool m_armed = true;
};

int runLive(const BuildWeekCliOptions& options)
{
    printBanner("LIVE");
    printRequest(options);
    auto planResult = MediaRealtimeRtpTranscodePlanner::plan(makeLiveRequest(options));
    if (!planResult) {
        return fail("realtime planning", planResult.error());
    }
    MediaRealtimeRtpTranscodePlan plan = std::move(planResult).value();
    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(plan);
    if (!graphResult) {
        return fail("realtime graph build", graphResult.error());
    }
    MediaGraph graph = std::move(graphResult).value();
    std::cout << formatBuildWeekGraph(graph) << '\n' << formatBuildWeekPlan(graph) << '\n';

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(options.diagnosticLogEnabled);
    runtime.setThreadingPolicy(plan.threadingPolicy);
    if (auto status = runtime.compile(std::move(graph)); !status) {
        return fail("realtime runtime compile", status.error());
    }
    if (auto status = runtime.registerDefaultRuntimeNodes(); !status) {
        return fail("realtime node registration", status.error());
    }
    if (auto status = runtime.startThreaded(); !status) {
        return fail("realtime start", status.error());
    }
    RuntimeStopGuard stopGuard(runtime);

    std::cout << "Live output is ready for VLC: udp://@:7354\n";
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(options.durationSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kLivePollIntervalMs));
        const MediaGraphRuntimeReport report = MediaGraphRuntimeReporter::capture(runtime);
        std::cout << "  " << report.summary() << '\n';
        if (report.metrics.workerErrors > 0) {
            return 1;
        }
        if (!runtime.threadedRunning()) {
            std::cerr << "[Build Week] realtime runtime stopped unexpectedly\n";
            return 1;
        }
    }

    if (auto status = runtime.stop(); !status) {
        return fail("realtime stop", status.error());
    }
    stopGuard.disarm();
    const MediaGraphRuntimeReport finalReport = MediaGraphRuntimeReporter::capture(runtime);
    std::cout << "Final\n  " << finalReport.summary() << '\n';
    return finalReport.metrics.workerErrors == 0 ? 0 : 1;
}

} // namespace

BuildWeekCliOptions parseBuildWeekCliOptions(int argc, char** argv)
{
    if (argc < 2) {
        throw std::invalid_argument("missing command");
    }

    BuildWeekCliOptions options;
    options.command = parseCommand(argv[1]);
    if (options.command == BuildWeekCommand::Help) {
        return options;
    }

    for (int index = 2; index < argc; ++index) {
        const std::string key = argv[index];
        if (isValueOption(key)) {
            if (index + 1 >= argc || std::string(argv[index + 1]).rfind("--", 0) == 0) {
                throw std::invalid_argument("missing value for " + key);
            }
            ++index;
            continue;
        }
        if (!isFlagOption(key)) {
            throw std::invalid_argument("unsupported argument: " + key);
        }
    }

    options.input = valueFor(argc, argv, "--input");
    if (options.input.empty()) {
        throw std::invalid_argument("--input is required");
    }
    options.output = valueFor(argc, argv, "--output");
    if (options.output.empty()) {
        options.output = options.command == BuildWeekCommand::Live
            ? "udp://127.0.0.1:7354?pkt_size=1316"
            : "build-week-output.mp4";
    }

    const std::string videoCodec = valueFor(argc, argv, "--video-codec");
    const std::string audioCodec = valueFor(argc, argv, "--audio-codec");
    if (!videoCodec.empty()) options.videoCodec = videoCodec;
    if (!audioCodec.empty()) options.audioCodec = audioCodec;

    const std::string width = valueFor(argc, argv, "--width");
    const std::string height = valueFor(argc, argv, "--height");
    if (width.empty() != height.empty()) {
        throw std::invalid_argument("--width and --height must be supplied together");
    }
    if (!width.empty()) {
        options.width = positiveInt(width, "--width");
        options.height = positiveInt(height, "--height");
    }

    const std::string bitrate = valueFor(argc, argv, "--bitrate");
    const std::string duration = valueFor(argc, argv, "--duration");
    if (!bitrate.empty()) options.videoBitrateKbps = positiveInt(bitrate, "--bitrate");
    if (!duration.empty()) options.durationSeconds = positiveInt(duration, "--duration");

    options.includeAudio = !hasFlag(argc, argv, "--no-audio");
    options.disableHardware = hasFlag(argc, argv, "--disable-hw");
    options.diagnosticLogEnabled = !hasFlag(argc, argv, "--quiet-graph");
    return options;
}

std::string buildWeekUsage()
{
    return
        "MediaTranscode OpenAI Build Week demo\n\n"
        "Usage:\n"
        "  media_transcode_build_week_cli demo    --input <file> [options]\n"
        "  media_transcode_build_week_cli inspect --input <file> [options]\n"
        "  media_transcode_build_week_cli live    --input <rtsp-url> [options]\n\n"
        "Options:\n"
        "  --output <path-or-url>      demo default: build-week-output.mp4\n"
        "                              live default: udp://127.0.0.1:7354?pkt_size=1316\n"
        "  --video-codec <name>        default: h264\n"
        "  --audio-codec <name>        default: aac\n"
        "  --width <pixels> --height <pixels>\n"
        "  --bitrate <kbps>            default: 4000\n"
        "  --duration <seconds>        live default: 15\n"
        "  --disable-hw                force software planning\n"
        "  --no-audio                  disable audio branch\n"
        "  --quiet-graph               disable detailed graph diagnostics\n";
}

std::string formatBuildWeekGraph(const MediaGraph& graph)
{
    std::ostringstream output;
    output << "DAG\n"
           << "  nodes          " << graph.nodeCount() << '\n'
           << "  edges          " << graph.edgeCount() << '\n';
    for (const MediaNode& node : graph.nodes()) {
        output << "  [" << node.id.value << "] " << node.name << '\n';
    }
    return output.str();
}

std::string formatBuildWeekPlan(const MediaGraph& graph)
{
    const MediaNode* encoder = findVideoEncoder(graph);
    if (!encoder) {
        return "Planner Decision\n  video path     packet copy or disabled\n";
    }
    const MediaNodeOptions& options = encoder->options;
    std::ostringstream output;
    output << "Planner Decision\n"
           << "  chain          " << optionText(options, "pipeline.chain") << '\n'
           << "  decoder        " << optionText(options, "decoder.pipeline.ffmpeg") << '\n'
           << "  filter         " << optionText(options, "filter.pipeline.filter", "not required") << '\n'
           << "  encoder        " << optionText(options, "encoder.pipeline.ffmpeg", optionText(options, "encoder")) << '\n'
           << "  score          " << optionText(options, "pipeline.score") << '\n'
           << "  zero copy      " << (optionText(options, "pipeline.zero_copy", "0") == "1" ? "yes" : "no") << '\n'
           << "  all hardware   " << (optionText(options, "pipeline.all_hardware", "0") == "1" ? "yes" : "no") << '\n';
    return output.str();
}

int runBuildWeekCli(int argc, char** argv)
{
    const BuildWeekCliOptions options = parseBuildWeekCliOptions(argc, argv);
    switch (options.command) {
    case BuildWeekCommand::Help:
        std::cout << buildWeekUsage();
        return 0;
    case BuildWeekCommand::Inspect:
        return runInspect(options);
    case BuildWeekCommand::Demo:
        return runDemo(options);
    case BuildWeekCommand::Live:
        return runLive(options);
    }
    return 2;
}

} // namespace media::ffmpeg::graph::build_week