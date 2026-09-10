#pragma once

#include "../common/VideoCliTranscodeOptions.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoOutputRequest.h"

namespace media::ffmpeg::graph::cli {

inline std::vector<std::string> realtimeVideoOutputValueArgs()
{
    return {
        "--video-codec",
        "--rc",
        "--width",
        "--height",
        "--fps",
        "--bitrate",
        "--min-bitrate",
        "--max-bitrate",
        "--gop",
        "--output-layout",
        "--output-transport",
        "--rtp-host",
        "--rtp-port",
        "--sdp",
        "--output",
    };
}

inline RealtimeOutputStreamLayout requiredRealtimeOutputLayout(int argc, char** argv)
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

inline MediaOutputTransportKind requiredRealtimeOutputTransport(int argc, char** argv)
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

inline void parseRealtimeOutputOptions(
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

inline MediaRealtimeVideoOutputRequest parseRealtimeVideoOutputOptions(
    int argc, char** argv, const MediaTranscodeParameterSet& parsedTranscode)
{
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
    MediaRealtimeVideoOutputRequest request;
    parseRealtimeOutputOptions(argc, argv, requiredRealtimeOutputLayout(argc, argv),
        requiredRealtimeOutputTransport(argc, argv), request.output);
    request.video.codecName = parsedTranscode.video.codecName;
    request.video.width = parsedTranscode.video.width;
    request.video.height = parsedTranscode.video.height;
    request.video.frameRate = parsedTranscode.video.frameRate;
    request.video.rateControl = parsedTranscode.video.rateControl;
    request.video.bitrateKbps = parsedTranscode.video.bitrateKbps;
    request.video.minBitrateKbps = parsedTranscode.video.minBitrateKbps;
    request.video.maxBitrateKbps = parsedTranscode.video.maxBitrateKbps;
    request.video.gop = parsedTranscode.video.gop;
    return request;
}

} // namespace media::ffmpeg::graph::cli
