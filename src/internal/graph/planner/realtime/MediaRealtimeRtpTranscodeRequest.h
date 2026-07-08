#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/RealtimeStreamLayout.h"

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
    std::string fmtp;
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
};

struct MediaRealtimeOutputConfig {
    std::optional<RealtimeOutputStreamLayout> streamLayout;
    std::string host;
    std::optional<std::size_t> basePort;
    std::string sdpPath;
    std::string url;
    std::optional<int> packetSize;
};

struct MediaRealtimeRtpTranscodeRequest {
    MediaRealtimeInputConfig input;
    MediaRealtimeOutputConfig output;
    MediaTranscodeParameterSet parameters;
    std::string mediaId;
};

} // namespace media::ffmpeg::graph
