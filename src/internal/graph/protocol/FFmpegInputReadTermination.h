#pragma once

#include "media_transcode/Result.h"

#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

enum class FFmpegInputReadTerminationKind {
    EndOfStream,
    Cancelled,
    SourceLost
};

class FFmpegInputReadTermination final {
public:
    static FFmpegInputReadTermination endOfStream();
    static FFmpegInputReadTermination cancelled(::media::ErrorInfo error);
    static FFmpegInputReadTermination sourceLost(::media::ErrorInfo error);

    FFmpegInputReadTerminationKind kind() const noexcept;
    const std::optional<::media::ErrorInfo>& error() const noexcept;

private:
    FFmpegInputReadTermination(
        FFmpegInputReadTerminationKind kind,
        std::optional<::media::ErrorInfo> error);

    FFmpegInputReadTerminationKind m_kind;
    std::optional<::media::ErrorInfo> m_error;
};

FFmpegInputReadTermination classifyFFmpegInputReadTermination(
    int ffmpegCode,
    bool interruptRequested,
    std::string_view operation);

} // namespace media::ffmpeg::graph
