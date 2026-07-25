#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpMuxStreamConfig.h"
#include "internal/graph/protocol/rtp/MediaRtpTimestamp.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

struct AVPacket;

namespace media::ffmpeg::graph {

using ScheduledRtpRewrittenDatagramSink = std::function<::media::Status(
    std::span<const std::uint8_t>, std::size_t)>;

class ScheduledRtpPacketizerSession {
public:
    virtual ~ScheduledRtpPacketizerSession() = default;

    virtual ::media::Status open() = 0;
    virtual ::media::Status writeAccessUnit(
        const AVPacket& packet,
        MediaRtpTimestamp timestamp) = 0;
};

class ScheduledRtpPacketizerFactory {
public:
    virtual ~ScheduledRtpPacketizerFactory() = default;

    virtual ::media::Result<std::unique_ptr<ScheduledRtpPacketizerSession>> create(
        ScheduledRtpMuxStreamConfig config,
        ScheduledRtpRewrittenDatagramSink sink) = 0;
};

} // namespace media::ffmpeg::graph
