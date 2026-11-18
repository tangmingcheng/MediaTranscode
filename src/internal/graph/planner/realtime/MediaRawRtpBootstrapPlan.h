#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaRtpInputWaitPlan.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaRawRtpBootstrapPlan final {
public:
    MediaRawRtpBootstrapPlan() = delete;

    static ::media::Result<MediaRawRtpBootstrapPlan> create(
        MediaIpAddressFamily addressFamily,
        std::size_t preparedInputByteBudget,
        MediaRtpInputWaitPlan inputWait);

    int socketReceiveBufferBytes() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t reorderWindowPackets() const noexcept;
    int maximumReorderDelayMilliseconds() const noexcept;
    const MediaRtpInputWaitPlan& inputWait() const noexcept;
    ::media::Status validateProduct() const;

private:
    MediaRawRtpBootstrapPlan(
        int socketReceiveBufferBytes,
        std::size_t maximumDatagramBytes,
        std::size_t reorderWindowPackets,
        MediaRtpInputWaitPlan inputWait) noexcept;

    int m_socketReceiveBufferBytes;
    std::size_t m_maximumDatagramBytes;
    std::size_t m_reorderWindowPackets;
    MediaRtpInputWaitPlan m_inputWait;
};

} // namespace media::ffmpeg::graph
