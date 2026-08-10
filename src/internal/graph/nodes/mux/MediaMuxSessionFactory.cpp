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

ExplicitMediaMuxSessionFactory::ExplicitMediaMuxSessionFactory(
    std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> outputAuthority)
    : m_generationState(std::move(generationState))
    , m_outputAuthority(std::move(outputAuthority))
{
}

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
        if (!m_generationState || !m_outputAuthority) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session requires its injected generation authority"));
        }
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
        const bool expectsVideoOnly =
            m_outputAuthority->streamSet() ==
            MediaTranscodeStreamSet::VideoOnly;
        if (!video.value() || audio.value() == expectsVideoOnly) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "project MPEG-TS mux session stream shape conflicts with its authority"));
        }
        auto generationSession = std::dynamic_pointer_cast<
            ProjectMpegTsGenerationSessionState>(
                m_generationState->sessionState());
        if (!generationSession) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "project MPEG-TS mux session requires its typed generation session"));
        }
        auto authority = ProjectMpegTsGenerationAuthority::create(
            m_generationState, std::move(generationSession),
            m_outputAuthority);
        if (!authority) {
            return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
                authority.error());
        }
        return ::media::Result<std::unique_ptr<MediaMuxSession>>::success(
            std::make_unique<ProjectMpegTsMuxSessionAdapter>(
                std::move(authority).value()));
    }
    }
    return ::media::Result<std::unique_ptr<MediaMuxSession>>::failure(
        ::media::ErrorInfo::unsupported("MediaMuxSessionFactory unsupported session kind"));
}

} // namespace media::ffmpeg::graph
