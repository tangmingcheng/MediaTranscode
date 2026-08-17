#pragma once

#include "internal/graph/model/MediaIpAddressFamily.h"
#include "media_transcode/Result.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaRawRtpBootstrapPlan final {
public:
    MediaRawRtpBootstrapPlan() = delete;

    static ::media::Result<MediaRawRtpBootstrapPlan> create(
        MediaIpAddressFamily addressFamily,
        std::size_t preparedInputByteBudget,
        int analyzeDurationMicroseconds);

    int socketReceiveBufferBytes() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t reorderWindowPackets() const noexcept;
    int maximumReorderDelayMilliseconds() const noexcept;
    ::media::Status validateProduct() const;

private:
    MediaRawRtpBootstrapPlan(
        int socketReceiveBufferBytes,
        std::size_t maximumDatagramBytes,
        std::size_t reorderWindowPackets,
        int maximumReorderDelayMilliseconds) noexcept;

    int m_socketReceiveBufferBytes;
    std::size_t m_maximumDatagramBytes;
    std::size_t m_reorderWindowPackets;
    int m_maximumReorderDelayMilliseconds;
};

} // namespace media::ffmpeg::graph
