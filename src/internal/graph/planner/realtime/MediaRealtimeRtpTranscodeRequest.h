#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputMetadata {
    std::string url;
    std::string codecName;
    std::optional<int> payloadType;
    std::optional<int> clockRate;
    std::optional<int> channels;
    std::optional<int> bitrateKbps;
    std::optional<std::string> fmtp;
};

struct MediaRealtimeMpegTsInputClockPolicy {
    std::optional<MediaRunningTime> maximumPcrGap;
};

struct MediaRealtimeInputConfig {
    std::optional<RealtimeInputType> type;
    std::optional<RealtimeInputStreamLayout> streamLayout;
    std::string url;
    std::string rtspTransport;
    std::optional<int> openTimeoutMs;
    std::optional<int> readTimeoutMs;
    std::optional<int> analyzeDurationUs;
    std::optional<int> probeSizeBytes;
    std::optional<bool> lowLatency;
    std::optional<int> videoStreamIndex;
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

struct MediaRealtimeAvSyncStartupConfig {
    std::optional<std::size_t> maximumVideoUnitBytes;
    std::optional<std::size_t> maximumAudioUnitBytes;
    std::optional<MediaRunningTime> maximumGap;
};

struct MediaRealtimePreparedHandoffConfig {
    std::optional<std::size_t> videoPacketCapacity;
    std::optional<std::size_t> audioPacketCapacity;
    std::optional<std::size_t> videoByteCapacity;
    std::optional<std::size_t> audioByteCapacity;
};

struct MediaRealtimeRtpTranscodeRequest {
    MediaRealtimeInputConfig input;
    MediaRealtimeOutputConfig output;
    std::optional<MediaRealtimeDeploymentEnvelope> deployment;
    MediaTranscodeParameterSet parameters;
    MediaRealtimeAvSyncStartupConfig avSyncStartup;
    MediaRealtimePreparedHandoffConfig preparedHandoff;
    std::string mediaId;
};

} // namespace media::ffmpeg::graph
