#pragma once

#include "RealtimeCommandArguments.h"
#include "../common/GraphCliSupport.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

#include <set>

namespace media::ffmpeg::graph::cli {

inline std::vector<std::string> realtimeInputValueArgs()
{
    return {"--input-type", "--input", "--rtsp-transport", "--open-timeout-ms",
        "--read-timeout-ms", "--input-latency-ms", "--analyze-duration-us", "--probe-size", "--mpegts-max-pcr-gap-ms",
        "--video-rtp-url", "--video-rtp-codec", "--video-rtp-payload-type", "--video-rtp-clock-rate",
        "--video-rtp-fmtp", "--audio-rtp-url", "--audio-rtp-codec", "--audio-rtp-payload-type",
        "--audio-rtp-clock-rate", "--audio-rtp-channels", "--audio-rtp-fmtp"};
}

inline RealtimeInputType requiredRealtimeInputType(int argc, char** argv)
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

inline void parseRealtimeInputOptions(
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
    if (hasArg(argc, argv, "--input-latency-ms")) {
        input.inputLatencyMs = requiredIntArg(argc, argv, "--input-latency-ms");
        if (inputType != RealtimeInputType::RtpPort || *input.inputLatencyMs <= 0)
            throw std::invalid_argument("--input-latency-ms requires RTP input and a positive value");
    }

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

inline void parseRtpInputMetadata(
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

inline MediaRealtimeInputConfig parseRealtimeInput(int argc, char** argv)
{
    MediaRealtimeInputConfig input;
    parseRealtimeInputOptions(argc, argv, requiredRealtimeInputType(argc, argv), input);
    parseRtpInputMetadata(argc, argv, "--video-rtp-url", "--video-rtp-codec",
        "--video-rtp-payload-type", "--video-rtp-clock-rate", nullptr, "--video-rtp-fmtp", input.videoRtp);
    parseRtpInputMetadata(argc, argv, "--audio-rtp-url", "--audio-rtp-codec",
        "--audio-rtp-payload-type", "--audio-rtp-clock-rate", "--audio-rtp-channels",
        "--audio-rtp-fmtp", input.audioRtp);
    return input;
}

inline MediaRealtimeCompositionConfig parseRealtimeCompositionInputs(int argc, char** argv)
{
    const auto allowed = realtimeInputValueArgs();
    std::set<std::string> uniqueCompositionOptions;
    for (int i = 1; i < argc; ++i) {
        const std::string option(argv[i]);
        if ((option == "--grid" || option == "--audio-source" || option == "--preparation-storage-limit-bytes") &&
            !uniqueCompositionOptions.insert(option).second)
            throw std::invalid_argument("duplicate composition option: " + option);
        if (containsKey(allowed, argv[i]))
            throw std::invalid_argument("composition input options must be inside --inputs values");
        if (std::string_view(argv[i]).starts_with("--") && i + 1 < argc &&
            !std::string_view(argv[i + 1]).starts_with("--")) ++i;
        else if (std::string_view(argv[i]) == "--inputs" && i + 1 < argc) ++i;
    }
    if (requiredArg(argc, argv, "--grid") != "fixed")
        throw std::invalid_argument("composition requires --grid fixed");
    const auto audioSource = requiredIntArg(argc, argv, "--audio-source");
    if (audioSource < 0) throw std::invalid_argument("--audio-source must be a zero-based source index");
    MediaRealtimeCompositionConfig config{{}, MediaRealtimeCompositionGrid::Fixed,
        static_cast<std::size_t>(audioSource),
        requiredUint64Arg(argc, argv, "--preparation-storage-limit-bytes")};
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) != "--inputs") continue;
        if (++i == argc) throw std::invalid_argument("--inputs requires input options");
        auto arguments = tokenizeRealtimeCommand(argv[i]);
        arguments.insert(arguments.begin(), "composition-source");
        std::vector<char*> pointers;
        pointers.reserve(arguments.size());
        for (auto& argument : arguments) pointers.push_back(argument.data());
        const auto count = static_cast<int>(pointers.size());
        // Every source has an independent, complete input fact set.
        std::set<std::string> seen;
        for (std::size_t j = 1; j < arguments.size(); j += 2) {
            if (!containsKey(allowed, arguments[j]) || j + 1 == arguments.size() ||
                !seen.insert(arguments[j]).second)
                throw std::invalid_argument("--inputs contains an unknown, duplicate or incomplete input option");
        }
        auto input = parseRealtimeInput(count, pointers.data());
        if (input.type != RealtimeInputType::RtpPort)
            throw std::invalid_argument("composition inputs require RTP/RTCP");
        if (!input.inputLatencyMs)
            throw std::invalid_argument("each composition source requires --input-latency-ms");
        config.inputs.push_back(std::move(input));
    }
    if (config.inputs.size() < 2 || config.inputs.size() > 4 || config.audioSource >= config.inputs.size())
        throw std::invalid_argument("composition requires 2-4 sources and a valid --audio-source index");
    return config;
}

} // namespace media::ffmpeg::graph::cli
