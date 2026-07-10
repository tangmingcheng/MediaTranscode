#include "internal/graph/runtime/channel/MediaChannel.h"

#include "internal/graph/runtime/queue/MediaQueueFactory.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

namespace media::ffmpeg::graph {

MediaChannel::MediaChannel(MediaChannelId id, const MediaEdge& edge)
    : m_id(id)
    , m_edgeId(edge.id)
    , m_policy(edge.policy)
    , m_format(edge.format)
    , m_time(edge.time)
    , m_hardware(edge.hardware)
    , m_queue(MediaQueueFactory::create(edge.policy.queuePolicy))
{
    m_binding.channelId = id;
    m_binding.edgeId = edge.id;
    m_binding.from = edge.from;
    m_binding.to = edge.to;
    m_binding.streamKind = edge.streamKind;
    m_binding.edgeKind = edge.edgeKind;
    m_binding.payloadKind = edge.payloadKind;
}

MediaChannelId MediaChannel::id() const noexcept
{
    return m_id;
}

MediaEdgeId MediaChannel::edgeId() const noexcept
{
    return m_edgeId;
}

const MediaChannelBinding& MediaChannel::binding() const noexcept
{
    return m_binding;
}

::media::Status MediaChannel::push(MediaBufferRef buffer)
{
    if (!m_queue) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaChannel push failed: queue is null"));
    }

    auto status = m_queue->push(std::move(buffer));
    if (status) {
        m_metrics.pushed++;
        if (m_consumerWakeup) {
            m_consumerWakeup->notify();
        }
    }
    refreshQueueMetrics();
    return status;
}

MediaQueuePushOutcome MediaChannel::pushOutcome(MediaBufferRef buffer)
{
    if (!m_queue) {
        return MediaQueuePushOutcome::Closed;
    }

    const MediaQueuePushOutcome outcome = m_queue->pushOutcome(std::move(buffer));
    if (outcome == MediaQueuePushOutcome::Accepted) {
        m_metrics.pushed++;
        if (m_consumerWakeup) {
            m_consumerWakeup->notify();
        }
    }
    refreshQueueMetrics();
    return outcome;
}

::media::Status MediaChannel::pop(MediaBufferRef& out)
{
    if (!m_queue) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaChannel pop failed: queue is null"));
    }

    auto status = m_queue->pop(out);
    if (status) {
        m_metrics.popped++;
        if (m_producerWakeup) {
            m_producerWakeup->notify();
        }
    }
    refreshQueueMetrics();
    return status;
}

bool MediaChannel::tryPop(MediaBufferRef& out)
{
    if (!m_queue) {
        return false;
    }

    const bool ok = m_queue->tryPop(out);
    if (ok) {
        m_metrics.popped++;
        if (m_producerWakeup) {
            m_producerWakeup->notify();
        }
    }
    refreshQueueMetrics();
    return ok;
}

void MediaChannel::close()
{
    if (m_queue) {
        m_queue->close();
    }
    m_metrics.closed++;
    if (m_consumerWakeup) {
        m_consumerWakeup->notify();
    }
    if (m_producerWakeup) {
        m_producerWakeup->notify();
    }
    refreshQueueMetrics();
}

void MediaChannel::abort()
{
    if (m_queue) {
        m_queue->abort();
    }
    m_metrics.aborted++;
    if (m_consumerWakeup) {
        m_consumerWakeup->notify();
    }
    if (m_producerWakeup) {
        m_producerWakeup->notify();
    }
    refreshQueueMetrics();
}

void MediaChannel::clear()
{
    if (m_queue) {
        m_queue->clear();
    }
    m_metrics.cleared++;
    if (m_consumerWakeup) {
        m_consumerWakeup->notify();
    }
    if (m_producerWakeup) {
        m_producerWakeup->notify();
    }
    refreshQueueMetrics();
}

bool MediaChannel::closed() const
{
    return !m_queue || m_queue->closed();
}

bool MediaChannel::aborted() const
{
    return m_queue && m_queue->aborted();
}

std::size_t MediaChannel::size() const
{
    return m_queue ? m_queue->size() : 0;
}

std::size_t MediaChannel::capacity() const
{
    return m_queue ? m_queue->capacity() : 0;
}

const MediaEdgePolicy& MediaChannel::policy() const noexcept
{
    return m_policy;
}

const MediaFormatDescriptor& MediaChannel::formatDescriptor() const noexcept
{
    return m_format;
}

const MediaTimeDescriptor& MediaChannel::timeDescriptor() const noexcept
{
    return m_time;
}

const MediaHardwareDescriptor& MediaChannel::hardwareDescriptor() const noexcept
{
    return m_hardware;
}

const MediaChannelMetrics& MediaChannel::metrics() const noexcept
{
    return m_metrics;
}

void MediaChannel::setConsumerWakeup(MediaNodeWakeup& wakeup) noexcept
{
    m_consumerWakeup = &wakeup;
}

void MediaChannel::setProducerWakeup(MediaNodeWakeup& wakeup) noexcept
{
    m_producerWakeup = &wakeup;
}

void MediaChannel::refreshQueueMetrics()
{
    if (m_queue) {
        m_metrics.queue = m_queue->metrics();
    }
}

} // namespace media::ffmpeg::graph
