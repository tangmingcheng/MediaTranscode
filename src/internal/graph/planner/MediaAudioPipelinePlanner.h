#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaAudioPipelineMode {
    Disabled,
    Copy,
    Encode
};

struct MediaAudioPipelinePlannerOptions {
    bool includeAudio = true;
    bool transformRequested = false;
    std::string requestedCodecName;
    std::string requestedEncoderName;
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
    MediaAudioPipelineMode mode = MediaAudioPipelineMode::Disabled;
    int sourceStreamIndex = invalidMediaStreamIndex;
    std::string sourceCodecName;
    std::string targetCodecName;
    std::string targetEncoderName;
    bool followsSourceParameters = false;
    std::string reason;
};

class MediaAudioPipelinePlanner final {
public:
    static ::media::Result<MediaAudioPipelinePlan> planFileAudio(
        const std::string& inputPath,
        const MediaAudioPipelinePlannerOptions& options);

private:
    MediaAudioPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
