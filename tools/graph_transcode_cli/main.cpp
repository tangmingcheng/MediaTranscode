#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace media::ffmpeg::graph;
using namespace media::ffmpeg::graph::cli;

namespace {

std::optional<MediaRealtimeInputKind> realtimeInputKindArg(int argc, char** argv)
{
    const std::string value = argValue(argc, argv, "--input-kind");
    if (value.empty()) {
        return std::nullopt;
    }
    if (value == "url") {
        return MediaRealtimeInputKind::RealtimeUrl;
    }
    if (value == "raw-rtp") {
        return MediaRealtimeInputKind::RawRtp;
    }
    throw std::invalid_argument("unsupported --input-kind: " + value);
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

LocalFileTranscodeOptions parseOptions(int argc, char** argv)
{
    rejectRemovedArg(argc, argv, "--encoder");
    rejectRemovedArg(argc, argv, "--audio-encoder");
    rejectRemovedArg(argc, argv, "--audio-transcode");
    rejectRemovedArg(argc, argv, "--bframes");

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

MediaRealtimeRtpTranscodeRequest parseRealtimeOptions(int argc, char** argv)
{
    MediaRealtimeRtpTranscodeRequest options;
    options.input.kind = realtimeInputKindArg(argc, argv);
    if (options.input.kind && *options.input.kind == MediaRealtimeInputKind::RawRtp) {
        options.input.videoRtp.url = requiredArg(argc, argv, "--video-rtp-url");
        options.input.videoRtp.codecName = requiredArg(argc, argv, "--video-rtp-codec");
        options.input.videoRtp.payloadType = requiredIntArg(argc, argv, "--video-rtp-payload-type");
        options.input.videoRtp.clockRate = requiredIntArg(argc, argv, "--video-rtp-clock-rate");
    } else {
        options.input.url = requiredArg(argc, argv, "--input");
    }
    options.input.rtspTransport = argValue(argc, argv, "--rtsp-transport");
    options.input.openTimeoutMs = optionalIntArg(argc, argv, "--open-timeout-ms");
    options.input.readTimeoutMs = optionalIntArg(argc, argv, "--read-timeout-ms");
    options.input.analyzeDurationUs = optionalIntArg(argc, argv, "--analyze-duration-us");
    options.input.probeSizeBytes = optionalIntArg(argc, argv, "--probe-size");
    if (hasArg(argc, argv, "--low-latency") || hasArg(argc, argv, "--no-low-latency")) {
        options.input.lowLatency = requiredExclusiveBoolArg(argc, argv, "--low-latency", "--no-low-latency");
    }
    options.output.host = argValue(argc, argv, "--rtp-host");
    if (auto port = optionalIntArg(argc, argv, "--rtp-port")) {
        options.output.basePort = static_cast<std::size_t>(*port);
    }
    options.output.sdpPath = argValue(argc, argv, "--sdp");
    options.output.packetSize = optionalIntArg(argc, argv, "--packet-size");
    options.output.url = argValue(argc, argv, "--output");

    MediaTranscodeParameterSet& parameters = options.parameters;
    parameters.execution.includeVideo = requiredExclusiveBoolArg(argc, argv, "--video", "--no-video");
    parameters.execution.includeAudio = requiredExclusiveBoolArg(argc, argv, "--audio", "--no-audio");
    parameters.execution.disableHardware = !requiredExclusiveBoolArg(argc, argv, "--enable-hw", "--disable-hw");
    parameters.execution.diagnosticLogEnabled = requiredExclusiveBoolArg(argc, argv, "--graph-diagnostics", "--quiet-graph");
    parameters.queues.metadata = requiredSizeArg(argc, argv, "--metadata-queue");
    parameters.queues.packet = requiredSizeArg(argc, argv, "--packet-queue");
    parameters.queues.frame = requiredSizeArg(argc, argv, "--frame-queue");
    parameters.queues.mux = requiredSizeArg(argc, argv, "--mux-queue");
    parameters.video.codecName = requiredArg(argc, argv, "--video-codec");
    parameters.video.rateControl = requiredRateControlArg(argc, argv, "--rc");
    parameters.video.preset = argValue(argc, argv, "--preset");
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
    if (parameters.execution.includeAudio && options.input.kind && *options.input.kind == MediaRealtimeInputKind::RawRtp) {
        options.input.audioRtp.url = requiredArg(argc, argv, "--audio-rtp-url");
        options.input.audioRtp.codecName = requiredArg(argc, argv, "--audio-rtp-codec");
        options.input.audioRtp.payloadType = requiredIntArg(argc, argv, "--audio-rtp-payload-type");
        options.input.audioRtp.clockRate = requiredIntArg(argc, argv, "--audio-rtp-clock-rate");
        options.input.audioRtp.channels = requiredIntArg(argc, argv, "--audio-rtp-channels");
        options.input.audioRtp.fmtp = argValue(argc, argv, "--audio-rtp-fmtp");
    }
    return options;
}


int runGraphTranscodeCli(int argc, char** argv)
{
    const std::string mode = argValue(argc, argv, "--mode", "local-file");
    const bool helpRequested = hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h");
    if (argc < 5 || helpRequested) {
        std::cout << "Usage: media_transcode_graph_transcode_cli --input in.mp4 --output out.mp4 [options]\n";
        std::cout << "       media_transcode_graph_transcode_cli --mode realtime-rtp --input-kind url --input rtsp://... --rtsp-transport tcp --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --low-latency --rtp-host 127.0.0.1 --rtp-port 5004 --sdp out.sdp --packet-size 1200 --video --audio --enable-hw --graph-diagnostics --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --audio-codec aac --duration 15\n";
        std::cout << "       raw RTP mode uses --video-rtp-url/--video-rtp-codec/--video-rtp-payload-type/--video-rtp-clock-rate and optional --audio-rtp-* when --audio is enabled\n";
        std::cout << "       encoder selection is automatic and planner-owned\n";
        std::cout << "       realtime mode requires exactly one of --graph-diagnostics or --quiet-graph\n";
        return helpRequested ? 0 : 2;
    }

    if (mode == "realtime-rtp") {
        MediaRealtimeRtpTranscodeRequest options = parseRealtimeOptions(argc, argv);
        const int durationSeconds = requiredIntArg(argc, argv, "--duration");
        std::cout << "[CLI] realtime input=" << redactUrlUserInfo(options.input.url)
                  << " sdp=" << options.output.sdpPath
                  << " duration=" << durationSeconds
                  << " hw=" << (options.parameters.execution.disableHardware ? "disabled" : "auto")
                  << '\n';

        auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
        if (!graphResult) {
            return failResult("realtime graph build", graphResult);
        }
        MediaGraph graph = std::move(graphResult).value();
        auto summaryStatus = printRealtimePlanSummary(graph);
        if (!summaryStatus) {
            return failStatus("print realtime plan summary", summaryStatus);
        }

        MediaGraphRuntime runtime;
        runtime.setDiagnosticsEnabled(options.parameters.execution.diagnosticLogEnabled);
        auto compileStatus = runtime.compile(std::move(graph));
        if (!compileStatus) {
            return failStatus("compile realtime graph", compileStatus);
        }
        auto registerStatus = runtime.registerDefaultRuntimeNodes();
        if (!registerStatus) {
            return failStatus("register realtime runtime nodes", registerStatus);
        }
        auto startStatus = runtime.startThreaded();
        if (!startStatus) {
            return failStatus("start realtime runtime", startStatus);
        }
        if (durationSeconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
        }
        auto stopStatus = runtime.stop();
        if (!stopStatus) {
            return failStatus("stop realtime runtime", stopStatus);
        }
        std::cout << "[CLI] realtime stopped\n";
        return 0;
    }

    if (mode != "local-file") {
        throw std::invalid_argument("unsupported --mode: " + mode);
    }

    LocalFileTranscodeOptions options = parseOptions(argc, argv);
    const MediaTranscodeParameterSet& parameters = options.parameters;
    std::cout << "[CLI] input=" << redactUrlUserInfo(options.inputUrl)
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
