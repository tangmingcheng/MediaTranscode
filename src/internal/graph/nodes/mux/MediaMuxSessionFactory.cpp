#include "internal/graph/nodes/mux/MediaMuxSessionFactory.h"

#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/mux/FFmpegFileMuxSession.h"
#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaMuxSessionKind> requiredKind(const MediaNodeOptions& options)
{
    auto value = requiredNodeOption(
        &options, "MediaMuxSessionFactory", MediaTranscodeOptionKey::MuxSessionKind);
    if (!value) {
        return ::media::Result<MediaMuxSessionKind>::failure(
            value.error());
    }
    return parseMediaMuxSessionKindOption(value.value());
}

} // namespace

::media::Result<std::unique_ptr<MediaMuxSession>> ExplicitMediaMuxSessionFactory::create(
    const MediaNodeOptions& options) const
{
    auto kind = requiredKind(options);
    if (!kind) {
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(kind.error());
    }
    switch (kind.value()) {
    case MediaMuxSessionKind::FFmpegFile: {
        auto video = requiredBoolNodeOption(
            &options, "MediaMuxSessionFactory", MediaTranscodeOptionKey::MuxExpectVideo);
        if (!video) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(video.error());
        }
        auto audio = requiredBoolNodeOption(
            &options, "MediaMuxSessionFactory", MediaTranscodeOptionKey::MuxExpectAudio);
        if (!audio) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(audio.error());
        }
        if (!video.value() && !audio.value()) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "FFmpeg file mux session requires at least one stream"));
        }
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::success(
            std::make_unique<FFmpegFileMuxSession>(video.value(), audio.value()));
    }
    case MediaMuxSessionKind::ProjectMpegTs: {
        auto video = requiredBoolNodeOption(
            &options, "MediaMuxSessionFactory", MediaTranscodeOptionKey::MuxExpectVideo);
        if (!video) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(video.error());
        }
        auto audio = requiredBoolNodeOption(
            &options, "MediaMuxSessionFactory", MediaTranscodeOptionKey::MuxExpectAudio);
        if (!audio) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(audio.error());
        }
        if (!video.value() || !audio.value()) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "project MPEG-TS mux session requires planned video and audio"));
        }
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::success(
            std::make_unique<ProjectMpegTsMuxSessionAdapter>());
    }
    }
    return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
        ::media::ErrorInfo::unsupported("MediaMuxSessionFactory unsupported session kind"));
}

} // namespace media::ffmpeg::graph
