#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
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

void rejectRemovedArg(int argc, char** argv, const std::string& key)
{
    if (hasArg(argc, argv, key)) {
        throw std::invalid_argument(key + " was removed; encoder selection is planner-owned");
    }
}

std::optional<int> optionalIntArg(int argc, char** argv, const std::string& key)
{
    const std::string value = argValue(argc, argv, key);
    if (value.empty()) {
        return std::nullopt;
    }

    std::size_t parsed = 0;
    const int result = std::stoi(value, &parsed, 10);
    if (parsed != value.size()) {
        throw std::invalid_argument("invalid integer value for " + key + ": " + value);
    }
    return result;
}

std::size_t positiveSizeArg(int argc, char** argv, const std::string& key, std::size_t fallback)
{
    const auto value = optionalIntArg(argc, argv, key);
    if (!value) {
        return fallback;
    }
    if (*value <= 0) {
        throw std::invalid_argument(key + " must be greater than 0");
    }
    return static_cast<std::size_t>(*value);
}

MediaRateControlMode rateControlArg(int argc, char** argv, const std::string& key)
{
    MediaRateControlMode mode = MediaRateControlMode::Auto;
    const std::string value = argValue(argc, argv, key);
    if (!parseMediaRateControlMode(value, mode)) {
        throw std::invalid_argument("unsupported rate control mode for " + key + ": " + value);
    }
    return mode;
}

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

void printUsage()
{
    std::cout << "Usage: media_transcode_graph_transcode_cli --input in.mp4 --output out.mp4 [options]\n";
    std::cout << "       media_transcode_graph_transcode_cli --realtime-rtp --input rtp://host:port --output rtp://host:port --dry-run [options]\n";
    std::cout << "       encoder selection is automatic and planner-owned\n";
    std::cout << "       add --quiet-graph to disable runtime graph diagnostics\n";
    std::cout << "       realtime RTP options: --sdp <path> --media-id <id> --queue-capacity <n>\n";
    std::cout << "                             --high-watermark <n> --critical-watermark <n> --no-packet-fanout --ingest-to-mux\n";
}

LocalFileTranscodeOptions parseOptions(int argc, char** argv)
{
    rejectRemovedArg(argc, argv, "--encoder");
    rejectRemovedArg(argc, argv, "--audio-encoder");
    rejectRemovedArg(argc, argv, "--audio-transcode");

    LocalFileTranscodeOptions options;
    options.inputUrl = argValue(argc, argv, "--input");
    options.outputUrl = argValue(argc, argv, "--output");
    options.outputFormat = argValue(argc, argv, "--format");

    MediaTranscodeParameterSet& parameters = options.parameters;
    parameters.execution.includeVideo = !hasArg(argc, argv, "--no-video");
    parameters.execution.includeAudio = !hasArg(argc, argv, "--no-audio");
    parameters.execution.disableHardware = hasArg(argc, argv, "--disable-hw");
    parameters.execution.diagnosticLogEnabled = !hasArg(argc, argv, "--quiet-graph");

    parameters.video.codecName = argValue(argc, argv, "--video-codec", parameters.video.codecName);
    parameters.video.rateControl = rateControlArg(argc, argv, "--rc");
    parameters.video.preset = argValue(argc, argv, "--preset", parameters.video.preset);
    parameters.video.profile = argValue(argc, argv, "--profile", parameters.video.profile);
    parameters.video.tune = argValue(argc, argv, "--tune", parameters.video.tune);
    parameters.video.level = argValue(argc, argv, "--level", parameters.video.level);
    parameters.video.width = optionalIntArg(argc, argv, "--width");
    parameters.video.height = optionalIntArg(argc, argv, "--height");
    if (auto fps = optionalIntArg(argc, argv, "--fps")) {
        parameters.video.frameRate.numerator = fps;
        parameters.video.frameRate.denominator = 1;
    }
    parameters.video.bitrateKbps = optionalIntArg(argc, argv, "--bitrate");
    parameters.video.minBitrateKbps = optionalIntArg(argc, argv, "--min-bitrate");
    parameters.video.maxBitrateKbps = optionalIntArg(argc, argv, "--max-bitrate");
    parameters.video.bufferSizeKbits = optionalIntArg(argc, argv, "--buffer-size");
    parameters.video.quality = optionalIntArg(argc, argv, "--quality");
    parameters.video.gop = optionalIntArg(argc, argv, "--gop");
    parameters.video.bFrames = optionalIntArg(argc, argv, "--bframes");

    parameters.audio.codecName = argValue(argc, argv, "--audio-codec", parameters.audio.codecName);
    parameters.audio.rateControl = rateControlArg(argc, argv, "--audio-rc");
    parameters.audio.bitrateKbps = optionalIntArg(argc, argv, "--audio-bitrate");
    parameters.audio.minBitrateKbps = optionalIntArg(argc, argv, "--audio-min-bitrate");
    parameters.audio.maxBitrateKbps = optionalIntArg(argc, argv, "--audio-max-bitrate");
    parameters.audio.bufferSizeKbits = optionalIntArg(argc, argv, "--audio-buffer-size");
    parameters.audio.sampleRate = optionalIntArg(argc, argv, "--sample-rate");
    parameters.audio.channels = optionalIntArg(argc, argv, "--channels");
    parameters.audio.quality = optionalIntArg(argc, argv, "--audio-quality");
    parameters.audio.preset = argValue(argc, argv, "--audio-preset", parameters.audio.preset);
    parameters.audio.profile = argValue(argc, argv, "--audio-profile", parameters.audio.profile);
    return options;
}

MediaRealtimeGraphBuilderOptions parseRealtimeRtpOptions(int argc, char** argv)
{
    MediaRealtimeGraphBuilderOptions options;
    options.kind = hasArg(argc, argv, "--ingest-to-mux")
        ? MediaRealtimeGraphKind::IngestToMux
        : MediaRealtimeGraphKind::PacketRelay;
    options.inputUrl = argValue(argc, argv, "--input");
    options.outputUrl = argValue(argc, argv, "--output");
    options.sdpPath = argValue(argc, argv, "--sdp");
    options.mediaId = argValue(argc, argv, "--media-id");
    options.enablePacketFanout = !hasArg(argc, argv, "--no-packet-fanout");
    options.enableRtpMux = options.kind == MediaRealtimeGraphKind::IngestToMux;
    options.enableSdpWriter = options.kind == MediaRealtimeGraphKind::PacketRelay && !options.sdpPath.empty();
    options.queueCapacity = positiveSizeArg(argc, argv, "--queue-capacity", options.queueCapacity);
    options.highWatermark = positiveSizeArg(argc, argv, "--high-watermark", options.highWatermark);
    options.criticalWatermark = positiveSizeArg(argc, argv, "--critical-watermark", options.criticalWatermark);

    if (options.inputUrl.empty()) {
        throw std::invalid_argument("--realtime-rtp requires --input");
    }
    if (options.outputUrl.empty()) {
        throw std::invalid_argument("--realtime-rtp requires --output");
    }
    return options;
}

const char* realtimeGraphKindName(MediaRealtimeGraphKind kind) noexcept
{
    switch (kind) {
    case MediaRealtimeGraphKind::PacketRelay:
        return "packet-relay";
    case MediaRealtimeGraphKind::IngestToMux:
        return "ingest-to-mux";
    }
    return "unknown";
}

int runRealtimeRtpCli(int argc, char** argv)
{
    if (!hasArg(argc, argv, "--dry-run")) {
        std::cerr << "[CLI] --realtime-rtp currently supports graph dry-run only; add --dry-run\n";
        return 2;
    }

    const MediaRealtimeGraphBuilderOptions options = parseRealtimeRtpOptions(argc, argv);
    std::cout << "[CLI] realtime rtp input=" << options.inputUrl
              << " output=" << options.outputUrl
              << " kind=" << realtimeGraphKindName(options.kind)
              << " packet_fanout=" << (options.enablePacketFanout ? "on" : "off")
              << " rtp_mux=" << (options.enableRtpMux ? "on" : "off")
              << " sdp_writer=" << (options.enableSdpWriter ? "on" : "off")
              << " queue_capacity=" << options.queueCapacity
              << '\n';

    std::cout << "[CLI] realtime rtp graph build begin\n";
    auto graphResult = MediaRealtimeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("realtime rtp graph build", graphResult);
    }

    const MediaGraph& graph = graphResult.value().graph;
    std::cout << "[CLI] realtime rtp graph build done: nodes=" << graph.nodeCount()
              << " edges=" << graph.edgeCount() << '\n';
    return 0;
}

int runGraphTranscodeCli(int argc, char** argv)
{
    if (argc < 5 || hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h")) {
        printUsage();
        return argc < 5 ? 2 : 0;
    }

    if (hasArg(argc, argv, "--realtime-rtp")) {
        return runRealtimeRtpCli(argc, argv);
    }

    LocalFileTranscodeOptions options = parseOptions(argc, argv);
    const MediaTranscodeParameterSet& parameters = options.parameters;
    std::cout << "[CLI] input=" << options.inputUrl
              << " output=" << options.outputUrl
              << " video=" << (parameters.execution.includeVideo ? "on" : "off")
              << " audio=" << (parameters.execution.includeAudio ? "on" : "off")
              << " width=" << optionalIntText(parameters.video.width)
              << " height=" << optionalIntText(parameters.video.height)
              << " fps=" << frameRateText(parameters.video.frameRate)
              << " bitrate_kbps=" << optionalIntText(parameters.video.bitrateKbps)
              << " min_bitrate_kbps=" << optionalIntText(parameters.video.minBitrateKbps)
              << " max_bitrate_kbps=" << optionalIntText(parameters.video.maxBitrateKbps)
              << " buffer_size_kbits=" << optionalIntText(parameters.video.bufferSizeKbits)
              << " rc=" << mediaRateControlModeName(parameters.video.rateControl)
              << " quality=" << optionalIntText(parameters.video.quality)
              << " diagnostics=" << (parameters.execution.diagnosticLogEnabled ? "on" : "off")
              << '\n';

    std::cout << "[CLI] graph build begin\n";
    auto graphResult = LocalFileTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("graph build", graphResult);
    }
    MediaGraph graph = std::move(graphResult).value();
    std::cout << "[CLI] graph build done: nodes=" << graph.nodeCount()
              << " edges=" << graph.edgeCount() << '\n';

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(parameters.execution.diagnosticLogEnabled);

    std::cout << "[CLI] compile begin\n";
    auto compileStatus = runtime.compile(std::move(graph));
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }
    std::cout << "[CLI] compile done\n";

    std::cout << "[CLI] register runtime nodes begin\n";
    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }
    std::cout << "[CLI] register runtime nodes done\n";

    std::cout << "[CLI] run begin\n";
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
    return result.completed ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return runGraphTranscodeCli(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[CLI] fatal exception: " << e.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "[CLI] fatal unknown exception\n";
        return 2;
    }
}
