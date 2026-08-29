#pragma once

#include "GraphCliSupport.h"

#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph::cli {

inline void parseCommonVideoTranscodeOptions(int argc, char** argv, MediaTranscodeParameterSet& parameters)
{
    parameters.execution.streamSet = hasArg(argc, argv, "--no-audio")
        ? MediaTranscodeStreamSet::VideoOnly
        : MediaTranscodeStreamSet::AudioVideo;
    parameters.execution.diagnosticLogEnabled = !hasArg(argc, argv, "--quiet-graph");
    parameters.video.codecName = argValue(argc, argv, "--video-codec");
    parameters.video.rateControl = rateControlArg(argc, argv, "--rc");
    parameters.video.preset = argValue(argc, argv, "--preset");
    parameters.video.profile = argValue(argc, argv, "--profile");
    parameters.video.tune = argValue(argc, argv, "--tune");
    parameters.video.level = argValue(argc, argv, "--level");
    parameters.video.width = optionalIntArg(argc, argv, "--width");
    parameters.video.height = optionalIntArg(argc, argv, "--height");
    if (auto targetFps = optionalIntArg(argc, argv, "--fps")) {
        parameters.video.frameRate.numerator = targetFps;
        parameters.video.frameRate.denominator = 1;
    }
    parameters.video.bitrateKbps = optionalIntArg(argc, argv, "--bitrate");
    parameters.video.minBitrateKbps = optionalIntArg(argc, argv, "--min-bitrate");
    parameters.video.maxBitrateKbps = optionalIntArg(argc, argv, "--max-bitrate");
    parameters.video.quality = optionalIntArg(argc, argv, "--quality");
    parameters.video.gop = optionalIntArg(argc, argv, "--gop");
    if (parameters.video.rateControl == MediaRateControlMode::Cbr &&
        (parameters.video.minBitrateKbps || parameters.video.maxBitrateKbps)) {
        throw std::invalid_argument(
            "CBR accepts --bitrate only; minimum and maximum are VBR facts");
    }
    if (parameters.video.rateControl == MediaRateControlMode::Vbr &&
        (!parameters.video.bitrateKbps || !parameters.video.minBitrateKbps ||
         !parameters.video.maxBitrateKbps)) {
        throw std::invalid_argument(
            "VBR requires --min-bitrate, --bitrate, and --max-bitrate");
    }
    parameters.audio.codecName = argValue(argc, argv, "--audio-codec");
    parameters.audio.rateControl = rateControlArg(argc, argv, "--audio-rc");
    parameters.audio.bitrateKbps = optionalIntArg(argc, argv, "--audio-bitrate");
    parameters.audio.minBitrateKbps = optionalIntArg(argc, argv, "--audio-min-bitrate");
    parameters.audio.maxBitrateKbps = optionalIntArg(argc, argv, "--audio-max-bitrate");
    parameters.audio.sampleRate = optionalIntArg(argc, argv, "--sample-rate");
    parameters.audio.channels = optionalIntArg(argc, argv, "--channels");
    parameters.audio.quality = optionalIntArg(argc, argv, "--audio-quality");
    parameters.audio.preset = argValue(argc, argv, "--audio-preset");
    parameters.audio.profile = argValue(argc, argv, "--audio-profile");
    if (parameters.audio.rateControl == MediaRateControlMode::Cbr &&
        (parameters.audio.minBitrateKbps || parameters.audio.maxBitrateKbps)) {
        throw std::invalid_argument(
            "audio CBR accepts target bitrate only; audio minimum and maximum bitrate are VBR facts");
    }
    if (parameters.audio.rateControl == MediaRateControlMode::Vbr &&
        (!parameters.audio.bitrateKbps || !parameters.audio.minBitrateKbps ||
         !parameters.audio.maxBitrateKbps)) {
        throw std::invalid_argument(
            "audio VBR requires minimum, target, and maximum bitrate facts");
    }
}

inline std::vector<std::string> commonVideoTranscodeValueArgs()
{
    return {
        "--video-codec",
        "--rc",
        "--preset",
        "--profile",
        "--tune",
        "--level",
        "--width",
        "--height",
        "--fps",
        "--bitrate",
        "--min-bitrate",
        "--max-bitrate",
        "--quality",
        "--gop",
        "--audio-codec",
        "--audio-rc",
        "--audio-bitrate",
        "--audio-min-bitrate",
        "--audio-max-bitrate",
        "--sample-rate",
        "--channels",
        "--audio-quality",
        "--audio-preset",
        "--audio-profile",
    };
}

inline std::vector<std::string> commonVideoTranscodeFlagArgs()
{
    return {
        "--help",
        "-h",
        "--no-audio",
        "--quiet-graph",
    };
}

} // namespace media::ffmpeg::graph::cli
