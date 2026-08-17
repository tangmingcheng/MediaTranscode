#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"
#include "media_transcode/Result.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaRtpIngressStorage;

class MediaRtpIngressAdapter {
public:
    virtual ~MediaRtpIngressAdapter() = default;

    virtual MediaRtpIngressAdapterKind kind() const noexcept = 0;
    virtual ::media::Result<std::size_t> receive(
        MediaRtpIngressStorage& storage,
        int timeoutMilliseconds) = 0;
    virtual ::media::Status interruptReceive() noexcept = 0;
    virtual ::media::Status stop() noexcept = 0;
    virtual ::media::Status abort() noexcept = 0;
};

} // namespace media::ffmpeg::graph
