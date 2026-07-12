#include "internal/graph/runtime/buffer/FFmpegCodecParametersSnapshot.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

namespace media::ffmpeg::graph {

FFmpegCodecParametersSnapshot::FFmpegCodecParametersSnapshot(
    ::media::ffmpeg::CodecParametersPtr parameters) noexcept
    : m_parameters(std::move(parameters))
{
}

::media::Result<FFmpegCodecParametersSnapshot> FFmpegCodecParametersSnapshot::takeOwnership(
    ::media::ffmpeg::CodecParametersPtr parameters)
{
    if (!parameters) {
        return ::media::Result<FFmpegCodecParametersSnapshot>::failure(
            ::media::ErrorInfo::invalidArgument("codec parameter snapshot requires owned parameters"));
    }
    return ::media::Result<FFmpegCodecParametersSnapshot>::success(
        FFmpegCodecParametersSnapshot(std::move(parameters)));
}

bool FFmpegCodecParametersSnapshot::complete() const noexcept
{
    return m_parameters != nullptr;
}

::media::Result<::media::ffmpeg::CodecParametersPtr>
FFmpegCodecParametersSnapshot::cloneCodecParameters() const
{
    if (!m_parameters) {
        return ::media::Result<::media::ffmpeg::CodecParametersPtr>::failure(
            ::media::ErrorInfo::notInitialized("codec parameter snapshot is empty"));
    }
    auto clone = ::media::ffmpeg::makeCodecParameters();
    if (!clone) {
        return ::media::Result<::media::ffmpeg::CodecParametersPtr>::failure(
            ::media::ErrorInfo::allocationFailed("codec parameter snapshot clone allocation failed"));
    }
    const int copyResult = avcodec_parameters_copy(clone.get(), m_parameters.get());
    if (copyResult < 0) {
        return ::media::Result<::media::ffmpeg::CodecParametersPtr>::failure(
            FFmpegGraphError::fromCode(copyResult, "codec parameter snapshot avcodec_parameters_copy"));
    }
    return ::media::Result<::media::ffmpeg::CodecParametersPtr>::success(std::move(clone));
}

} // namespace media::ffmpeg::graph
