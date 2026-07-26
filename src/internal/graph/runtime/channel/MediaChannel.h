#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/channel/MediaChannelBinding.h"
#include "internal/graph/runtime/channel/MediaChannelId.h"
#include "internal/graph/runtime/channel/MediaChannelMetrics.h"
#include "internal/graph/runtime/queue/MediaQueue.h"
#include "media_transcode/Result.h"

#include <memory>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;
class MediaAtomicOutputTransaction;
class MediaReservedOutputTransaction;
struct MediaChannelAtomicOutputTestAccess;

class MediaChannel final {
public:
    MediaChannel(MediaChannelId id, const MediaEdge& edge);

    MediaChannel(const MediaChannel&) = delete;
    MediaChannel& operator=(const MediaChannel&) = delete;

    MediaChannelId id() const noexcept;
    MediaEdgeId edgeId() const noexcept;
    const MediaChannelBinding& binding() const noexcept;

    ::media::Status push(MediaBufferRef buffer);
    MediaQueuePushOutcome pushOutcome(MediaBufferRef buffer);
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
    void setConsumerWakeup(MediaNodeWakeup& wakeup) noexcept;
    void setProducerWakeup(MediaNodeWakeup& wakeup) noexcept;

private:
    friend class MediaAtomicOutputTransaction;
    friend class MediaReservedOutputTransaction;
    friend struct MediaChannelAtomicOutputTestAccess;

    MediaQueuePushOutcome pushOutcomeLocked(
        MediaBufferRef buffer,
        bool publishAccepted = true);
    MediaQueuePushOutcome pushReservedOutcomeLocked(MediaBufferRef buffer);
    void publishAcceptedMutation() noexcept;
    void publishReservedCapacityMutation() noexcept;
    void finalizeDeferredCloseLocked() noexcept;
    void signalMutationWaiters() noexcept;
    void refreshQueueMetrics() noexcept;

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
    MediaNodeWakeup* m_consumerWakeup = nullptr;
    MediaNodeWakeup* m_producerWakeup = nullptr;
    mutable std::mutex m_mutationMutex;
    std::condition_variable m_mutationChanged;
    std::condition_variable m_waitStateChanged;
    std::atomic_uint64_t m_mutationSequence{0};
    std::atomic_uint64_t m_externalBlockedPushes{0};
    std::atomic_size_t m_externalBlockedProducers{0};
    std::atomic_size_t m_externalBlockedConsumers{0};
    std::size_t m_reservedCapacity = 0;
    std::size_t m_authorizedCapacity = 0;
    bool m_closeRequested = false;
};

} // namespace media::ffmpeg::graph
