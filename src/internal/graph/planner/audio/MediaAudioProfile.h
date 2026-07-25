#pragma once

#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaAudioProfileKnowledge {
    NotApplicable,
    Known,
    Unknown
};

class MediaAudioProfile final {
public:
    static MediaAudioProfile notApplicable();
    static MediaAudioProfile unknown();
    static MediaAudioProfile knownAacLow();
    static ::media::Result<MediaAudioProfile> fromCodecProfile(
        const std::string& codecName,
        const std::string& profileName);

    MediaAudioProfileKnowledge knowledge() const noexcept;
    const std::string& canonicalName() const noexcept;
    int ffmpegProfileId() const noexcept;

    bool operator==(const MediaAudioProfile& other) const noexcept = default;

private:
    MediaAudioProfile(MediaAudioProfileKnowledge knowledge,
                      std::string canonicalName,
                      int ffmpegProfileId);

    MediaAudioProfileKnowledge m_knowledge;
    std::string m_canonicalName;
    int m_ffmpegProfileId;
};

} // namespace media::ffmpeg::graph
