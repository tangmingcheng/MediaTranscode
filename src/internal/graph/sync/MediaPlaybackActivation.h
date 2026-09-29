#pragma once

#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

// An activation is one validated value; an absent audio origin is only valid
// for a planned video-only domain, never a runtime fallback from A/V.
class MediaPlaybackActivation final {
public:
    static ::media::Result<MediaPlaybackActivation> create(
        MediaTranscodeStreamSet members, MediaPlaybackEpoch epoch,
        std::optional<MediaAudioPlaybackOrigin> audioOrigin)
    {
        using Result = ::media::Result<MediaPlaybackActivation>;
        if ((members != MediaTranscodeStreamSet::VideoOnly &&
             members != MediaTranscodeStreamSet::AudioVideo) ||
            epoch.generation == 0 ||
            audioOrigin.has_value() != (members == MediaTranscodeStreamSet::AudioVideo) ||
            (audioOrigin && (audioOrigin->generation != epoch.generation ||
                audioOrigin->sourceStart != epoch.sourceStart ||
                audioOrigin->masterRelease != epoch.masterRelease ||
                audioOrigin->epochOutputSampleIndex < 0 || audioOrigin->outputSampleRate <= 0)))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Playback activation requires the exact planned members and coherent origins"));
        return Result::success(MediaPlaybackActivation(members, epoch, audioOrigin));
    }

    MediaTranscodeStreamSet members() const noexcept { return m_members; }
    const MediaPlaybackEpoch& epoch() const noexcept { return m_epoch; }
    const std::optional<MediaAudioPlaybackOrigin>& audioOrigin() const noexcept
    { return m_audioOrigin; }
    MediaPlaybackActivation reanchor(MediaRunningTime masterRelease) const noexcept
    {
        auto anchored = *this;
        anchored.m_epoch.masterRelease = masterRelease;
        if (anchored.m_audioOrigin) anchored.m_audioOrigin->masterRelease = masterRelease;
        return anchored;
    }
    bool operator==(const MediaPlaybackActivation&) const = default;

private:
    MediaPlaybackActivation(MediaTranscodeStreamSet members, MediaPlaybackEpoch epoch,
                            std::optional<MediaAudioPlaybackOrigin> audioOrigin)
        : m_members(members), m_epoch(epoch), m_audioOrigin(audioOrigin) {}
    MediaTranscodeStreamSet m_members;
    MediaPlaybackEpoch m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
};

} // namespace media::ffmpeg::graph
