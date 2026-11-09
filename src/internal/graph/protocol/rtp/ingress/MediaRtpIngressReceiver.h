#pragma once

#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapter.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressBatch.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"

#include "internal/graph/runtime/storage/MediaLineageUniquePtr.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRtpIngressReceiver final {
public:
    MediaRtpIngressReceiver() = delete;
    ~MediaRtpIngressReceiver() = default;

    MediaRtpIngressReceiver(const MediaRtpIngressReceiver&) = delete;
    MediaRtpIngressReceiver& operator=(const MediaRtpIngressReceiver&) = delete;
    MediaRtpIngressReceiver(MediaRtpIngressReceiver&&) noexcept = default;
    MediaRtpIngressReceiver& operator=(MediaRtpIngressReceiver&&) noexcept = default;

    static ::media::Result<MediaRtpIngressReceiver> create(
        const MediaRtpReceiveGeometry& plan,
        std::shared_ptr<MediaRtpIngressAdapter> adapter,
        MediaLineageAllocationScope allocation);

    ::media::Result<MediaRtpIngressBatch> receiveNext(
        int timeoutMilliseconds);
    ::media::Status waitForBatchRelease(std::stop_token stopToken);
    ::media::Status interruptReceive() noexcept;
    ::media::Status stop() noexcept;
    ::media::Status abort() noexcept;

private:
    MediaRtpIngressReceiver(
        MediaRtpIngressStorage storage,
        std::shared_ptr<MediaRtpIngressAdapter> adapter) noexcept;

    MediaRtpIngressStorage m_storage;
    std::shared_ptr<MediaRtpIngressAdapter> m_adapter;
};

} // namespace media::ffmpeg::graph
