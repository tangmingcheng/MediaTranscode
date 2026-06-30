#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaRateControlMode {
    Auto,
    Cbr,
    Vbr,
    Cq
};

inline const char* mediaRateControlModeName(MediaRateControlMode mode) noexcept
{
    switch (mode) {
    case MediaRateControlMode::Auto: return "auto";
    case MediaRateControlMode::Cbr: return "cbr";
    case MediaRateControlMode::Vbr: return "vbr";
    case MediaRateControlMode::Cq: return "cq";
    }
    return "auto";
}

inline bool parseMediaRateControlMode(std::string_view text, MediaRateControlMode& mode) noexcept
{
    if (text.empty() || text == "auto") {
        mode = MediaRateControlMode::Auto;
        return true;
    }
    if (text == "cbr") {
        mode = MediaRateControlMode::Cbr;
        return true;
    }
    if (text == "vbr") {
        mode = MediaRateControlMode::Vbr;
        return true;
    }
    if (text == "cq") {
        mode = MediaRateControlMode::Cq;
        return true;
    }
    return false;
}

struct MediaFrameRateParameters {
    std::optional<int> numerator;
    std::optional<int> denominator;

    bool specified() const noexcept
    {
        return numerator.has_value() || denominator.has_value();
    }

    bool complete() const noexcept
    {
        return numerator.has_value() == denominator.has_value();
    }
};

struct MediaVideoTranscodeParameters {
    std::string codecName;
    std::string encoderName;
    std::optional<int> width;
    std::optional<int> height;
    MediaFrameRateParameters frameRate;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> quality;
    std::string preset;
    std::string tune;
    std::string profile;
    std::string level;
    std::optional<int> gop;
    std::optional<int> bFrames;
};

struct MediaAudioTranscodeParameters {
    bool transcode = false;
    std::string codecName;
    std::string encoderName;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> sampleRate;
    std::optional<int> channels;
    std::optional<int> quality;
    std::string preset;
    std::string profile;
};

struct MediaTranscodeExecutionParameters {
    bool includeVideo = true;
    bool includeAudio = true;
    bool disableHardware = false;
    bool diagnosticLogEnabled = true;
};

struct MediaGraphQueueParameters {
    std::size_t metadata = 1;
    std::size_t packet = 256;
    std::size_t frame = 128;
    std::size_t mux = 256;
};

struct MediaTranscodeParameterSet {
    MediaTranscodeExecutionParameters execution;
    MediaVideoTranscodeParameters video;
    MediaAudioTranscodeParameters audio;
    MediaGraphQueueParameters queues;
};

namespace MediaTranscodeOptionKey {
inline constexpr char PlannedEncoder[] = "encoder";
inline constexpr char PlannedDecoder[] = "decoder";
inline constexpr char PlannedFilter[] = "filter";

inline constexpr char VideoCodec[] = "video.codec";
inline constexpr char VideoEncoder[] = "video.encoder";
inline constexpr char VideoWidth[] = "video.width";
inline constexpr char VideoHeight[] = "video.height";
inline constexpr char VideoFpsNum[] = "video.fps.num";
inline constexpr char VideoFpsDen[] = "video.fps.den";
inline constexpr char VideoRateControl[] = "video.rc";
inline constexpr char VideoBitrateKbps[] = "video.bitrate.kbps";
inline constexpr char VideoMinBitrateKbps[] = "video.bitrate.min_kbps";
inline constexpr char VideoMaxBitrateKbps[] = "video.bitrate.max_kbps";
inline constexpr char VideoQuality[] = "video.quality";
inline constexpr char VideoPreset[] = "video.preset";
inline constexpr char VideoTune[] = "video.tune";
inline constexpr char VideoProfile[] = "video.profile";
inline constexpr char VideoLevel[] = "video.level";
inline constexpr char VideoGop[] = "video.gop";
inline constexpr char VideoBFrames[] = "video.bframes";

inline constexpr char AudioCodec[] = "audio.codec";
inline constexpr char AudioEncoder[] = "audio.encoder";
inline constexpr char AudioRateControl[] = "audio.rc";
inline constexpr char AudioBitrateKbps[] = "audio.bitrate.kbps";
inline constexpr char AudioMinBitrateKbps[] = "audio.bitrate.min_kbps";
inline constexpr char AudioMaxBitrateKbps[] = "audio.bitrate.max_kbps";
inline constexpr char AudioSampleRate[] = "audio.sample_rate";
inline constexpr char AudioChannels[] = "audio.channels";
inline constexpr char AudioQuality[] = "audio.quality";
inline constexpr char AudioPreset[] = "audio.preset";
inline constexpr char AudioProfile[] = "audio.profile";
} // namespace MediaTranscodeOptionKey

} // namespace media::ffmpeg::graph
