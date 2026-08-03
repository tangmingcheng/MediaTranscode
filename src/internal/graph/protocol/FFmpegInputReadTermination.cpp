#include "internal/graph/protocol/FFmpegInputReadTermination.h"

extern "C" {
#include <libavutil/error.h>
}

#include <array>
#include <string>

namespace media::ffmpeg::graph {

FFmpegInputReadTermination::FFmpegInputReadTermination(
    FFmpegInputReadTerminationKind kind,
    std::optional<::media::ErrorInfo> error)
    : m_kind(kind), m_error(std::move(error))
{
}

FFmpegInputReadTermination FFmpegInputReadTermination::endOfStream()
{
    return FFmpegInputReadTermination(
        FFmpegInputReadTerminationKind::EndOfStream, std::nullopt);
}

FFmpegInputReadTermination FFmpegInputReadTermination::cancelled(
    ::media::ErrorInfo error)
{
    return FFmpegInputReadTermination(
        FFmpegInputReadTerminationKind::Cancelled, std::move(error));
}

FFmpegInputReadTermination FFmpegInputReadTermination::sourceLost(
    ::media::ErrorInfo error)
{
    return FFmpegInputReadTermination(
        FFmpegInputReadTerminationKind::SourceLost, std::move(error));
}

FFmpegInputReadTerminationKind FFmpegInputReadTermination::kind() const noexcept
{
    return m_kind;
}

const std::optional<::media::ErrorInfo>&
FFmpegInputReadTermination::error() const noexcept
{
    return m_error;
}

FFmpegInputReadTermination classifyFFmpegInputReadTermination(
    int ffmpegCode,
    bool interruptRequested,
    std::string_view operation)
{
    if (ffmpegCode == AVERROR_EOF) {
        return FFmpegInputReadTermination::endOfStream();
    }

    if (interruptRequested) {
        return FFmpegInputReadTermination::cancelled(
            ::media::ErrorInfo::cancelled(
                std::string(operation) + " was cancelled"));
    }

    std::array<char, AV_ERROR_MAX_STRING_SIZE> nativeMessage{};
    const int descriptionStatus = av_strerror(
        ffmpegCode, nativeMessage.data(), nativeMessage.size());
    std::string message(operation);
    message += " lost its input source";
    if (descriptionStatus == 0) {
        message += ": ";
        message += nativeMessage.data();
    }
    return FFmpegInputReadTermination::sourceLost(
        ::media::ErrorInfo::ioFailure(std::move(message), ffmpegCode));
}

} // namespace media::ffmpeg::graph
