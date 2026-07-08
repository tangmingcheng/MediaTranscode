#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

using namespace media::ffmpeg::graph;
using namespace media::ffmpeg::graph::cli;

namespace {

struct RealtimeRtpInputCliRuntimeOptions {
    int maxDurationSeconds = 15;
    int progressTimeoutMs = 5000;
    int pollIntervalMs = 250;
};

MediaRealtimeRtpTranscodeRequest parseRawRtpOptions(int argc, char** argv)
{
    MediaRealtimeRtpTranscodeRequest options;
    options.input.kind = MediaRealtimeInputKind::RawRtp;
    options.input.videoRtp.url = requiredArg(argc, argv, "--video-rtp-url");
    options.input.videoRtp.codecName = requiredArg(argc, argv, "--video-rtp-codec");
    options.input.videoRtp.payloadType = requiredIntArg(argc, argv, "--video-rtp-payload-type");
    options.input.videoRtp.clockRate = requiredIntArg(argc, argv, "--video-rtp-clock-rate");
    options.input.videoRtp.fmtp = argValue(argc, argv, "--video-rtp-fmtp");
    options.input.openTimeoutMs = requiredIntArg(argc, argv, "--open-timeout-ms");
    options.input.readTimeoutMs = requiredIntArg(argc, argv, "--read-timeout-ms");
    options.input.analyzeDurationUs = requiredIntArg(argc, argv, "--analyze-duration-us");
    options.input.probeSizeBytes = requiredIntArg(argc, argv, "--probe-size");
    options.input.lowLatency = enabledByDefaultBoolArg(
        argc, argv, "--low-latency", "--no-low-latency", "low latency input mode");

    options.output.host = requiredArg(argc, argv, "--rtp-host");
    options.output.basePort = static_cast<std::size_t>(requiredIntArg(argc, argv, "--rtp-port"));
    options.output.sdpPath = requiredArg(argc, argv, "--sdp");
    options.output.packetSize = requiredIntArg(argc, argv, "--packet-size");

    MediaTranscodeParameterSet& parameters = options.parameters;
    parameters.execution.includeVideo = true;
    parameters.execution.includeAudio = hasArg(argc, argv, "--audio");
    parameters.execution.disableHardware = !enabledByDefaultBoolArg(
        argc, argv, "--enable-hw", "--disable-hw", "hardware planning");
    parameters.execution.diagnosticLogEnabled = enabledByDefaultBoolArg(
        argc, argv, "--graph-diagnostics", "--quiet-graph", "graph diagnostic logging");
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
    parameters.audio.codecName = argValue(argc, argv, "--audio-codec", "aac");
    parameters.audio.rateControl = rateControlArg(argc, argv, "--audio-rc");
    parameters.audio.bitrateKbps = optionalIntArg(argc, argv, "--audio-bitrate");
    parameters.audio.sampleRate = optionalIntArg(argc, argv, "--sample-rate");
    parameters.audio.channels = optionalIntArg(argc, argv, "--channels");
    if (parameters.execution.includeAudio) {
        options.input.audioRtp.url = requiredArg(argc, argv, "--audio-rtp-url");
        options.input.audioRtp.codecName = requiredArg(argc, argv, "--audio-rtp-codec");
        options.input.audioRtp.payloadType = requiredIntArg(argc, argv, "--audio-rtp-payload-type");
        options.input.audioRtp.clockRate = requiredIntArg(argc, argv, "--audio-rtp-clock-rate");
        options.input.audioRtp.channels = requiredIntArg(argc, argv, "--audio-rtp-channels");
        options.input.audioRtp.fmtp = argValue(argc, argv, "--audio-rtp-fmtp");
    }
    return options;
}

RealtimeRtpInputCliRuntimeOptions parseRuntimeOptions(int argc, char** argv)
{
    RealtimeRtpInputCliRuntimeOptions options;
    options.maxDurationSeconds = requiredIntArg(argc, argv, "--max-duration");
    if (auto progressTimeoutMs = optionalIntArg(argc, argv, "--progress-timeout-ms")) {
        options.progressTimeoutMs = *progressTimeoutMs;
    }
    if (auto pollIntervalMs = optionalIntArg(argc, argv, "--poll-interval-ms")) {
        options.pollIntervalMs = *pollIntervalMs;
    }
    if (options.maxDurationSeconds <= 0 ||
        options.progressTimeoutMs <= 0 ||
        options.pollIntervalMs <= 0) {
        throw std::invalid_argument("runtime duration, progress timeout, and poll interval must be positive");
    }
    return options;
}

::media::Status waitForRealtimeProgress(MediaGraphRuntime& runtime,
                                        const RealtimeRtpInputCliRuntimeOptions& options)
{
    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    auto lastProgressAt = startedAt;
    uint64_t lastEncodedPacketsPushed = 0;
    bool observedProgress = false;
    const auto workerStartupGrace = std::chrono::milliseconds(
        std::min(options.progressTimeoutMs, std::max(options.pollIntervalMs * 2, 1000)));

    while (runtime.threadedRunning()) {
        const MediaGraphRuntimeReport report = MediaGraphRuntimeReporter::capture(runtime);
        std::cout << "[CLI] " << report.summary() << '\n';

        if (report.metrics.workerErrors > 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::ffmpegFailure("realtime runtime reported worker errors"));
        }
        const auto now = Clock::now();
        if (report.metrics.activeWorkers == 0 &&
            now - startedAt >= workerStartupGrace) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("realtime runtime has no active workers"));
        }
        if (report.metrics.encodedPacketsPushed > lastEncodedPacketsPushed) {
            lastEncodedPacketsPushed = report.metrics.encodedPacketsPushed;
            lastProgressAt = Clock::now();
            observedProgress = true;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startedAt).count();
        const auto idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgressAt).count();
        if (observedProgress && elapsed >= options.maxDurationSeconds) {
            return ::media::Status::success();
        }
        if (idleMs >= options.progressTimeoutMs) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("realtime runtime made no progress before timeout"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(options.pollIntervalMs));
    }

    return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("realtime runtime stopped before progress condition completed"));
}

int runRealtimeRtpInputCli(int argc, char** argv)
{
    const bool helpRequested = hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h");
    if (argc < 5 || helpRequested) {
        std::cout << "Usage: media_transcode_realtime_rtp_input_cli --video-rtp-url rtp://127.0.0.1:5004 --video-rtp-codec h264 --video-rtp-payload-type 96 --video-rtp-clock-rate 90000 --video-rtp-fmtp \"packetization-mode=1;sprop-parameter-sets=...;profile-level-id=...\" --open-timeout-ms 5000 --read-timeout-ms 5000 --analyze-duration-us 500000 --probe-size 524288 --rtp-host 127.0.0.1 --rtp-port 5008 --sdp out.sdp --packet-size 1200 --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --video-codec h264 --rc auto --max-duration 15 [--disable-hw] [--quiet-graph] [--no-low-latency]\n";
        return helpRequested ? 0 : 2;
    }

    MediaRealtimeRtpTranscodeRequest options = parseRawRtpOptions(argc, argv);
    RealtimeRtpInputCliRuntimeOptions runtimeOptions = parseRuntimeOptions(argc, argv);
    std::cout << "[CLI] raw_rtp_video=" << redactUrlUserInfo(options.input.videoRtp.url)
              << " video_rtp_codec=" << options.input.videoRtp.codecName
              << " video_payload_type=" << *options.input.videoRtp.payloadType
              << " audio=" << (options.parameters.execution.includeAudio ? "on" : "off")
              << " output_sdp=" << options.output.sdpPath
              << " max_duration=" << runtimeOptions.maxDurationSeconds
              << " hw=" << (options.parameters.execution.disableHardware ? "disabled" : "auto")
              << '\n';

    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
    if (!graphResult) {
        return failResult("raw RTP graph build", graphResult);
    }
    MediaGraph graph = std::move(graphResult).value();
    auto summaryStatus = printRealtimePlanSummary(graph);
    if (!summaryStatus) {
        return failStatus("print raw RTP plan summary", summaryStatus);
    }

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(options.parameters.execution.diagnosticLogEnabled);
    auto compileStatus = runtime.compile(std::move(graph));
    if (!compileStatus) {
        return failStatus("compile raw RTP graph", compileStatus);
    }
    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register raw RTP runtime nodes", registerStatus);
    }
    auto startStatus = runtime.startThreaded();
    if (!startStatus) {
        return failStatus("start raw RTP runtime", startStatus);
    }

    auto waitStatus = waitForRealtimeProgress(runtime, runtimeOptions);
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("stop raw RTP runtime", stopStatus);
    }
    const MediaGraphRuntimeReport finalReport = MediaGraphRuntimeReporter::capture(runtime);
    std::cout << "[CLI] final " << finalReport.summary() << '\n';
    if (!waitStatus) {
        return failStatus("raw RTP runtime progress", waitStatus);
    }

    std::cout << "[CLI] raw RTP realtime validation stopped successfully\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        return runRealtimeRtpInputCli(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[CLI] fatal exception: " << e.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "[CLI] fatal unknown exception\n";
        return 2;
    }
}
