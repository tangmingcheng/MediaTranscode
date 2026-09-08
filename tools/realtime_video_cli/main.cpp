#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../common/GraphCliSupport.h"
#include "../common/VideoCliTranscodeOptions.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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
    if (value == "url") {
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
    std::vector<std::string> valueArgs {
        "--video-codec",
        "--rc",
        "--width",
        "--height",
        "--fps",
        "--bitrate",
        "--min-bitrate",
        "--max-bitrate",
        "--gop",
        "--audio-codec",
        "--audio-rc",
        "--audio-bitrate",
        "--audio-min-bitrate",
        "--audio-max-bitrate",
        "--sample-rate",
        "--channels",
        "--media-id",
        "--input-type",
        "--output-layout",
        "--output-transport",
        "--egress-capacity-bps",
        "--maximum-wire-residence-ms",
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
        "--output",
        "--max-duration",
        "--progress-timeout-ms",
        "--first-output-timeout-ms",
        "--poll-interval-ms",
    };

    std::vector<std::string> flagArgs = commonVideoTranscodeFlagArgs();
    rejectUnknownArgs(argc, argv, valueArgs, flagArgs);
}

void parseRealtimeInputOptions(
    int argc,
    char** argv,
    RealtimeInputType inputType,
    MediaRealtimeInputConfig& input)
{
    input.type = inputType;
    input.openTimeoutMs = requiredIntArg(argc, argv, "--open-timeout-ms");
    input.readTimeoutMs = requiredIntArg(argc, argv, "--read-timeout-ms");
    input.analyzeDurationUs = requiredIntArg(argc, argv, "--analyze-duration-us");
    input.probeSizeBytes = requiredIntArg(argc, argv, "--probe-size");

    if (hasArg(argc, argv, "--input")) {
        input.url = requiredArg(argc, argv, "--input");
    }
    if (hasArg(argc, argv, "--rtsp-transport")) {
        input.rtspTransport = requiredArg(argc, argv, "--rtsp-transport");
    }
    if (hasArg(argc, argv, "--mpegts-max-pcr-gap-ms")) {
        const int maximumPcrGapMs = requiredIntArg(
            argc, argv, "--mpegts-max-pcr-gap-ms");
        input.mpegTsClock.maximumPcrGap = MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(maximumPcrGapMs) * 1'000'000);
    }
}

void parseRtpInputMetadata(
    int argc,
    char** argv,
    const char* urlArgument,
    const char* codecArgument,
    const char* payloadTypeArgument,
    const char* clockRateArgument,
    const char* channelsArgument,
    const char* fmtpArgument,
    MediaRealtimeRtpInputMetadata& metadata)
{
    if (hasArg(argc, argv, urlArgument)) {
        metadata.url = requiredArg(argc, argv, urlArgument);
    }
    if (hasArg(argc, argv, codecArgument)) {
        metadata.codecName = requiredArg(argc, argv, codecArgument);
    }
    if (hasArg(argc, argv, payloadTypeArgument)) {
        metadata.payloadType = requiredIntArg(argc, argv, payloadTypeArgument);
    }
    if (hasArg(argc, argv, clockRateArgument)) {
        metadata.clockRate = requiredIntArg(argc, argv, clockRateArgument);
    }
    if (channelsArgument && hasArg(argc, argv, channelsArgument)) {
        metadata.channels = requiredIntArg(argc, argv, channelsArgument);
    }
    if (hasArg(argc, argv, fmtpArgument)) {
        metadata.fmtp = requiredArg(argc, argv, fmtpArgument);
    }
}

void parseRealtimeOutputOptions(
    int argc,
    char** argv,
    RealtimeOutputStreamLayout outputLayout,
    MediaOutputTransportKind outputTransport,
    MediaRealtimeOutputConfig& output)
{
    output.streamLayout = outputLayout;
    output.transport = outputTransport;
    if (hasArg(argc, argv, "--rtp-host")) {
        output.host = requiredArg(argc, argv, "--rtp-host");
    }
    if (hasArg(argc, argv, "--rtp-port")) {
        output.basePort = static_cast<std::size_t>(
            requiredIntArg(argc, argv, "--rtp-port"));
    }
    if (hasArg(argc, argv, "--sdp")) {
        output.sdpPath = requiredArg(argc, argv, "--sdp");
    }
    if (hasArg(argc, argv, "--output")) {
        output.url = requiredArg(argc, argv, "--output");
    }
}

MediaRealtimeRtpTranscodeRequest parseRealtimeOptions(int argc, char** argv)
{
    rejectUnknownRealtimeArgs(argc, argv);

    MediaTranscodeParameterSet parsedTranscode;
    parseCommonVideoTranscodeOptions(argc, argv, parsedTranscode);
    if (!hasArg(argc, argv, "--rc")) {
        throw std::invalid_argument("missing required argument: --rc");
    }
    if (parsedTranscode.video.rateControl != MediaRateControlMode::Cbr &&
        parsedTranscode.video.rateControl != MediaRateControlMode::Vbr) {
        throw std::invalid_argument(
            "realtime video --rc must be cbr or vbr");
    }
    if (!parsedTranscode.video.bitrateKbps) {
        throw std::invalid_argument(
            "missing required integer argument: --bitrate");
    }
    if (*parsedTranscode.video.bitrateKbps <= 0) {
        throw std::invalid_argument(
            "realtime video --bitrate must be positive");
    }
    if (!parsedTranscode.video.gop) {
        throw std::invalid_argument(
            "missing required integer argument: --gop");
    }
    if (*parsedTranscode.video.gop <= 0) {
        throw std::invalid_argument(
            "realtime video --gop must be positive");
    }
    const RealtimeInputType inputType = requiredRealtimeInputType(argc, argv);
    const RealtimeOutputStreamLayout outputLayout =
        requiredRealtimeOutputLayout(argc, argv);
    const MediaOutputTransportKind outputTransport =
        requiredRealtimeOutputTransport(argc, argv);

    MediaRealtimeRtpTranscodeRequest options;
    options.mediaId = requiredArg(argc, argv, "--media-id");
    options.deployment.provisionedEgressCapacityBitsPerSecond =
        requiredUint64Arg(argc, argv, "--egress-capacity-bps");
    const auto maximumWireResidenceMs = requiredUint64Arg(
        argc, argv, "--maximum-wire-residence-ms");
    if (maximumWireResidenceMs == 0 ||
        maximumWireResidenceMs > static_cast<std::uint64_t>(
            (std::numeric_limits<std::int64_t>::max)() / 1'000'000)) {
        throw std::invalid_argument(
            "--maximum-wire-residence-ms is outside the positive running-time range");
    }
    options.deployment.maximumWireResidence =
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(
            maximumWireResidenceMs * 1'000'000));
    parseRealtimeInputOptions(argc, argv, inputType, options.input);
    parseRtpInputMetadata(
        argc, argv,
        "--video-rtp-url", "--video-rtp-codec",
        "--video-rtp-payload-type", "--video-rtp-clock-rate",
        nullptr, "--video-rtp-fmtp", options.input.videoRtp);
    parseRtpInputMetadata(
        argc, argv,
        "--audio-rtp-url", "--audio-rtp-codec",
        "--audio-rtp-payload-type", "--audio-rtp-clock-rate",
        "--audio-rtp-channels", "--audio-rtp-fmtp",
        options.input.audioRtp);
    parseRealtimeOutputOptions(
        argc, argv, outputLayout, outputTransport, options.output);
    options.parameters.execution.streamSet = parsedTranscode.execution.streamSet;
    options.parameters.execution.diagnosticLogEnabled =
        parsedTranscode.execution.diagnosticLogEnabled;
    options.parameters.video.codecName = std::move(parsedTranscode.video.codecName);
    options.parameters.video.width = parsedTranscode.video.width;
    options.parameters.video.height = parsedTranscode.video.height;
    options.parameters.video.frameRate = parsedTranscode.video.frameRate;
    options.parameters.video.rateControl = parsedTranscode.video.rateControl;
    options.parameters.video.bitrateKbps = parsedTranscode.video.bitrateKbps;
    options.parameters.video.minBitrateKbps = parsedTranscode.video.minBitrateKbps;
    options.parameters.video.maxBitrateKbps = parsedTranscode.video.maxBitrateKbps;
    options.parameters.video.gop = parsedTranscode.video.gop;
    options.parameters.audio.codecName = std::move(parsedTranscode.audio.codecName);
    options.parameters.audio.rateControl = parsedTranscode.audio.rateControl;
    options.parameters.audio.bitrateKbps = parsedTranscode.audio.bitrateKbps;
    options.parameters.audio.minBitrateKbps = parsedTranscode.audio.minBitrateKbps;
    options.parameters.audio.maxBitrateKbps = parsedTranscode.audio.maxBitrateKbps;
    options.parameters.audio.sampleRate = parsedTranscode.audio.sampleRate;
    options.parameters.audio.channels = parsedTranscode.audio.channels;
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

MediaRealtimeVideoRunPolicy makeRunPolicy(
    const RealtimeVideoRuntimeOptions& options)
{
    std::optional<std::chrono::milliseconds> maximumDuration;
    if (options.maxDurationSeconds) {
        maximumDuration = std::chrono::seconds(*options.maxDurationSeconds);
    }
    auto policy = MediaRealtimeVideoRunPolicy::create(
        std::chrono::milliseconds(options.progressTimeoutMs),
        std::chrono::milliseconds(options.firstOutputTimeoutMs),
        std::chrono::milliseconds(options.pollIntervalMs),
        maximumDuration);
    if (!policy) {
        throw std::invalid_argument(policy.error().message);
    }
    return std::move(policy).value();
}

const char* runFailureAction(MediaRealtimeVideoRunStage stage) noexcept
{
    switch (stage) {
    case MediaRealtimeVideoRunStage::Preflight:
        return "realtime video graph preflight";
    case MediaRealtimeVideoRunStage::ExecutableGraphBuild:
        return "realtime video executable graph build";
    case MediaRealtimeVideoRunStage::PreparedNotification:
        return "print realtime video plan summary";
    case MediaRealtimeVideoRunStage::RuntimeCompile:
        return "compile realtime video graph";
    case MediaRealtimeVideoRunStage::RuntimeNodeRegistration:
        return "register realtime video runtime nodes";
    case MediaRealtimeVideoRunStage::RuntimeStart:
        return "start realtime video runtime";
    case MediaRealtimeVideoRunStage::PolicyValidation:
    case MediaRealtimeVideoRunStage::StopRequested:
    case MediaRealtimeVideoRunStage::RuntimeProgress:
    case MediaRealtimeVideoRunStage::RuntimeCompletion:
    case MediaRealtimeVideoRunStage::Completed:
        return "realtime video runtime";
    }
    return "realtime video runtime";
}

void printPreparedReport(const MediaRealtimeVideoPreparedReport& report)
{
    if (report.audio) {
        std::cout << "[CLI] audio_plan branch="
                  << mediaBranchModeName(report.audio->branchMode)
                  << " reason=" << report.audio->reason;
        if (report.audio->resolvedOutput) {
            const auto& output = *report.audio->resolvedOutput;
            std::cout << " codec=" << output.codecName
                      << " sample_rate=" << output.sampleRate
                      << " channels=" << output.channels
                      << " access_unit_samples=" << output.accessUnitSamples;
            if (!output.encoderName.empty()) {
                std::cout << " encoder=" << output.encoderName;
            }
            if (output.bitrateKbps) {
                std::cout << " bitrate_kbps=" << *output.bitrateKbps;
            }
        }
        std::cout << '\n';
    }
    std::cout << "[CLI] selected_chain=" << report.selectedChain
              << " score=" << report.selectedScore
              << " decoder=" << report.decoderName
              << " filter="
              << (report.filterActive ? report.filterName : "not_required")
              << " encoder=" << report.encoderName
              << '\n';
}

void printStalledEdges(const MediaGraphRuntimeReport& report)
{
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
}

int runRealtimeVideoCli(int argc, char** argv)
{
    std::cout << std::unitbuf;

    const bool helpRequested = hasArg(argc, argv, "--help") || hasArg(argc, argv, "-h");
    if (argc < 5 || helpRequested) {
        std::cout << "Usage: media_transcode_realtime_video_cli --media-id ID --input-type url|rtp|mpegts-udp --output-layout separate|mpegts --output-transport udp|rtp --egress-capacity-bps BPS --maximum-wire-residence-ms MS [--max-duration SECONDS] [options]\n";
        std::cout << "Realtime video encoding: --rc cbr requires positive --bitrate; --rc vbr requires positive --min-bitrate, --bitrate, and --max-bitrate; --gop is always a required positive frame count.\n";
        std::cout << "Raw RTP video: omit --video-rtp-fmtp only for H264/HEVC in-band parameter-set probing; codec, payload type, clock rate, URL, and all probe limits remain required.\n";
        std::cout << "Raw RTP audio: AAC requires explicit --audio-rtp-fmtp; Opus keeps its no-fmtp contract.\n";
        return helpRequested ? 0 : 2;
    }

    MediaRealtimeRtpTranscodeRequest options = parseRealtimeOptions(argc, argv);
    RealtimeVideoRuntimeOptions runtimeOptions = parseRuntimeOptions(argc, argv);
    const MediaRealtimeVideoRunPolicy runPolicy = makeRunPolicy(runtimeOptions);
    std::cout << "[CLI] input_type=" << static_cast<int>(*options.input.type)
              << " input=" << redactUrlUserInfo(options.input.url.empty() ? options.input.videoRtp.url : options.input.url)
              << " audio="
              << (options.parameters.execution.streamSet == MediaTranscodeStreamSet::AudioVideo
                      ? "on"
                      : "off")
              << " max_duration=";
    if (runtimeOptions.maxDurationSeconds) {
        std::cout << *runtimeOptions.maxDurationSeconds;
    } else {
        std::cout << "source_driven";
    }
    std::cout
              << " hw=planner-highest-score"
              << '\n';

    MediaRealtimeVideoRunControl control;
    const MediaRealtimeVideoRunObserver observer {
        printPreparedReport,
        [](const MediaGraphRuntimeReport& report) {
            std::cout << "[CLI] " << report.summary() << '\n';
        }
    };
    const MediaRealtimeVideoRunOutcome outcome =
        MediaRealtimeVideoRunController::run(
            options, runPolicy, control, observer);
    if (outcome.endReason == MediaRealtimeVideoRunEndReason::ProgressTimeout &&
        outcome.failureReport) {
        printStalledEdges(*outcome.failureReport);
    }
    if (outcome.finalReport) {
        std::cout << "[CLI] final " << outcome.finalReport->summary() << '\n';
    }
    if (!outcome.status) {
        return failStatus(runFailureAction(outcome.stage), outcome.status);
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
