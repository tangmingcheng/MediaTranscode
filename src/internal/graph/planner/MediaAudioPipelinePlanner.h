#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/planner/audio/capability/MediaAudioDecoderCapabilityProvider.h"
#include "internal/graph/planner/audio/capability/MediaAudioResamplerCapabilityProvider.h"
#include "internal/graph/planner/MediaPreparedAudioEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioPipelinePlannerOptions {
    MediaAudioPipelinePlannerOptions() = delete;

    explicit MediaAudioPipelinePlannerOptions(MediaTranscodeStreamSet streamSet) noexcept
        : streamSet(streamSet)
    {
    }

    MediaTranscodeStreamSet streamSet;
    std::string requestedCodecName;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> requestedBitrateKbps;
    std::optional<int> requestedMinBitrateKbps;
    std::optional<int> requestedMaxBitrateKbps;
    std::optional<int> requestedBufferSizeKbits;
    std::optional<int> requestedSampleRate;
    std::optional<int> requestedChannels;
    std::optional<int> requestedQuality;
    std::string requestedPreset;
    std::string requestedProfile;
    MediaAudioOutputRequirement outputRequirement;
    bool diagnosticLogEnabled = false;
};

struct MediaAudioPipelinePlan {
    bool enabled = false;
    MediaBranchMode branchMode = MediaBranchMode::Drop;
    int sourceStreamIndex = invalidMediaStreamIndex;
    std::string sourceCodecName;
    std::optional<MediaResolvedAudioOutputPlan> resolvedOutput;
    bool monotonicPacketTimestamps = false;
    std::optional<int> maximumAccessUnitSamples;
    std::string reason;
    std::optional<MediaSelectedAudioDecoder> selectedDecoder;
    std::optional<MediaSelectedAudioResampler> selectedResampler;
    std::optional<MediaPreparedAudioEncoderEmissionEnvelope> preparedEmission;
};

struct MediaInputAudioStreamInfo {
    int streamIndex = invalidMediaStreamIndex;
    std::string codecName;
    int sampleRate = 0;
    int channels = 0;
    std::string channelLayout;
    std::string sampleFormat;
    MediaAudioProfile profile = MediaAudioProfile::unknown();
    int64_t bitrateBitsPerSecond = 0;
    std::optional<int> maximumAccessUnitSamples;
    std::optional<MediaSelectedAudioDecoder> selectedDecoder;
};

class MediaAudioPipelinePlanner final {
public:
    static ::media::Result<MediaAudioPipelinePlan> planFileAudio(
        const std::string& inputPath,
        const MediaAudioPipelinePlannerOptions& options);

    static ::media::Result<MediaAudioPipelinePlan> planKnownAudio(
        MediaInputAudioStreamInfo inputInfo,
        const MediaAudioPipelinePlannerOptions& options);

private:
    MediaAudioPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
