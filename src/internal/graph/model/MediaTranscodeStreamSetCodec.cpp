#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"

namespace media::ffmpeg::graph {

::media::Result<std::string_view> MediaTranscodeStreamSetCodec::encode(
    MediaTranscodeStreamSet streamSet)
{
    switch (streamSet) {
    case MediaTranscodeStreamSet::AudioVideo:
        return ::media::Result<std::string_view>::success("audio_video");
    case MediaTranscodeStreamSet::VideoOnly:
        return ::media::Result<std::string_view>::success("video_only");
    }
    return ::media::Result<std::string_view>::failure(
        ::media::ErrorInfo::invalidArgument(
            "stream set codec rejects an unknown enum value"));
}

::media::Result<MediaTranscodeStreamSet> MediaTranscodeStreamSetCodec::decode(
    std::string_view value)
{
    if (value == "audio_video") {
        return ::media::Result<MediaTranscodeStreamSet>::success(
            MediaTranscodeStreamSet::AudioVideo);
    }
    if (value == "video_only") {
        return ::media::Result<MediaTranscodeStreamSet>::success(
            MediaTranscodeStreamSet::VideoOnly);
    }
    return ::media::Result<MediaTranscodeStreamSet>::failure(
        ::media::ErrorInfo::invalidArgument(
            "stream set codec requires an explicit known value"));
}

} // namespace media::ffmpeg::graph
