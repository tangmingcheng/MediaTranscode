#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

inline constexpr std::string_view MediaAudioDecodeLineageIdentity =
    "audio_decoder_lineage_registry";
inline constexpr std::string_view MediaAudioStartupTrimLineageIdentity =
    "audio_startup_trim_lineage_registry";
inline constexpr std::string_view MediaAudioResampleLineageIdentity =
    "audio_resampler_lineage_registry";
inline constexpr std::string_view MediaAudioEncodeLineageIdentity =
    "audio_encoder_lineage_registry";

} // namespace media::ffmpeg::graph
