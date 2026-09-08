#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputMetadata {
    std::string url;
    std::string codecName;
    std::optional<int> payloadType;
    std::optional<int> clockRate;
    std::optional<int> channels;
    std::optional<std::string> fmtp;
};

struct MediaRealtimeMpegTsInputClockPolicy {
    std::optional<MediaRunningTime> maximumPcrGap;
};

struct MediaRealtimeInputConfig {
    std::optional<RealtimeInputType> type;
    std::string url;
    std::string rtspTransport;
    std::optional<int> openTimeoutMs;
    std::optional<int> readTimeoutMs;
    std::optional<int> analyzeDurationUs;
    std::optional<int> probeSizeBytes;
    MediaRealtimeRtpInputMetadata videoRtp;
    MediaRealtimeRtpInputMetadata audioRtp;
    MediaRealtimeMpegTsInputClockPolicy mpegTsClock;
};

struct MediaRealtimeOutputConfig {
    std::optional<RealtimeOutputStreamLayout> streamLayout;
    std::optional<MediaOutputTransportKind> transport;
    std::string host;
    std::optional<std::size_t> basePort;
    std::string sdpPath;
    std::string url;
};

struct MediaRealtimeDeploymentConfig {
    std::optional<std::uint64_t> provisionedEgressCapacityBitsPerSecond;
    std::optional<MediaRunningTime> maximumWireResidence;
};

struct MediaRealtimeExecutionParameters {
    std::optional<MediaTranscodeStreamSet> streamSet;
    bool diagnosticLogEnabled = true;
};

struct MediaRealtimeVideoTranscodeParameters {
    std::string codecName;
    std::optional<int> width;
    std::optional<int> height;
    MediaFrameRateParameters frameRate;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> gop;

    bool resizeRequested() const noexcept
    {
        return width.has_value() && height.has_value();
    }
};

struct MediaRealtimeAudioTranscodeParameters {
    std::string codecName;
    MediaRateControlMode rateControl = MediaRateControlMode::Auto;
    std::optional<int> bitrateKbps;
    std::optional<int> minBitrateKbps;
    std::optional<int> maxBitrateKbps;
    std::optional<int> sampleRate;
    std::optional<int> channels;
};

struct MediaRealtimeTranscodeParameters {
    MediaRealtimeExecutionParameters execution;
    MediaRealtimeVideoTranscodeParameters video;
    MediaRealtimeAudioTranscodeParameters audio;
};

struct MediaRealtimeRtpTranscodeRequest {
    MediaRealtimeInputConfig input;
    MediaRealtimeOutputConfig output;
    MediaRealtimeDeploymentConfig deployment;
    MediaRealtimeTranscodeParameters parameters;
    std::string mediaId;
};

} // namespace media::ffmpeg::graph
