#pragma once

#include "media_transcode/Result.h"
#include <string_view>

namespace media::ffmpeg::graph {

enum class MediaVideoStartupReadiness { IntervalCoverage, DecodedPresentationSpan };

inline ::media::Result<std::string_view> encodeMediaVideoStartupReadiness(
    MediaVideoStartupReadiness value)
{
    using R = ::media::Result<std::string_view>;
    switch (value) {
    case MediaVideoStartupReadiness::IntervalCoverage: return R::success("interval_coverage");
    case MediaVideoStartupReadiness::DecodedPresentationSpan: return R::success("decoded_presentation_span");
    }
    return R::failure(::media::ErrorInfo::invalidArgument("Unknown video startup readiness"));
}

inline ::media::Result<MediaVideoStartupReadiness> decodeMediaVideoStartupReadiness(
    std::string_view value)
{
    using R = ::media::Result<MediaVideoStartupReadiness>;
    if (value == "interval_coverage") return R::success(MediaVideoStartupReadiness::IntervalCoverage);
    if (value == "decoded_presentation_span") return R::success(MediaVideoStartupReadiness::DecodedPresentationSpan);
    return R::failure(::media::ErrorInfo::invalidArgument("Unknown video startup readiness"));
}

} // namespace media::ffmpeg::graph
