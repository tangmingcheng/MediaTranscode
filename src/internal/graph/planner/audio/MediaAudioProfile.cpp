#include "internal/graph/planner/audio/MediaAudioProfile.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <algorithm>
#include <cctype>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string normalized(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    std::replace(value.begin(), value.end(), '-', '_');
    return value;
}

} // namespace

MediaAudioProfile::MediaAudioProfile(MediaAudioProfileKnowledge knowledge,
                                     std::string canonicalName,
                                     int ffmpegProfileId)
    : m_knowledge(knowledge),
      m_canonicalName(std::move(canonicalName)),
      m_ffmpegProfileId(ffmpegProfileId)
{
}

MediaAudioProfile MediaAudioProfile::notApplicable()
{
    return MediaAudioProfile(MediaAudioProfileKnowledge::NotApplicable, {}, AV_PROFILE_UNKNOWN);
}

MediaAudioProfile MediaAudioProfile::unknown()
{
    return MediaAudioProfile(MediaAudioProfileKnowledge::Unknown, {}, AV_PROFILE_UNKNOWN);
}

MediaAudioProfile MediaAudioProfile::knownAacLow()
{
    return MediaAudioProfile(MediaAudioProfileKnowledge::Known, "aac_low", AV_PROFILE_AAC_LOW);
}

::media::Result<MediaAudioProfile> MediaAudioProfile::fromCodecProfile(
    const std::string& codecName,
    const std::string& profileName)
{
    const std::string codec = canonicalCodecName(codecName);
    if (codec != "aac") {
        return ::media::Result<MediaAudioProfile>::success(notApplicable());
    }
    if (profileName.empty()) {
        return ::media::Result<MediaAudioProfile>::success(unknown());
    }
    const std::string profile = normalized(profileName);
    if (profile == "lc" || profile == "aac_lc" || profile == "aac_low") {
        return ::media::Result<MediaAudioProfile>::success(knownAacLow());
    }
    if (profile == "he" || profile == "he_aac" || profile == "aac_he") {
        return ::media::Result<MediaAudioProfile>::success(
            MediaAudioProfile(MediaAudioProfileKnowledge::Known, "aac_he", AV_PROFILE_AAC_HE));
    }
    if (profile == "he_v2" || profile == "he_aac_v2" || profile == "aac_he_v2") {
        return ::media::Result<MediaAudioProfile>::success(
            MediaAudioProfile(MediaAudioProfileKnowledge::Known, "aac_he_v2", AV_PROFILE_AAC_HE_V2));
    }
    return ::media::Result<MediaAudioProfile>::failure(
        ::media::ErrorInfo::unsupported("unsupported AAC profile: " + profileName));
}

MediaAudioProfileKnowledge MediaAudioProfile::knowledge() const noexcept { return m_knowledge; }
const std::string& MediaAudioProfile::canonicalName() const noexcept { return m_canonicalName; }
int MediaAudioProfile::ffmpegProfileId() const noexcept { return m_ffmpegProfileId; }

} // namespace media::ffmpeg::graph
