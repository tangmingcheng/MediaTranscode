#include "internal/graph/runtime/channel/MediaChannel.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
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
    if (hardByteLimitEnabled()) {
        m_byteBudgetConfigurationValid =
            !m_policy.bufferPolicy.memoryBudget.allowDynamicGrowth &&
            m_policy.queuePolicy.overflowPolicy ==
                MediaQueueOverflowPolicy::BlockProducer;
    }
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
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaChannel push failed: buffer is null"));
    }

    std::unique_lock lock(m_mutationMutex);
    for (;;) {
        const std::uint64_t sequence = m_mutationSequence.load(
            std::memory_order_acquire);
        switch (pushOutcomeLocked(buffer)) {
        case MediaQueuePushOutcome::Accepted:
            return ::media::Status::success();
        case MediaQueuePushOutcome::Dropped:
            m_metrics.pushed++;
            if (m_consumerWakeup) m_consumerWakeup->notify();
            refreshQueueMetrics();
            return ::media::Status::success();
        case MediaQueuePushOutcome::Closed:
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                "MediaChannel push interrupted: queue closed"));
        case MediaQueuePushOutcome::Aborted:
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MediaChannel push failed: queue aborted"));
        case MediaQueuePushOutcome::WouldBlock:
            m_externalBlockedPushes.fetch_add(1, std::memory_order_relaxed);
            m_externalBlockedProducers.fetch_add(1, std::memory_order_release);
            refreshQueueMetrics();
            m_externalBlockedProducers.notify_all();
            m_mutationChanged.wait(lock, [&] {
                return m_mutationSequence.load(std::memory_order_acquire) !=
                    sequence;
            });
            m_externalBlockedProducers.fetch_sub(1, std::memory_order_release);
            refreshQueueMetrics();
            m_externalBlockedProducers.notify_all();
            break;
        }
    }
}

MediaQueuePushOutcome MediaChannel::pushOutcome(MediaBufferRef buffer)
{
    std::lock_guard lock(m_mutationMutex);
    return pushOutcomeLocked(std::move(buffer));
}

MediaQueuePushOutcome MediaChannel::pushOutcomeLocked(
    MediaBufferRef buffer,
    bool publishAccepted)
{
    if (!m_queue) return MediaQueuePushOutcome::Closed;
    if (m_queue->aborted()) return MediaQueuePushOutcome::Aborted;
    if (m_closeRequested) return MediaQueuePushOutcome::Closed;

    std::uint64_t payloadBytes = 0;
    if (hardByteLimitEnabled()) {
        if (!m_byteBudgetConfigurationValid) {
            return MediaQueuePushOutcome::Aborted;
        }
        const auto footprint = payloadBytesForBudget(buffer);
        if (!footprint) return MediaQueuePushOutcome::Aborted;
        payloadBytes = *footprint;
        if (payloadBytes > m_policy.bufferPolicy.memoryBudget.maxBytes) {
            return MediaQueuePushOutcome::Aborted;
        }
        if (wouldExceedByteBudget(payloadBytes)) {
            return MediaQueuePushOutcome::WouldBlock;
        }
    }

    const std::size_t capacity = m_queue->capacity();
    if (m_reservedCapacity != 0 &&
        (m_reservedCapacity > capacity ||
         m_queue->size() >= capacity - m_reservedCapacity)) {
        return MediaQueuePushOutcome::WouldBlock;
    }

    const MediaQueuePushOutcome outcome = m_queue->pushOutcome(std::move(buffer));
    if (outcome == MediaQueuePushOutcome::Accepted) {
        accountAcceptedPayload(payloadBytes);
        m_metrics.pushed++;
        if (publishAccepted) publishAcceptedMutation();
    }
    refreshQueueMetrics();
    return outcome;
}

MediaQueuePushOutcome MediaChannel::pushReservedOutcomeLocked(
    MediaBufferRef buffer)
{
    if (!m_queue || m_queue->aborted()) return MediaQueuePushOutcome::Aborted;
    std::uint64_t payloadBytes = 0;
    if (hardByteLimitEnabled()) {
        if (!m_byteBudgetConfigurationValid) {
            return MediaQueuePushOutcome::Aborted;
        }
        const auto footprint = payloadBytesForBudget(buffer);
        if (!footprint) return MediaQueuePushOutcome::Aborted;
        payloadBytes = *footprint;
        if (payloadBytes > m_policy.bufferPolicy.memoryBudget.maxBytes) {
            return MediaQueuePushOutcome::Aborted;
        }
        if (wouldExceedByteBudget(payloadBytes)) {
            return MediaQueuePushOutcome::WouldBlock;
        }
    }
    const MediaQueuePushOutcome outcome = m_queue->pushOutcome(std::move(buffer));
    if (outcome == MediaQueuePushOutcome::Accepted) {
        accountAcceptedPayload(payloadBytes);
        m_metrics.pushed++;
    }
    refreshQueueMetrics();
    return outcome;
}

void MediaChannel::publishAcceptedMutation() noexcept
{
    if (m_consumerWakeup) m_consumerWakeup->notify();
    signalMutationWaiters();
}

void MediaChannel::publishReservedCapacityMutation() noexcept
{
    if (m_producerWakeup) m_producerWakeup->notify();
    signalMutationWaiters();
}

void MediaChannel::finalizeDeferredCloseLocked() noexcept
{
    if (m_closeRequested && m_authorizedCapacity == 0 && m_queue &&
        !m_queue->closed() && !m_queue->aborted()) {
        m_queue->close();
    }
}

::media::Status MediaChannel::pop(MediaBufferRef& out)
{
    if (!m_queue) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaChannel pop failed: queue is null"));
    }

    bool externalWait = false;
    {
        std::lock_guard lock(m_mutationMutex);
        externalWait = m_policy.queuePolicy.mode != MediaQueueMode::SpscRing &&
            m_queue->size() == 0 && !m_queue->closed() &&
            !m_queue->aborted();
        if (externalWait) {
            m_externalBlockedConsumers.fetch_add(
                1, std::memory_order_release);
            refreshQueueMetrics();
            m_externalBlockedConsumers.notify_all();
        }
    }
    auto status = m_queue->pop(out);
    std::lock_guard lock(m_mutationMutex);
    if (externalWait) {
        m_externalBlockedConsumers.fetch_sub(
            1, std::memory_order_release);
        m_externalBlockedConsumers.notify_all();
    }
    if (status) {
        accountPoppedPayload(out);
        m_metrics.popped++;
        if (m_producerWakeup) m_producerWakeup->notify();
        signalMutationWaiters();
    }
    refreshQueueMetrics();
    return status;
}

bool MediaChannel::tryPop(MediaBufferRef& out)
{
    std::lock_guard lock(m_mutationMutex);
    if (!m_queue) {
        return false;
    }

    const bool ok = m_queue->tryPop(out);
    if (ok) {
        accountPoppedPayload(out);
        m_metrics.popped++;
        if (m_producerWakeup) {
            m_producerWakeup->notify();
        }
    }
    if (ok) signalMutationWaiters();
    refreshQueueMetrics();
    return ok;
}

void MediaChannel::close()
{
    m_externalLifecycleMutations.fetch_add(1, std::memory_order_release);
    m_externalLifecycleMutations.notify_all();
    std::lock_guard lock(m_mutationMutex);
    m_externalLifecycleMutations.fetch_sub(1, std::memory_order_release);
    m_externalLifecycleMutations.notify_all();
    m_closeRequested = true;
    if (m_queue && m_authorizedCapacity == 0) {
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
    signalMutationWaiters();
}

void MediaChannel::abort()
{
    m_externalLifecycleMutations.fetch_add(1, std::memory_order_release);
    m_externalLifecycleMutations.notify_all();
    std::lock_guard lock(m_mutationMutex);
    m_externalLifecycleMutations.fetch_sub(1, std::memory_order_release);
    m_externalLifecycleMutations.notify_all();
    m_closeRequested = true;
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
    signalMutationWaiters();
}

void MediaChannel::clear()
{
    std::lock_guard lock(m_mutationMutex);
    if (m_queue) {
        m_queue->clear();
    }
    m_queuedPayloadBytes = 0;
    m_metrics.cleared++;
    if (m_consumerWakeup) {
        m_consumerWakeup->notify();
    }
    if (m_producerWakeup) {
        m_producerWakeup->notify();
    }
    refreshQueueMetrics();
    signalMutationWaiters();
}

void MediaChannel::signalMutationWaiters() noexcept
{
    m_mutationSequence.fetch_add(1, std::memory_order_release);
    m_mutationChanged.notify_all();
}

bool MediaChannel::closed() const
{
    std::lock_guard lock(m_mutationMutex);
    return m_closeRequested || !m_queue || m_queue->closed();
}

bool MediaChannel::aborted() const
{
    return m_queue && m_queue->aborted();
}

std::size_t MediaChannel::size() const
{
    std::lock_guard lock(m_mutationMutex);
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

void MediaChannel::refreshQueueMetrics() noexcept
{
    if (m_queue) {
        m_metrics.queue = m_queue->metrics();
        m_metrics.queue.blockedPushes.fetch_add(
            m_externalBlockedPushes.load(std::memory_order_relaxed));
        m_metrics.queue.blockedProducers.fetch_add(
            m_externalBlockedProducers.load(std::memory_order_acquire));
        m_metrics.queue.blockedConsumers.fetch_add(
            m_externalBlockedConsumers.load(std::memory_order_acquire));
    }
}

bool MediaChannel::hardByteLimitEnabled() const noexcept
{
    const auto& budget = m_policy.bufferPolicy.memoryBudget;
    return budget.enforceHardLimit && budget.hasByteLimit();
}

std::optional<std::uint64_t> MediaChannel::payloadBytesForBudget(
    const MediaBufferRef& buffer) const noexcept
{
    if (!buffer) return std::nullopt;
    if (buffer->isEof() || buffer->isFlush()) return std::uint64_t{0};
    return buffer->payloadFootprintBytes();
}

bool MediaChannel::wouldExceedByteBudget(
    std::uint64_t payloadBytes) const noexcept
{
    const std::uint64_t maximum =
        m_policy.bufferPolicy.memoryBudget.maxBytes;
    return payloadBytes > maximum || m_queuedPayloadBytes >
        maximum - payloadBytes;
}

void MediaChannel::accountAcceptedPayload(
    std::uint64_t payloadBytes) noexcept
{
    if (hardByteLimitEnabled()) m_queuedPayloadBytes += payloadBytes;
}

void MediaChannel::accountPoppedPayload(
    const MediaBufferRef& buffer) noexcept
{
    if (!hardByteLimitEnabled()) return;
    const auto footprint = payloadBytesForBudget(buffer);
    if (!footprint || *footprint > m_queuedPayloadBytes) {
        m_byteBudgetConfigurationValid = false;
        m_queuedPayloadBytes = 0;
        return;
    }
    m_queuedPayloadBytes -= *footprint;
}

} // namespace media::ffmpeg::graph
