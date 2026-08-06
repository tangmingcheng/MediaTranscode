#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/diagnostics/MediaGraphRuntimeReport.h"
#include "internal/graph/runtime/lifecycle/MediaRealtimeProgressTracker.h"
#include "internal/graph/runtime/lifecycle/MediaRealtimeRuntimeCompletion.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"
#include "../common/VideoCliTranscodeOptions.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <optional>
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
    std::optional<int> maxDurationSeconds;
    int progressTimeoutMs = 5000;
    int firstOutputTimeoutMs = 30000;
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

MediaOutputTransportKind requiredRealtimeOutputTransport(int argc, char** argv)
{
    const std::string value = requiredArg(argc, argv, "--output-transport");
    if (value == "udp") {
        return MediaOutputTransportKind::UdpDatagrams;
    }
    if (value == "rtp") {
        return MediaOutputTransportKind::RtpAvp;
    }
    throw std::invalid_argument("unsupported --output-transport: " + value);
}

void rejectUnknownRealtimeArgs(int argc, char** argv)
{
    std::vector<std::string> valueArgs = commonVideoTranscodeValueArgs();
    const std::vector<std::string> realtimeValueArgs {
        "--media-id",
        "--input-type",
        "--input-layout",
        "--output-layout",
        "--output-transport",
        "--input",
        "--rtsp-transport",
        "--open-timeout-ms",
        "--read-timeout-ms",
        "--analyze-duration-us",
        "--probe-size",
        "--mpegts-max-pcr-gap-ms",
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
        "--first-output-timeout-ms",
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

    if (*input.type != RealtimeInputType::MpegTsUdp &&
        hasArg(argc, argv, "--mpegts-max-pcr-gap-ms")) {
        throw std::invalid_argument("--mpegts-max-pcr-gap-ms is valid only for mpegts-udp input");
    }

    if (*input.type == RealtimeInputType::RtpPort) {
        input.videoRtp.url = requiredArg(argc, argv, "--video-rtp-url");
        input.videoRtp.codecName = requiredArg(argc, argv, "--video-rtp-codec");
        input.videoRtp.payloadType = requiredIntArg(argc, argv, "--video-rtp-payload-type");
        input.videoRtp.clockRate = requiredIntArg(argc, argv, "--video-rtp-clock-rate");
        if (hasArg(argc, argv, "--video-rtp-fmtp")) {
            input.videoRtp.fmtp = requiredArg(argc, argv, "--video-rtp-fmtp");
        }
        return;
    }

    input.url = requiredArg(argc, argv, "--input");
    if (*input.type == RealtimeInputType::Url) {
        input.rtspTransport = requiredArg(argc, argv, "--rtsp-transport");
        return;
    }
    const int maximumPcrGapMs = requiredIntArg(argc, argv, "--mpegts-max-pcr-gap-ms");
    if (maximumPcrGapMs <= 0) {
        throw std::invalid_argument("MPEG-TS maximum PCR gap must be positive");
    }
    input.mpegTsClock.maximumPcrGap = MediaRunningTime::fromNanoseconds(
        static_cast<std::int64_t>(maximumPcrGapMs) * 1'000'000);
}

void parseRealtimeOutputOptions(int argc, char** argv, MediaRealtimeOutputConfig& output)
{
    output.streamLayout = requiredRealtimeOutputLayout(argc, argv);
    output.transport = requiredRealtimeOutputTransport(argc, argv);
    if (*output.transport == MediaOutputTransportKind::RtpAvp) {
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
    if (hasArg(argc, argv, "--audio-rtp-fmtp")) {
        options.input.audioRtp.fmtp = requiredArg(argc, argv, "--audio-rtp-fmtp");
    }
}

MediaRealtimeRtpTranscodeRequest parseRealtimeOptions(int argc, char** argv)
{
    rejectUnknownRealtimeArgs(argc, argv);

    MediaRealtimeRtpTranscodeRequest options;
    options.mediaId = requiredArg(argc, argv, "--media-id");
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
    options.maxDurationSeconds = optionalIntArg(argc, argv, "--max-duration");
    if (auto progressTimeoutMs = optionalIntArg(argc, argv, "--progress-timeout-ms")) {
        options.progressTimeoutMs = *progressTimeoutMs;
    }
    if (auto firstOutputTimeoutMs = optionalIntArg(argc, argv, "--first-output-timeout-ms")) {
        options.firstOutputTimeoutMs = *firstOutputTimeoutMs;
    }
    if (auto pollIntervalMs = optionalIntArg(argc, argv, "--poll-interval-ms")) {
        options.pollIntervalMs = *pollIntervalMs;
    }
    if ((options.maxDurationSeconds && *options.maxDurationSeconds <= 0) ||
        options.progressTimeoutMs <= 0 ||
        options.firstOutputTimeoutMs <= 0 ||
        options.pollIntervalMs <= 0) {
        throw std::invalid_argument(
            "configured runtime duration, progress timeout, first-output timeout, and poll interval must be positive");
    }
    return options;
}

::media::Status waitForRealtimeProgress(MediaGraphRuntime& runtime,
                                        const RealtimeVideoRuntimeOptions& options)
{
    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    auto lastProgressAt = startedAt;
    MediaRealtimeProgressTracker progressTracker;
    const auto firstOutputStartupDeadline =
        std::chrono::milliseconds(options.firstOutputTimeoutMs);
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
        if (runtime.threadedCompleted()) {
            return ::media::Status::success();
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
            auto workerFailure = runtime.synchronizeThreadedState();
            if (!workerFailure) {
                return workerFailure;
            }
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "realtime runtime reported worker errors without a preserved primary failure"));
        }
        const auto now = Clock::now();
        if (report.metrics.activeWorkers == 0 &&
            now - startedAt >= workerStartupGrace) {
            auto terminalStatus = runtime.synchronizeThreadedState();
            if (!terminalStatus) {
                return terminalStatus;
            }
            if (runtime.threadedCompleted()) {
                return ::media::Status::success();
            }
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("realtime runtime has no active workers"));
        }
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - startedAt);
        auto progress = progressTracker.observe(
            report.metrics.workerProgress,
            report.metrics.encodedPacketsPushed,
            elapsedMs);
        if (!progress) {
            return ::media::Status::failure(progress.error());
        }
        if (progress.value()) {
            lastProgressAt = Clock::now();
        }

        const auto idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProgressAt).count();
        if (progressTracker.firstOutputDeadlineExpired(
                elapsedMs, firstOutputStartupDeadline)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "realtime runtime produced no encoded output before startup deadline"));
        }
        if (options.maxDurationSeconds &&
            progressTracker.maximumOutputDurationExpired(
                elapsedMs, std::chrono::seconds(*options.maxDurationSeconds))) {
            return ::media::Status::success();
        }
        if (idleMs >= options.progressTimeoutMs) {
            for (const auto& decision : report.backpressure.decisions) {
                if (decision.kind == MediaBackpressureDecisionKind::QueueFull ||
                    decision.kind ==
                        MediaBackpressureDecisionKind::AboveCriticalWatermark) {
                    std::cerr << "[CLI] stalled edge=" << decision.edgeId.value
                              << " queued=" << decision.queueSize
                              << " capacity=" << decision.capacity
                              << " reason=" << decision.message << '\n';
                }
            }
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
        std::cout << "Usage: media_transcode_realtime_video_cli --media-id ID --input-type rtsp|rtp|mpegts-udp --input-layout session|separate|mpegts --output-layout separate|mpegts --output-transport udp|rtp --metadata-queue 1 --packet-queue 256 --frame-queue 128 --mux-queue 256 --startup-max-video-unit-bytes 4194304 --startup-max-audio-unit-bytes 1048576 --startup-max-gap-ms 40 --mpegts-max-pcr-gap-ms 1000 [--max-duration SECONDS] [options]\n";
        std::cout << "Raw RTP video: omit --video-rtp-fmtp only for H264/HEVC in-band parameter-set probing; codec, payload type, clock rate, URL, and all probe limits remain required.\n";
        std::cout << "Raw RTP audio: AAC requires explicit --audio-rtp-fmtp; Opus keeps its no-fmtp contract.\n";
        return helpRequested ? 0 : 2;
    }

    MediaRealtimeRtpTranscodeRequest options = parseRealtimeOptions(argc, argv);
    RealtimeVideoRuntimeOptions runtimeOptions = parseRuntimeOptions(argc, argv);
    std::cout << "[CLI] input_type=" << static_cast<int>(*options.input.type)
              << " input=" << redactUrlUserInfo(options.input.url.empty() ? options.input.videoRtp.url : options.input.url)
              << " audio=" << (options.parameters.execution.includeAudio ? "on" : "off")
              << " max_duration=";
    if (runtimeOptions.maxDurationSeconds) {
        std::cout << *runtimeOptions.maxDurationSeconds;
    } else {
        std::cout << "source_driven";
    }
    std::cout
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

    const auto waitStatus = waitForRealtimeProgress(runtime, runtimeOptions);
    const auto completion = MediaRealtimeRuntimeCompletion::complete(runtime, waitStatus);
    const MediaGraphRuntimeReport finalReport = MediaGraphRuntimeReporter::capture(runtime);
    std::cout << "[CLI] final " << finalReport.summary() << '\n';
    runtime.reset();
    if (!completion.status) {
        return failStatus("realtime video runtime", completion.status);
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
