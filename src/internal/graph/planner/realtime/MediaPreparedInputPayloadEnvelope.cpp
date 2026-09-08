#include "internal/graph/planner/realtime/MediaPreparedInputPayloadEnvelope.h"

#include <algorithm>

namespace media::ffmpeg::graph {

bool MediaPreparedInputPayloadBound::valid() const noexcept
{
    return (streamKind == MediaStreamKind::Video ||
            streamKind == MediaStreamKind::Audio) &&
        maximumPayloadBytes > 0 && !authority.empty();
}

::media::Status MediaPreparedInputPayloadEnvelope::validate() const
{
    if (maximumPayloadsPerInputCompletion == 0 ||
        completionAuthority.empty() || streams.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "prepared input payload envelope is incomplete"));
    }
    for (std::size_t index = 0; index < streams.size(); ++index) {
        if (!streams[index].valid()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "prepared input payload bound is invalid"));
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (streams[previous].streamKind == streams[index].streamKind) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "prepared input payload envelope duplicates a stream"));
            }
        }
    }
    return ::media::Status::success();
}

const MediaPreparedInputPayloadBound*
MediaPreparedInputPayloadEnvelope::find(
    MediaStreamKind streamKind) const noexcept
{
    const auto found = std::find_if(
        streams.begin(), streams.end(), [streamKind](const auto& bound) {
            return bound.streamKind == streamKind;
        });
    return found == streams.end() ? nullptr : &*found;
}

} // namespace media::ffmpeg::graph
