#include "internal/graph/protocol/rtp/ingress/linux/MediaLinuxRtpIngressAdapter.h"

#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include "internal/graph/runtime/resource/MediaPreparedAllocationCost.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <cerrno>
#include <poll.h>
#include <sys/socket.h>
#endif

namespace media::ffmpeg::graph {

struct MediaLinuxRtpIngressAdapter::ReceiveState final {
#ifndef _WIN32
    ReceiveState(std::size_t descriptorCapacity, MediaRtpUdpIngressReceiveLease channelLease, std::pmr::memory_resource* resource)
        : lease(std::move(channelLease)), messages(descriptorCapacity, resource),
          vectors(descriptorCapacity, resource),
          slots(descriptorCapacity, resource)
    {
    }

    MediaRtpUdpIngressReceiveLease lease;
    std::pmr::vector<mmsghdr> messages;
    std::pmr::vector<iovec> vectors;
    std::pmr::vector<MediaRtpIngressWritableSlot> slots;
#endif
    std::atomic<std::uint64_t> receiveCalls{0};
    std::atomic<std::uint64_t> receiveSystemCalls{0};
    std::atomic<std::uint64_t> completedDatagrams{0};
    std::atomic<std::uint64_t> wouldBlocks{0};
    std::atomic<std::uint64_t> cancellations{0};
    std::atomic<std::uint64_t> truncations{0};
    std::atomic<std::uint64_t> pressureFailures{0};
    std::atomic<bool> diagnosticsLogged{false};
};

namespace {

::media::ErrorInfo invalidAdapter(const char* reason)
{
    return ::media::ErrorInfo::invalidArgument(reason);
}

::media::ErrorInfo cancelledReceive()
{
    return ::media::ErrorInfo::cancelled(
        "Linux RTP ingress batch receive was cancelled");
}

#ifndef _WIN32
void releaseSlots(
    MediaRtpIngressStorage& storage,
    std::span<const MediaRtpIngressWritableSlot> slots) noexcept
{
    for (const auto& slot : slots) {
        (void)storage.releaseReceiveSlot(slot.slotIndex);
    }
}
#endif

} // namespace

MediaLinuxRtpIngressAdapter::MediaLinuxRtpIngressAdapter(
    ConstructionKey,
    std::shared_ptr<MediaRtpUdpTransport> transport,
    MediaLineageUniquePtr<ReceiveState> receiveState,
    MediaRtpReceiveGeometryFacts planFacts) noexcept
    : m_transport(std::move(transport)),
      m_receiveState(std::move(receiveState)),
      m_planFacts(planFacts)
{
}

MediaLinuxRtpIngressAdapter::~MediaLinuxRtpIngressAdapter()
{
    logDiagnostics("destroyed");
}

::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>
MediaLinuxRtpIngressAdapter::create(
    std::shared_ptr<MediaRtpUdpTransport> transport,
    const MediaRtpReceiveGeometry& plan,
    MediaLineageAllocationScope allocation)
{
#ifdef _WIN32
    (void)transport;
    (void)plan;
    (void)allocation;
    return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
        ::media::ErrorInfo::notInitialized(
            "Linux RTP ingress adapter is unavailable on Windows"));
#else
    if (!MediaLineageAllocator<std::byte>{allocation}.resource())
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            invalidAdapter("RTP ingress requires its allocation domain"));
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            status.error());
    }
    if (plan.adapterKind() !=
            MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages ||
        plan.storageOwnership() !=
            MediaRtpIngressStorageOwnership::ReusableMessageArena ||
        plan.cancellationContract() !=
            MediaRtpIngressCancellationContract::DescriptorWake ||
        plan.completionEvidence() !=
            MediaRtpIngressCompletionEvidence::ReceiveMultipleMessagesReturn ||
        !transport || !transport->isOpen() ||
        transport->effectiveReceiveBufferBytes(plan.channel()) <= 0 ||
        plan.socketReceiveCapacityBytes() != static_cast<std::size_t>(
            transport->effectiveReceiveBufferBytes(plan.channel())) ||
        plan.descriptorCapacity() >
            static_cast<std::size_t>((std::numeric_limits<unsigned int>::max)())) {
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            invalidAdapter(
                "Linux RTP ingress adapter requires the exact planner-selected socket and recvmmsg contract"));
    }
    auto lease = transport->acquireIngressReceiveLease(plan.channel());
    if (!lease) return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(lease.error());
    try {
        auto state = makeMediaLineageUnique<ReceiveState>(allocation,
            plan.descriptorCapacity(), std::move(lease).value(),
            MediaLineageAllocator<std::byte>{allocation}.resource());
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::success(
            std::allocate_shared<MediaLinuxRtpIngressAdapter> (
                MediaLineageAllocator<MediaLinuxRtpIngressAdapter>{allocation}, ConstructionKey{},
                std::move(transport), std::move(state), plan.facts()));
    } catch (const std::bad_alloc&) {
        return ::media::Result<std::shared_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Linux RTP ingress reusable descriptor allocation failed"));
    }
#endif
}

::media::Result<std::size_t> MediaLinuxRtpIngressAdapter::preparationStorageBytes(std::size_t descriptors)
{
#ifndef _WIN32
    using Cost = MediaPreparedAllocationCost;
    return Cost::sum({Cost::shared<MediaLinuxRtpIngressAdapter>(), Cost::object<ReceiveState>(),
        Cost::fixedVector<mmsghdr>(descriptors),
        Cost::fixedVector<iovec>(descriptors), Cost::fixedVector<MediaRtpIngressWritableSlot>(descriptors)});
#else
    (void)descriptors;
    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::unsupported("Ingress allocation geometry requires its target platform"));
#endif
}

MediaRtpIngressAdapterKind
MediaLinuxRtpIngressAdapter::kind() const noexcept
{
    return MediaRtpIngressAdapterKind::LinuxReceiveMultipleMessages;
}

::media::Result<std::size_t> MediaLinuxRtpIngressAdapter::receive(
    MediaRtpIngressStorage& storage,
    int timeoutMilliseconds)
{
#ifdef _WIN32
    (void)storage;
    (void)timeoutMilliseconds;
    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::notInitialized(
            "Linux RTP ingress adapter is unavailable on Windows"));
#else
    if (timeoutMilliseconds <= 0 || !m_receiveState ||
        storage.descriptorCapacity() != m_receiveState->messages.size()) {
        return ::media::Result<std::size_t>::failure(
            invalidAdapter(
                "Linux RTP ingress storage differs from the initialized planner product"));
    }
    m_receiveState->receiveCalls.fetch_add(1, std::memory_order_relaxed);
    auto& lease = m_receiveState->lease;
    const auto channel = m_planFacts.channel;
    const int socketHandle = static_cast<int>(channel == MediaRtpUdpChannel::Rtp
        ? lease.rtpHandle() : lease.rtcpHandle());
    pollfd descriptors[]{
        {static_cast<int>(lease.cancellationHandle()), POLLIN, 0},
        {socketHandle, POLLIN, 0}};
    const int waited = ::poll(
        descriptors, 2,
        (std::min)(timeoutMilliseconds, lease.timeoutMilliseconds()));
    if (waited == 0) {
        m_receiveState->wouldBlocks.fetch_add(1, std::memory_order_relaxed);
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::wouldBlock(
                "Linux RTP ingress batch receive timed out"));
    }
    if (waited < 0) {
        return ::media::Result<std::size_t>::failure(
            errno == EINTR && lease.cancelled()
                ? cancelledReceive()
                : ::media::ErrorInfo::ioFailure(
                    "Linux RTP ingress poll failed", errno));
    }
    if ((descriptors[0].revents & POLLIN) != 0 || lease.cancelled()) {
        m_receiveState->cancellations.fetch_add(1, std::memory_order_relaxed);
        return ::media::Result<std::size_t>::failure(cancelledReceive());
    }
    const short failureEvents = POLLERR | POLLHUP | POLLNVAL;
    if ((descriptors[1].revents & failureEvents) != 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::ioFailure(
                "Linux RTP ingress socket poll failed", EIO));
    }
    if ((descriptors[1].revents & POLLIN) == 0) {
        return ::media::Result<std::size_t>::failure(::media::ErrorInfo::wouldBlock(
            "Linux RTP ingress poll returned no readable socket"));
    }

    std::size_t acquired = 0;
    for (; acquired < m_receiveState->slots.size(); ++acquired) {
        auto slot = storage.acquireReceiveSlot();
        if (!slot) {
            m_receiveState->pressureFailures.fetch_add(
                1, std::memory_order_relaxed);
            releaseSlots(storage, std::span(
                m_receiveState->slots.data(), acquired));
            return ::media::Result<std::size_t>::failure(slot.error());
        }
        m_receiveState->slots[acquired] = slot.value();
        m_receiveState->vectors[acquired] = iovec{
            slot.value().bytes.data(), slot.value().bytes.size()};
        m_receiveState->messages[acquired] = mmsghdr{};
        m_receiveState->messages[acquired].msg_hdr.msg_iov =
            &m_receiveState->vectors[acquired];
        m_receiveState->messages[acquired].msg_hdr.msg_iovlen = 1;
    }
    m_receiveState->receiveSystemCalls.fetch_add(
        1, std::memory_order_relaxed);
    const int received = ::recvmmsg(
        socketHandle,
        m_receiveState->messages.data(),
        static_cast<unsigned int>(acquired),
        MSG_DONTWAIT | MSG_TRUNC,
        nullptr);
    if (received < 0) {
        const int error = errno;
        releaseSlots(storage, std::span(
            m_receiveState->slots.data(), acquired));
        if (error == EAGAIN || error == EWOULDBLOCK) {
            m_receiveState->wouldBlocks.fetch_add(
                1, std::memory_order_relaxed);
        }
        return ::media::Result<std::size_t>::failure(
            error == EAGAIN || error == EWOULDBLOCK
                ? ::media::ErrorInfo::wouldBlock(
                    "Linux RTP ingress socket became empty before recvmmsg")
                : ::media::ErrorInfo::ioFailure(
                    "Linux RTP ingress recvmmsg failed", error));
    }
    if (received == 0) {
        releaseSlots(storage, std::span(
            m_receiveState->slots.data(), acquired));
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::wouldBlock(
                "Linux RTP ingress recvmmsg returned no datagrams"));
    }
    const std::size_t completed = static_cast<std::size_t>(received);
    for (std::size_t index = 0; index < completed; ++index) {
        const auto& message = m_receiveState->messages[index];
        if ((message.msg_hdr.msg_flags & MSG_TRUNC) != 0 ||
            message.msg_len == 0 ||
            message.msg_len > m_receiveState->slots[index].bytes.size()) {
            m_receiveState->truncations.fetch_add(
                1, std::memory_order_relaxed);
            releaseSlots(storage, std::span(
                m_receiveState->slots.data(), acquired));
            return ::media::Result<std::size_t>::failure(
                ::media::ErrorInfo::ioFailure(
                    "Linux RTP ingress datagram was truncated", EMSGSIZE));
        }
    }
    if (lease.cancelled()) {
        m_receiveState->cancellations.fetch_add(1, std::memory_order_relaxed);
        releaseSlots(storage, std::span(
            m_receiveState->slots.data(), acquired));
        return ::media::Result<std::size_t>::failure(cancelledReceive());
    }
    releaseSlots(storage, std::span(
        m_receiveState->slots.data() + completed,
        acquired - completed));
    const std::int64_t observedAtNanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    for (std::size_t index = 0; index < completed; ++index) {
        if (auto status = storage.commitReceivedSlot(
                m_receiveState->slots[index].slotIndex,
                channel,
                m_receiveState->messages[index].msg_len,
                observedAtNanoseconds); !status) {
            return ::media::Result<std::size_t>::failure(status.error());
        }
    }
    lease.markReceived(channel);
    m_receiveState->completedDatagrams.fetch_add(
        completed, std::memory_order_relaxed);
    return ::media::Result<std::size_t>::success(completed);
#endif
}

::media::Status
MediaLinuxRtpIngressAdapter::interruptReceive() noexcept
{
    return m_transport->interruptReceive();
}

::media::Status MediaLinuxRtpIngressAdapter::stop() noexcept
{
    auto status = m_transport->stop();
    logDiagnostics("stopped");
    return status;
}

::media::Status MediaLinuxRtpIngressAdapter::abort() noexcept
{
    auto status = m_transport->abort();
    logDiagnostics("aborted");
    return status;
}

void MediaLinuxRtpIngressAdapter::logDiagnostics(const char* stage) noexcept
{
    try {
    if (!m_receiveState ||
        m_receiveState->diagnosticsLogged.exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        std::string("rtp_ingress_adapter stage=") + stage +
            " adapter=linux_receive_multiple_messages" +
            " channel=" + std::string(m_planFacts.channel == MediaRtpUdpChannel::Rtp ? "rtp" : "rtcp") +
            " socket_capacity_bytes=" +
                std::to_string(m_planFacts.socketReceiveCapacityBytes) +
            " batch_capacity_bytes=" +
                std::to_string(m_planFacts.batchByteCapacity) +
            " descriptor_capacity=" +
                std::to_string(m_planFacts.descriptorCapacity) +
            " maximum_datagram_bytes=" +
                std::to_string(m_planFacts.maximumDatagramBytes) +
            " receive_calls=" + std::to_string(
                m_receiveState->receiveCalls.load(std::memory_order_relaxed)) +
            " receive_syscalls=" + std::to_string(
                m_receiveState->receiveSystemCalls.load(std::memory_order_relaxed)) +
            " completed_datagrams=" + std::to_string(
                m_receiveState->completedDatagrams.load(std::memory_order_relaxed)) +
            " would_block=" + std::to_string(
                m_receiveState->wouldBlocks.load(std::memory_order_relaxed)) +
            " cancellations=" + std::to_string(
                m_receiveState->cancellations.load(std::memory_order_relaxed)) +
            " truncations=" + std::to_string(
                m_receiveState->truncations.load(std::memory_order_relaxed)) +
            " pressure_failures=" + std::to_string(
                m_receiveState->pressureFailures.load(std::memory_order_relaxed)) +
            " storage_reuse=preallocated post_start_allocation_path=none");

    } catch (...) {
        // Cleanup and protocol cancellation must finish when diagnostics cannot allocate.
    }
}

} // namespace media::ffmpeg::graph
