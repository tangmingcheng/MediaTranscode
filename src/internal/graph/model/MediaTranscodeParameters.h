#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaBranchMode {
    Drop,
    CopyPacket,
    TranscodeFrame
};

inline const char* mediaBranchModeName(MediaBranchMode mode) noexcept
{
    switch (mode) {
    case MediaBranchMode::Drop: return "drop";
    case MediaBranchMode::CopyPacket: return "copy_packet";
    case MediaBranchMode::TranscodeFrame: return "transcode_frame";
    }
    return "drop";
}

enum class MediaRateControlMode {
    Auto,
    Cbr,
    Vbr,
    Cvbr,
    Crf
};

inline const char* mediaRateControlModeName(MediaRateControlMode mode) noexcept
{
    switch (mode) {
    case MediaRateControlMode::Auto: return "auto";
    case MediaRateControlMode::Cbr: return "cbr";
    case MediaRateControlMode::Vbr: return "vbr";
    case MediaRateControlMode::Cvbr: return "cvbr";
    case MediaRateControlMode::Crf: return "crf";
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
    if (text == "cvbr" || text == "capped_vbr" || text == "capped-vbr") {
        mode = MediaRateControlMode::Cvbr;
        return true;
    }
    if (text == "crf") {
        mode = MediaRateControlMode::Crf;
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
    std::optional<int> width;
    std::optional<int> height;
    MediaFrameRateParameters frameRate;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> bufferSizeKbits;
    std::optional<int> quality;
    std::string preset;
    std::string tune;
    std::string profile;
    std::string level;
    std::optional<int> gop;
    std::optional<int> bFrames;
    std::optional<bool> globalHeader;

    bool resizeRequested() const noexcept
    {
        return width.has_value() && height.has_value();
    }
};

struct MediaAudioTranscodeParameters {
    std::string codecName;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> bufferSizeKbits;
    std::optional<int> sampleRate;
    std::optional<int> channels;
    std::optional<int> quality;
    std::string preset;
    std::string profile;
};

struct MediaTranscodeExecutionParameters {
    bool includeAudio = true;
    bool disableHardware = false;
    bool diagnosticLogEnabled = true;
};

struct MediaGraphQueueParameters {
    std::size_t metadata = 0;
    std::size_t packet = 0;
    std::size_t frame = 0;
    std::size_t mux = 0;
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

inline constexpr char MuxExpectVideo[] = "mux.expect_video";
inline constexpr char MuxExpectAudio[] = "mux.expect_audio";

inline constexpr char PacketSourceStreamIndex[] = "packet.source_stream_index";
inline constexpr char PacketStreamKind[] = "packet.stream_kind";

inline constexpr char VideoCodec[] = "video.codec";
inline constexpr char VideoWidth[] = "video.width";
inline constexpr char VideoHeight[] = "video.height";
inline constexpr char VideoFpsNum[] = "video.fps.num";
inline constexpr char VideoFpsDen[] = "video.fps.den";
inline constexpr char VideoSynthesizeMissingTimestamps[] = "video.timestamp.synthesize_missing";
inline constexpr char VideoRateControl[] = "video.rc";
inline constexpr char VideoBitrateKbps[] = "video.bitrate.kbps";
inline constexpr char VideoMinBitrateKbps[] = "video.bitrate.min_kbps";
inline constexpr char VideoMaxBitrateKbps[] = "video.bitrate.max_kbps";
inline constexpr char VideoBufferSizeKbits[] = "video.rc.buffer_size.kbits";
inline constexpr char VideoQuality[] = "video.quality";
inline constexpr char VideoPreset[] = "video.preset";
inline constexpr char VideoTune[] = "video.tune";
inline constexpr char VideoProfile[] = "video.profile";
inline constexpr char VideoLevel[] = "video.level";
inline constexpr char VideoGop[] = "video.gop";
inline constexpr char VideoBFrames[] = "video.bframes";
inline constexpr char VideoGlobalHeader[] = "video.global_header";

inline constexpr char AudioSourceStreamIndex[] = "audio.source_stream_index";
inline constexpr char AudioCodec[] = "audio.codec";
inline constexpr char AudioRateControl[] = "audio.rc";
inline constexpr char AudioBitrateKbps[] = "audio.bitrate.kbps";
inline constexpr char AudioMinBitrateKbps[] = "audio.bitrate.min_kbps";
inline constexpr char AudioMaxBitrateKbps[] = "audio.bitrate.max_kbps";
inline constexpr char AudioBufferSizeKbits[] = "audio.rc.buffer_size.kbits";
inline constexpr char AudioSampleRate[] = "audio.sample_rate";
inline constexpr char AudioChannels[] = "audio.channels";
inline constexpr char AudioQuality[] = "audio.quality";
inline constexpr char AudioPreset[] = "audio.preset";
inline constexpr char AudioProfile[] = "audio.profile";
} // namespace MediaTranscodeOptionKey

} // namespace media::ffmpeg::graph
