#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaAudioLineageExecutionMode {
    LegacyPlainPacket,
    SynchronizedReleasedAudio
};

inline constexpr std::string_view mediaAudioLineageExecutionModeName(
    MediaAudioLineageExecutionMode mode) noexcept
{
    switch (mode) {
    case MediaAudioLineageExecutionMode::LegacyPlainPacket: return "legacy_plain_packet";
    case MediaAudioLineageExecutionMode::SynchronizedReleasedAudio: return "synchronized_released_audio";
    }
    return {};
}

inline constexpr std::string_view MediaAudioLineageModeOptionKey =
    "audio.lineage.mode";

} // namespace media::ffmpeg::graph
