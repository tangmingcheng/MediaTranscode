#pragma once

#include "GraphCliSupport.h"

#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph::cli {

inline void parseCommonVideoTranscodeOptions(int argc, char** argv, MediaTranscodeParameterSet& parameters)
{
    parameters.execution.includeVideo = true;
    parameters.execution.includeAudio = !hasArg(argc, argv, "--no-audio");
    parameters.execution.disableHardware = disabledByExplicitArg(argc, argv, "--disable-hw", "hardware planning");
    parameters.execution.diagnosticLogEnabled = !hasArg(argc, argv, "--quiet-graph");
    parameters.queues.metadata = requiredSizeArg(argc, argv, "--metadata-queue");
    parameters.queues.packet = requiredSizeArg(argc, argv, "--packet-queue");
    parameters.queues.frame = requiredSizeArg(argc, argv, "--frame-queue");
    parameters.queues.mux = requiredSizeArg(argc, argv, "--mux-queue");

    parameters.video.codecName = argValue(argc, argv, "--video-codec");
    parameters.video.rateControl = rateControlArg(argc, argv, "--rc");
    parameters.video.preset = argValue(argc, argv, "--preset");
    parameters.video.profile = argValue(argc, argv, "--profile");
    parameters.video.tune = argValue(argc, argv, "--tune");
    parameters.video.level = argValue(argc, argv, "--level");
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

    parameters.audio.codecName = argValue(argc, argv, "--audio-codec");
    parameters.audio.rateControl = rateControlArg(argc, argv, "--audio-rc");
    parameters.audio.bitrateKbps = optionalIntArg(argc, argv, "--audio-bitrate");
    parameters.audio.minBitrateKbps = optionalIntArg(argc, argv, "--audio-min-bitrate");
    parameters.audio.maxBitrateKbps = optionalIntArg(argc, argv, "--audio-max-bitrate");
    parameters.audio.bufferSizeKbits = optionalIntArg(argc, argv, "--audio-buffer-size");
    parameters.audio.sampleRate = optionalIntArg(argc, argv, "--sample-rate");
    parameters.audio.channels = optionalIntArg(argc, argv, "--channels");
    parameters.audio.quality = optionalIntArg(argc, argv, "--audio-quality");
    parameters.audio.preset = argValue(argc, argv, "--audio-preset");
    parameters.audio.profile = argValue(argc, argv, "--audio-profile");
}

inline std::vector<std::string> commonVideoTranscodeValueArgs()
{
    return {
        "--metadata-queue",
        "--packet-queue",
        "--frame-queue",
        "--mux-queue",
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
        "--buffer-size",
        "--quality",
        "--gop",
        "--audio-codec",
        "--audio-rc",
        "--audio-bitrate",
        "--audio-min-bitrate",
        "--audio-max-bitrate",
        "--audio-buffer-size",
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
        "--disable-hw",
        "--quiet-graph",
    };
}

} // namespace media::ffmpeg::graph::cli
