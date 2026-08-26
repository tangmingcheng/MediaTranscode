#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "internal/graph/model/MediaNumericIpAddress.h"
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
        "--output",
        "--max-duration",
        "--progress-timeout-ms",
        "--first-output-timeout-ms",
        "--poll-interval-ms",
        "--egress-scope-kind",
        "--egress-scope-id",
        "--egress-scope-authority",
        "--egress-mtu-authority",
        "--egress-address-family",
        "--egress-maximum-ip-packet-bytes",
        "--egress-sender-maximum-payload-bytes",
        "--egress-sustained-wire-bytes-per-second",
        "--egress-peak-wire-bytes-per-second",
        "--egress-burst-wire-bytes",
        "--egress-service-authority",
        "--egress-graph-resource-scope",
        "--egress-maximum-graph-payload-and-reserved-storage-bytes",
        "--egress-maximum-network-memory-bytes",
        "--egress-maximum-socket-memory-bytes",
        "--egress-maximum-residence-ms",
        "--egress-resource-authority",
        "--egress-local-address",
        "--egress-local-first-port",
        "--egress-local-port-count",
        "--egress-local-authority",
        "--egress-target-residence-ms",
        "--egress-latency-authority",
        "--egress-maximum-release-jitter-ms",
        "--egress-release-jitter-authority",
        "--egress-observation-run-datagrams",
        "--egress-observation-drain-residence-ms",
        "--egress-tx-evidence-policy",
        "--egress-observation-authority",
        "--receiver-transport-decode-lead-ms",
        "--receiver-timing-authority",
    };
    valueArgs.insert(valueArgs.end(), realtimeValueArgs.begin(), realtimeValueArgs.end());

    std::vector<std::string> flagArgs = commonVideoTranscodeFlagArgs();
    flagArgs.push_back("--no-low-latency");
    rejectUnknownArgs(argc, argv, valueArgs, flagArgs);
}

MediaRealtimeDeploymentEnvelope parseRealtimeDeploymentEnvelope(
    int argc, char** argv)
{
    const std::string scopeKind = requiredArg(
        argc, argv, "--egress-scope-kind");
    MediaDatagramServiceScopeKind kind =
        MediaDatagramServiceScopeKind::Unknown;
    if (scopeKind == "managed") {
        kind = MediaDatagramServiceScopeKind::ManagedEgress;
    } else if (scopeKind == "provisioned") {
        kind = MediaDatagramServiceScopeKind::ProvisionedEgress;
    } else {
        throw std::invalid_argument(
            "--egress-scope-kind must be managed or provisioned");
    }
    const auto milliseconds = [&](const char* option) {
        const std::size_t value = requiredSizeArg(argc, argv, option);
        if (value == 0 || value > static_cast<std::size_t>(
                std::numeric_limits<std::int64_t>::max() / 1'000'000)) {
            throw std::invalid_argument(
                std::string(option) + " is outside the running-time range");
        }
        return MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(value) * 1'000'000);
    };
    MediaRealtimeDeploymentEnvelopeEncoding encoding;
    encoding.serviceScope = {
        kind,
        requiredArg(argc, argv, "--egress-scope-id"),
        requiredArg(argc, argv, "--egress-scope-authority")};
    const std::string addressFamily = requiredArg(
        argc, argv, "--egress-address-family");
    const auto mtuFamily = addressFamily == "ipv4"
        ? MediaIpAddressFamily::Ipv4
        : addressFamily == "ipv6"
            ? MediaIpAddressFamily::Ipv6
            : throw std::invalid_argument(
                  "--egress-address-family must be ipv4 or ipv6");
    encoding.mtu = {
        mtuFamily,
        requiredArg(argc, argv, "--egress-mtu-authority"),
        requiredSizeArg(argc, argv, "--egress-maximum-ip-packet-bytes"),
        requiredSizeArg(argc, argv, "--egress-sender-maximum-payload-bytes")};
    encoding.service = {
        requiredSizeArg(
            argc, argv, "--egress-sustained-wire-bytes-per-second"),
        requiredSizeArg(
            argc, argv, "--egress-peak-wire-bytes-per-second"),
        requiredSizeArg(argc, argv, "--egress-burst-wire-bytes"),
        requiredArg(argc, argv, "--egress-service-authority")};
    const std::string graphResourceScope = requiredArg(
        argc, argv, "--egress-graph-resource-scope");
    const auto parsedGraphResourceScope =
        graphResourceScope == "engine-managed-payload-and-reserved-storage"
            ? MediaRealtimeGraphResourceBudgetScope::
                  EngineManagedPayloadAndReservedStorage
            : graphResourceScope ==
                  "engine-managed-payload-and-reserved-storage-plus-device"
                ? MediaRealtimeGraphResourceBudgetScope::
                      EngineManagedPayloadAndReservedStoragePlusDevice
                : throw std::invalid_argument(
                      "--egress-graph-resource-scope is invalid");
    encoding.resources = {
        parsedGraphResourceScope,
        requiredSizeArg(
            argc, argv,
            "--egress-maximum-graph-payload-and-reserved-storage-bytes"),
        requiredSizeArg(argc, argv, "--egress-maximum-network-memory-bytes"),
        requiredSizeArg(argc, argv, "--egress-maximum-socket-memory-bytes"),
        requiredArg(argc, argv, "--egress-resource-authority")};
    const std::string localAddress = requiredArg(
        argc, argv, "--egress-local-address");
    std::optional<MediaIpAddressFamily> localFamily;
    if (MediaNumericIpAddress::create(
            MediaIpAddressFamily::Ipv4, localAddress)) {
        localFamily = MediaIpAddressFamily::Ipv4;
    } else if (MediaNumericIpAddress::create(
                   MediaIpAddressFamily::Ipv6, localAddress)) {
        localFamily = MediaIpAddressFamily::Ipv6;
    } else {
        throw std::invalid_argument(
            "--egress-local-address must be a numeric IP address");
    }
    const int localFirstPort = requiredIntArg(
        argc, argv, "--egress-local-first-port");
    const int localPortCount = requiredIntArg(
        argc, argv, "--egress-local-port-count");
    if (localFirstPort <= 0 || localFirstPort > 65'535 ||
        localPortCount <= 0 || localPortCount > 65'535) {
        throw std::invalid_argument(
            "egress local port range values must be within 1..65535");
    }
    encoding.localPorts = {
        *localFamily,
        localAddress,
        static_cast<std::uint16_t>(localFirstPort),
        static_cast<std::uint16_t>(localPortCount),
        requiredArg(argc, argv, "--egress-local-authority")};
    encoding.latency = {
        milliseconds("--egress-target-residence-ms"),
        milliseconds("--egress-maximum-residence-ms"),
        requiredArg(argc, argv, "--egress-latency-authority"),
        milliseconds("--egress-maximum-release-jitter-ms"),
        requiredArg(argc, argv, "--egress-release-jitter-authority")};
    const std::string evidencePolicy = requiredArg(
        argc, argv, "--egress-tx-evidence-policy");
    MediaRealtimeTransmitEvidencePolicy parsedEvidence =
        MediaRealtimeTransmitEvidencePolicy::Unknown;
    if (evidencePolicy == "disabled") {
        parsedEvidence = MediaRealtimeTransmitEvidencePolicy::Disabled;
    } else if (evidencePolicy == "report") {
        parsedEvidence = MediaRealtimeTransmitEvidencePolicy::Report;
    } else if (evidencePolicy == "fail") {
        parsedEvidence = MediaRealtimeTransmitEvidencePolicy::Fail;
    } else {
        throw std::invalid_argument(
            "--egress-tx-evidence-policy must be disabled, report, or fail");
    }
    encoding.observation = {
        requiredSizeArg(argc, argv, "--egress-observation-run-datagrams"),
        milliseconds("--egress-observation-drain-residence-ms"),
        parsedEvidence,
        requiredArg(argc, argv, "--egress-observation-authority")};
    const bool hasDecodeLead = hasArg(
        argc, argv, "--receiver-transport-decode-lead-ms");
    const bool hasTimingAuthority = hasArg(
        argc, argv, "--receiver-timing-authority");
    if (hasDecodeLead || hasTimingAuthority) {
        if (!(hasDecodeLead && hasTimingAuthority)) {
            throw std::invalid_argument(
                "receiver timing capability requires decode lead and authority together");
        }
        encoding.receiverTiming = MediaRealtimeReceiverTimingCapability{
            milliseconds("--receiver-transport-decode-lead-ms"),
            requiredArg(argc, argv, "--receiver-timing-authority")};
    }
    auto envelope = MediaRealtimeDeploymentEnvelope::decode(
        std::move(encoding));
    if (!envelope) throw std::invalid_argument(envelope.error().message);
    return std::move(envelope).value();
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
        return;
    }

    output.url = requiredArg(argc, argv, "--output");
}

void parseAudioRtpOptions(int argc, char** argv, MediaRealtimeRtpTranscodeRequest& options)
{
    if (hasArg(argc, argv, "--audio-rtp-url")) {
        options.input.audioRtp.url = requiredArg(argc, argv, "--audio-rtp-url");
    }
    if (hasArg(argc, argv, "--audio-rtp-codec")) {
        options.input.audioRtp.codecName = requiredArg(argc, argv, "--audio-rtp-codec");
    }
    if (hasArg(argc, argv, "--audio-rtp-payload-type")) {
        options.input.audioRtp.payloadType = requiredIntArg(
            argc, argv, "--audio-rtp-payload-type");
    }
    if (hasArg(argc, argv, "--audio-rtp-clock-rate")) {
        options.input.audioRtp.clockRate = requiredIntArg(
            argc, argv, "--audio-rtp-clock-rate");
    }
    if (hasArg(argc, argv, "--audio-rtp-channels")) {
        options.input.audioRtp.channels = requiredIntArg(
            argc, argv, "--audio-rtp-channels");
    }
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
    options.deployment = parseRealtimeDeploymentEnvelope(argc, argv);
    parseCommonVideoTranscodeOptions(argc, argv, options.parameters);
    parseAudioRtpOptions(argc, argv, options);
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
        std::cout << "Usage: media_transcode_realtime_video_cli --media-id ID --input-type rtsp|rtp|mpegts-udp --input-layout session|separate|mpegts --output-layout separate|mpegts --output-transport udp|rtp --mpegts-max-pcr-gap-ms 1000 [--hardware-backend auto|rkmpp] [--max-duration SECONDS] [options]\n";
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
              << " hw="
              << (options.parameters.execution.disableHardware
                      ? "disabled"
                      : mediaHardwareBackendRequestName(
                            options.parameters.execution.hardwareBackend))
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
