#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioPipelinePlannerOptions {
    MediaAudioPipelinePlannerOptions() = delete;

    explicit MediaAudioPipelinePlannerOptions(bool includeAudio) noexcept
        : includeAudio(includeAudio)
    {
    }

    bool includeAudio;
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
    bool diagnosticLogEnabled = false;
};

struct MediaAudioPipelinePlan {
    bool enabled = false;
    MediaBranchMode branchMode = MediaBranchMode::Drop;
    int sourceStreamIndex = invalidMediaStreamIndex;
    std::string sourceCodecName;
    std::string targetCodecName;
    std::string targetEncoderName;
    bool followsSourceParameters = false;
    bool monotonicPacketTimestamps = false;
    std::string reason;
};

struct MediaInputAudioStreamInfo {
    int streamIndex = invalidMediaStreamIndex;
    std::string codecName;
    int sampleRate = 0;
    int channels = 0;
    int64_t bitrateBitsPerSecond = 0;
};

class MediaAudioPipelinePlanner final {
public:
    static ::media::Result<MediaAudioPipelinePlan> planFileAudio(
        const std::string& inputPath,
        const MediaAudioPipelinePlannerOptions& options);

    static ::media::Result<MediaAudioPipelinePlan> planKnownAudioTranscode(
        MediaInputAudioStreamInfo inputInfo,
        const MediaAudioPipelinePlannerOptions& options);

private:
    MediaAudioPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
