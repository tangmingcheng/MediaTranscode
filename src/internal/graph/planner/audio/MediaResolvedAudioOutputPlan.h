#pragma once

#include "internal/graph/planner/audio/MediaResolvedAudioTargetDecision.h"
#include "internal/graph/planner/MediaPreparedAudioEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaSelectedAudioEncoder final {
    std::string name;
    std::string sampleFormat;
    std::vector<int> supportedSampleRates;
    std::vector<int> supportedProfileIds;
    int frameSizeSamples = 0;
    int delaySamples = 0;
    std::optional<int> bufferSizeKbits;
    std::optional<MediaPreparedAudioEncoderEmissionEnvelope> preparedEmission;
};

class MediaResolvedAudioOutputPlan final {
public:
    static ::media::Result<MediaResolvedAudioOutputPlan> create(
        const MediaResolvedAudioTargetDecision& target,
        const std::optional<MediaSelectedAudioEncoder>& selectedEncoder,
        std::optional<int> copiedAccessUnitSamples);

    const std::string& codecName() const noexcept;
    const MediaAudioProfile& profile() const noexcept;
    int sampleRate() const noexcept;
    int channels() const noexcept;
    const std::string& channelLayout() const noexcept;
    const std::string& sampleFormat() const noexcept;
    MediaBranchMode branchMode() const noexcept;
    const std::string& encoderName() const noexcept;
    MediaRateControlMode rateControl() const noexcept;
    const std::optional<int>& bitrateKbps() const noexcept;
    const std::optional<int>& minBitrateKbps() const noexcept;
    const std::optional<int>& maxBitrateKbps() const noexcept;
    const std::optional<int>& bufferSizeKbits() const noexcept;
    const std::optional<int>& quality() const noexcept;
    const std::string& preset() const noexcept;
    int codecFrameSamples() const noexcept;
    int encoderDelaySamples() const noexcept;

private:
    MediaResolvedAudioOutputPlan() = default;

    std::string m_codecName;
    MediaAudioProfile m_profile = MediaAudioProfile::unknown();
    int m_sampleRate = 0;
    int m_channels = 0;
    std::string m_channelLayout;
    std::string m_sampleFormat;
    MediaBranchMode m_branchMode = MediaBranchMode::Drop;
    std::string m_encoderName;
    MediaRateControlMode m_rateControl = MediaRateControlMode::Auto;
    std::optional<int> m_bitrateKbps;
    std::optional<int> m_minBitrateKbps;
    std::optional<int> m_maxBitrateKbps;
    std::optional<int> m_bufferSizeKbits;
    std::optional<int> m_quality;
    std::string m_preset;
    int m_codecFrameSamples = 0;
    int m_encoderDelaySamples = 0;
};

} // namespace media::ffmpeg::graph
