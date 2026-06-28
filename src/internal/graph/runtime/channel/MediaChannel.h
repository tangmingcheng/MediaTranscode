#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/channel/MediaChannelBinding.h"
#include "internal/graph/runtime/channel/MediaChannelId.h"
#include "internal/graph/runtime/channel/MediaChannelMetrics.h"
#include "internal/graph/runtime/queue/MediaQueue.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaChannel final {
public:
    MediaChannel(MediaChannelId id, const MediaEdge& edge);

    MediaChannel(const MediaChannel&) = delete;
    MediaChannel& operator=(const MediaChannel&) = delete;

    MediaChannelId id() const noexcept;
    MediaEdgeId edgeId() const noexcept;
    const MediaChannelBinding& binding() const noexcept;

    ::media::Status push(MediaBufferRef buffer);
    bool tryPush(MediaBufferRef buffer);
    ::media::Status pop(MediaBufferRef& out);
    bool tryPop(MediaBufferRef& out);

    void close();
    void abort();
    void clear();

    bool closed() const;
    bool aborted() const;
    std::size_t size() const;
    std::size_t capacity() const;

    const MediaEdgePolicy& policy() const noexcept;
    const MediaFormatDescriptor& formatDescriptor() const noexcept;
    const MediaTimeDescriptor& timeDescriptor() const noexcept;
    const MediaHardwareDescriptor& hardwareDescriptor() const noexcept;
    const MediaChannelMetrics& metrics() const noexcept;

private:
    void refreshQueueMetrics();

private:
    MediaChannelId m_id;
    MediaEdgeId m_edgeId;
    MediaChannelBinding m_binding;
    MediaEdgePolicy m_policy;
    MediaFormatDescriptor m_format;
    MediaTimeDescriptor m_time;
    MediaHardwareDescriptor m_hardware;
    std::unique_ptr<MediaQueue> m_queue;
    MediaChannelMetrics m_metrics;
};

} // namespace media::ffmpeg::graph
