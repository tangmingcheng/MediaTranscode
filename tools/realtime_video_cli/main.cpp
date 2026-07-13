#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"
#include "../common/VideoCliTranscodeOptions.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

using namespace media::ffmpeg::graph;
using namespace media::ffmpeg::graph::cli;

namespace {

struct RealtimeVideoRuntimeOptions {
    int maxDurationSeconds = 15;
    int progressTimeoutMs = 5000;
    int pollIntervalMs = 250;
};

RealtimeInputType requiredRealtimeInputType(int argc, char** argv)
{
    const std::string value = requiredArg(argc, argv, "--input-type");
    if (value == "rtsp") {
        return RealtimeInputType::Url;
    }
    if (value == "rtp") {
        return RealtimeInputType::RtpPort;
    }
    if (value == "mpegts-udp") {
        return RealtimeInputType::MpegTsUdp;
    }
    throw std::invalid_argument("unsupported --input-type: " + value);
}

RealtimeInputStreamLayout requiredRealtimeInputLayout(int argc, char** argv)
{
    const std::string value = requiredArg(argc, argv, "--input-layout");
    if (value == "session") {
        return RealtimeInputStreamLayout::SessionDescribed;
    }
    if (value == "separate") {
        return RealtimeInputStreamLayout::SeparateStreams;
    }
    if (value == "mpegts") {
        return RealtimeInputStreamLayout::MuxedTransportStream;
    }
    throw std::invalid_argument("unsupported --input-layout: " + value);
}

RealtimeOutputStreamLayout requiredRealtimeOutputLayout(int argc, char** argv)
{
    const std::string value = requiredArg(argc, argv, "--output-layout");
    if (value == "separate") {
        return RealtimeOutputStreamLayout::SeparateStreams;
    }
    if (value == "mpegts") {
        return RealtimeOutputStreamLayout::MuxedTransportStream;
    }
    throw std::invalid_argument("unsupported --output-layout: " + value);
}

void rejectUnknownRealtimeArgs(int argc, char** argv)
{
    std::vector<std::string> valueArgs = commonVideoTranscodeValueArgs();
    const std::vector<std::string> realtimeValueArgs {
        "--input-type",
        "--input-layout",
        "--output-layout",
        "--input",
        "--rtsp-transport",
        "--open-timeout-ms",
        "--read-timeout-ms",
        "--analyze-duration-us",
        "--probe-size",
        "--video-rtp-url",
        "--video-rtp-codec",
        "--video-rtp-payload-type",
        "--video-rtp-clock-rate",
        "--video-rtp-fmtp",
        "--audio-rtp-url",
        "--audio-rtp-codec",
        "--audio-rtp-payload-type",
        "--audio-rtp-clock-rate",
        "--audio-rtp-channels",
        "--audio-rtp-fmtp",
        "--rtp-host",
        "--rtp-port",
        "--sdp",
        "--packet-size",
        "--output",
        "--max-duration",
        "--progress-timeout-ms",
        "--poll-interval-ms",
        "--startup-max-video-unit-bytes",
        "--startup-max-audio-unit-bytes",
        "--startup-max-gap-ms",
    };
    valueArgs.insert(valueArgs.end(), realtimeValueArgs.begin(), realtimeValueArgs.end());

    std::vector<std::string> flagArgs = commonVideoTranscodeFlagArgs();
    flagArgs.push_back("--no-low-latency");
    rejectUnknownArgs(argc, argv, valueArgs, flagArgs);
}

void parseRealtimeInputOptions(int argc, char** argv, MediaRealtimeInputConfig& input)
{
    input.type = requiredRealtimeInputType(argc, argv);
    input.streamLayout = requiredRealtimeInputLayout(argc, argv);
    input.openTimeoutMs = requiredIntArg(argc, argv, "--open-timeout-ms");
    input.readTimeoutMs = requiredIntArg(argc, argv, "--read-timeout-ms");
    input.analyzeDurationUs = requiredIntArg(argc, argv, "--analyze-duration-us");
    input.probeSizeBytes = requiredIntArg(argc, argv, "--probe-size");
    input.lowLatency = !hasArg(argc, argv, "--no-low-latency");

    if (*input.type == RealtimeInputType::RtpPort) {
        input.videoRtp.url = requiredArg(argc, argv, "--video-rtp-url");
        input.videoRtp.codecName = requiredArg(argc, argv, "--video-rtp-codec");
        input.videoRtp.payloadType = requiredIntArg(argc, argv, "--video-rtp-payload-type");
        input.videoRtp.clockRate = requiredIntArg(argc, argv, "--video-rtp-clock-rate");
        input.videoRtp.fmtp = argValue(argc, argv, "--video-rtp-fmtp");
        return;
    }

    input.url = requiredArg(argc, argv, "--input");
    if (*input.type == RealtimeInputType::Url) {
        input.rtspTransport = requiredArg(argc, argv, "--rtsp-transport");
    }
}

void parseRealtimeOutputOptions(int argc, char** argv, MediaRealtimeOutputConfig& output)
{
    output.streamLayout = requiredRealtimeOutputLayout(argc, argv);
    if (*output.streamLayout == RealtimeOutputStreamLayout::SeparateStreams) {
        output.host = requiredArg(argc, argv, "--rtp-host");
        output.basePort = static_cast<std::size_t>(requiredIntArg(argc, argv, "--rtp-port"));
        output.sdpPath = requiredArg(argc, argv, "--sdp");
        output.packetSize = requiredIntArg(argc, argv, "--packet-size");
        return;
    }

    output.url = requiredArg(argc, argv, "--output");
}

void parseAudioRtpOptionsIfNeeded(int argc, char** argv, MediaRealtimeRtpTranscodeRequest& options)
{
    if (!options.parameters.execution.includeAudio ||
        !options.input.type ||
        *options.input.type != RealtimeInputType::RtpPort) {
        return;
    }

    options.input.audioRtp.url = requiredArg(argc, argv, "--audio-rtp-url");
    options.input.audioRtp.codecName = requiredArg(argc, argv, "--audio-rtp-codec");
    options.input.audioRtp.payloadType = requiredIntArg(argc, argv, "--audio-rtp-payload-type");
    options.input.audioRtp.clockRate = requiredIntArg(argc, argv, "--audio-rtp-clock-rate");
    options.input.audioRtp.channels = requiredIntArg(argc, argv, "--audio-rtp-channels");
    options.input.audioRtp.fmtp = argValue(argc, argv, "--audio-rtp-fmtp");
}

MediaRealtimeRtpTranscodeRequest parseRealtimeOptions(int argc, char** argv)
{
    rejectUnknownRealtimeArgs(argc, argv);

    MediaRealtimeRtpTranscodeRequest options;
    parseRealtimeInputOptions(argc, argv, options.input);
    parseRealtimeOutputOptions(argc, argv, options.output);
    parseCommonVideoTranscodeOptions(argc, argv, options.parameters);
    parseAudioRtpOptionsIfNeeded(argc, argv, options);
    if (options.parameters.execution.includeAudio) {
        options.avSyncStartup.maximumVideoUnitBytes = requiredSizeArg(
            argc, argv, "--startup-max-video-unit-bytes");
        options.avSyncStartup.maximumAudioUnitBytes = requiredSizeArg(
            argc, argv, "--startup-max-audio-unit-bytes");
        const int maximumGapMs = requiredIntArg(argc, argv, "--startup-max-gap-ms");
        if (maximumGapMs <= 0) {
            throw std::invalid_argument("startup maximum gap must be positive");
        }
        options.avSyncStartup.maximumGap = MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(maximumGapMs) * 1'000'000);
    }
    return options;
}

RealtimeVideoRuntimeOptions parseRuntimeOptions(int argc, char** argv)
{
    RealtimeVideoRuntimeOptions options;
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
                                        const RealtimeVideoRuntimeOptions& options)
{
    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    auto lastProgressAt = startedAt;
    uint64_t lastEncodedPacketsPushed = 0;
    bool observedProgress = false;
    const auto workerStartupGrace = std::chrono::milliseconds(
        std::min(options.progressTimeoutMs, std::max(options.pollIntervalMs * 2, 1000)));

    while (true) {
        auto lifecycleStatus = runtime.synchronizeThreadedState();
        if (!lifecycleStatus) {
            return lifecycleStatus;
        }
        if (!runtime.threadedRunning()) {
            break;
        }
        const MediaGraphRuntimeReport progressReport = MediaGraphRuntimeReporter::capture(runtime);
        auto sampleStatus = runtime.acceptanceCollector().sample(
            progressReport.metrics.encodedPacketsPushed);
        if (!sampleStatus) {
            return sampleStatus;
        }
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

int runRealtimeVideoCli(int argc, char** argv)
{
    std::cout << std::unitbuf;

    const bool helpRequested = hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h");
    if (argc < 5 || helpRequested) {
        std::cout << "Usage: media_transcode_realtime_video_cli --input-type rtsp|rtp|mpegts-udp --input-layout session|separate|mpegts --output-layout separate|mpegts --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --max-duration 15 [options]\n";
        return helpRequested ? 0 : 2;
    }

    MediaRealtimeRtpTranscodeRequest options = parseRealtimeOptions(argc, argv);
    RealtimeVideoRuntimeOptions runtimeOptions = parseRuntimeOptions(argc, argv);
    std::cout << "[CLI] input_type=" << static_cast<int>(*options.input.type)
              << " input=" << redactUrlUserInfo(options.input.url.empty() ? options.input.videoRtp.url : options.input.url)
              << " audio=" << (options.parameters.execution.includeAudio ? "on" : "off")
              << " max_duration=" << runtimeOptions.maxDurationSeconds
              << " hw=" << (options.parameters.execution.disableHardware ? "disabled" : "auto")
              << '\n';

    auto preflightResult = MediaRealtimeRtpTranscodePlanner::preflight(options);
    if (!preflightResult) {
        return failResult("realtime video graph preflight", preflightResult);
    }
    MediaRealtimeTranscodePreflight preflight = std::move(preflightResult).value();
    const MediaThreadingPolicy threadingPolicy = preflight.plan.threadingPolicy;

    auto executableResult = MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(std::move(preflight));
    if (!executableResult) {
        return failResult("realtime video executable graph build", executableResult);
    }
    MediaRealtimeExecutableGraph executable = std::move(executableResult).value();
    auto summaryStatus = printRealtimePlanSummary(executable.graph);
    if (!summaryStatus) {
        return failStatus("print realtime video plan summary", summaryStatus);
    }

    MediaGraphRuntime runtime;
    runtime.setDiagnosticsEnabled(options.parameters.execution.diagnosticLogEnabled);
    runtime.setThreadingPolicy(threadingPolicy);
    auto compileStatus = runtime.compile(std::move(executable));
    if (!compileStatus) {
        return failStatus("compile realtime video graph", compileStatus);
    }
    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register realtime video runtime nodes", registerStatus);
    }
    auto startStatus = runtime.startThreaded();
    if (!startStatus) {
        return failStatus("start realtime video runtime", startStatus);
    }

    auto waitStatus = waitForRealtimeProgress(runtime, runtimeOptions);
    auto stopStatus = runtime.stop();
    if (!stopStatus) {
        return failStatus("stop realtime video runtime", stopStatus);
    }
    const MediaGraphRuntimeReport finalReport = MediaGraphRuntimeReporter::capture(runtime);
    std::cout << "[CLI] final " << finalReport.summary() << '\n';
    runtime.reset();
    if (!waitStatus) {
        return failStatus("realtime video runtime progress", waitStatus);
    }

    std::cout << "[CLI] realtime video validation stopped successfully\n";
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    try {
        return runRealtimeVideoCli(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[CLI] fatal exception: " << e.what() << '\n';
        return 2;
    } catch (...) {
        std::cerr << "[CLI] fatal unknown exception\n";
        return 2;
    }
}
