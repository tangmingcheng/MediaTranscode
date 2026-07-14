#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/audio/MediaAudioProfile.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioOutputRequirement final {
    std::optional<std::string> codecName;
    std::optional<MediaAudioProfile> profile;
    std::optional<int> sampleRate;
    std::optional<int> channels;
};

struct MediaResolvedAudioSource final {
    std::string codecName;
    MediaAudioProfile profile = MediaAudioProfile::unknown();
    int sampleRate = 0;
    int channels = 0;
    std::string channelLayout;
    std::string sampleFormat;
    std::int64_t bitrateBitsPerSecond = 0;
};

struct MediaResolvedAudioRequest final {
    std::string codecName;
    std::optional<MediaAudioProfile> profile;
    std::optional<int> sampleRate;
    std::optional<int> channels;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> bufferSizeKbits;
    std::optional<int> quality;
    std::string preset;
};

class MediaResolvedAudioTargetDecision final {
public:
    static ::media::Result<MediaResolvedAudioTargetDecision> create(
        const MediaResolvedAudioSource& source,
        const MediaResolvedAudioRequest& request,
        const MediaAudioOutputRequirement& topologyRequirement);

    const std::string& codecName() const noexcept;
    const MediaAudioProfile& profile() const noexcept;
    int sampleRate() const noexcept;
    int channels() const noexcept;
    const std::string& channelLayout() const noexcept;
    const std::string& sourceSampleFormat() const noexcept;
    MediaBranchMode branchMode() const noexcept;
    MediaRateControlMode rateControl() const noexcept;
    const std::optional<int>& bitrateKbps() const noexcept;
    const std::optional<int>& minBitrateKbps() const noexcept;
    const std::optional<int>& maxBitrateKbps() const noexcept;
    const std::optional<int>& bufferSizeKbits() const noexcept;
    const std::optional<int>& quality() const noexcept;
    const std::string& preset() const noexcept;

private:
    MediaResolvedAudioTargetDecision() = default;

    std::string m_codecName;
    MediaAudioProfile m_profile = MediaAudioProfile::unknown();
    int m_sampleRate = 0;
    int m_channels = 0;
    std::string m_channelLayout;
    std::string m_sourceSampleFormat;
    MediaBranchMode m_branchMode = MediaBranchMode::Drop;
    MediaRateControlMode m_rateControl = MediaRateControlMode::Auto;
    std::optional<int> m_bitrateKbps;
    std::optional<int> m_minBitrateKbps;
    std::optional<int> m_maxBitrateKbps;
    std::optional<int> m_bufferSizeKbits;
    std::optional<int> m_quality;
    std::string m_preset;
};

} // namespace media::ffmpeg::graph
