#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class FFmpegCodecParametersSnapshot final {
public:
    FFmpegCodecParametersSnapshot() = default;
    FFmpegCodecParametersSnapshot(FFmpegCodecParametersSnapshot&&) noexcept = default;
    FFmpegCodecParametersSnapshot& operator=(FFmpegCodecParametersSnapshot&&) noexcept = default;

    FFmpegCodecParametersSnapshot(const FFmpegCodecParametersSnapshot&) = delete;
    FFmpegCodecParametersSnapshot& operator=(const FFmpegCodecParametersSnapshot&) = delete;

    static ::media::Result<FFmpegCodecParametersSnapshot> takeOwnership(
        ::media::ffmpeg::CodecParametersPtr parameters);

    bool complete() const noexcept;
    ::media::Result<::media::ffmpeg::CodecParametersPtr> cloneCodecParameters() const;

private:
    explicit FFmpegCodecParametersSnapshot(::media::ffmpeg::CodecParametersPtr parameters) noexcept;

    ::media::ffmpeg::CodecParametersPtr m_parameters;
};

} // namespace media::ffmpeg::graph
