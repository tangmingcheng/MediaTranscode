#include "internal/graph/utils/MediaCleanupError.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpTransport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <new>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace media::ffmpeg::graph {

MediaRtpUdpIngressReceiveLease::MediaRtpUdpIngressReceiveLease(
    std::shared_ptr<void> owner,
    void* state,
    intptr_t rtpHandle,
    intptr_t rtcpHandle,
    intptr_t cancellationHandle,
    int timeoutMilliseconds,
    MediaRtpUdpChannel preferredChannel,
    std::uint64_t cancellationSequence,
    CancelledFn cancelledFn,
    MarkReceivedFn markReceivedFn,
    ReleaseFn releaseFn) noexcept
    : m_owner(std::move(owner)),
      m_state(state),
      m_rtpHandle(rtpHandle),
      m_rtcpHandle(rtcpHandle),
      m_cancellationHandle(cancellationHandle),
      m_timeoutMilliseconds(timeoutMilliseconds),
      m_preferredChannel(preferredChannel),
      m_cancellationSequence(cancellationSequence),
      m_cancelledFn(cancelledFn),
      m_markReceivedFn(markReceivedFn),
      m_releaseFn(releaseFn)
{
}

MediaRtpUdpIngressReceiveLease::~MediaRtpUdpIngressReceiveLease()
{
    release();
}

MediaRtpUdpIngressReceiveLease::MediaRtpUdpIngressReceiveLease(
    MediaRtpUdpIngressReceiveLease&& other) noexcept
    : m_owner(std::move(other.m_owner)),
      m_state(std::exchange(other.m_state, nullptr)),
      m_rtpHandle(std::exchange(other.m_rtpHandle, -1)),
      m_rtcpHandle(std::exchange(other.m_rtcpHandle, -1)),
      m_cancellationHandle(std::exchange(other.m_cancellationHandle, -1)),
      m_timeoutMilliseconds(std::exchange(other.m_timeoutMilliseconds, 0)),
      m_preferredChannel(other.m_preferredChannel),
      m_cancellationSequence(other.m_cancellationSequence),
      m_cancelledFn(std::exchange(other.m_cancelledFn, nullptr)),
      m_markReceivedFn(std::exchange(other.m_markReceivedFn, nullptr)),
      m_releaseFn(std::exchange(other.m_releaseFn, nullptr))
{
}

MediaRtpUdpIngressReceiveLease&
MediaRtpUdpIngressReceiveLease::operator=(
    MediaRtpUdpIngressReceiveLease&& other) noexcept
{
    if (this != &other) {
        release();
        m_owner = std::move(other.m_owner);
        m_state = std::exchange(other.m_state, nullptr);
        m_rtpHandle = std::exchange(other.m_rtpHandle, -1);
        m_rtcpHandle = std::exchange(other.m_rtcpHandle, -1);
        m_cancellationHandle = std::exchange(other.m_cancellationHandle, -1);
        m_timeoutMilliseconds = std::exchange(other.m_timeoutMilliseconds, 0);
        m_preferredChannel = other.m_preferredChannel;
        m_cancellationSequence = other.m_cancellationSequence;
        m_cancelledFn = std::exchange(other.m_cancelledFn, nullptr);
        m_markReceivedFn = std::exchange(other.m_markReceivedFn, nullptr);
        m_releaseFn = std::exchange(other.m_releaseFn, nullptr);
    }
    return *this;
}

intptr_t MediaRtpUdpIngressReceiveLease::rtpHandle() const noexcept { return m_rtpHandle; }
intptr_t MediaRtpUdpIngressReceiveLease::rtcpHandle() const noexcept { return m_rtcpHandle; }
intptr_t MediaRtpUdpIngressReceiveLease::cancellationHandle() const noexcept { return m_cancellationHandle; }
int MediaRtpUdpIngressReceiveLease::timeoutMilliseconds() const noexcept { return m_timeoutMilliseconds; }
MediaRtpUdpChannel MediaRtpUdpIngressReceiveLease::preferredChannel() const noexcept { return m_preferredChannel; }

bool MediaRtpUdpIngressReceiveLease::cancelled() const noexcept
{
    return !m_state || !m_cancelledFn ||
        m_cancelledFn(m_state, m_cancellationSequence);
}

void MediaRtpUdpIngressReceiveLease::markReceived(
    MediaRtpUdpChannel channel) noexcept
{
    if (m_state && m_markReceivedFn) m_markReceivedFn(m_state, channel);
}

void MediaRtpUdpIngressReceiveLease::release() noexcept
{
    if (m_state && m_releaseFn) m_releaseFn(m_state, m_preferredChannel);
    m_state = nullptr;
    m_owner.reset();
}

struct MediaRtpUdpTransport::Impl final {
    enum class State { Running, StopRequested, Aborted };
    Impl(std::shared_ptr<MediaSocketRuntime> socketRuntime,
         MediaUdpSocket rtpSocket,
         MediaUdpSocket rtcpSocket,
#ifdef _WIN32
         WSAEVENT cancelEvent,
#else
         int cancelReadFd,
         int cancelWriteFd,
#endif
         std::size_t datagramBytes,
         std::pmr::memory_resource& storage,
         int readTimeoutMs,
         std::shared_ptr<MediaRtpUdpTransportPhaseController> controller)
        : runtime(std::move(socketRuntime))
        , rtp(std::move(rtpSocket))
        , rtcp(std::move(rtcpSocket))
        , rtpPortValue(rtp.localPort())
        , rtcpPortValue(rtcp.localPort())
#ifdef _WIN32
        , cancellationEvent(cancelEvent)
#else
        , cancellationRead(cancelReadFd)
        , cancellationWrite(cancelWriteFd)
#endif
        , maximumDatagramBytes(datagramBytes)
        , receiveScratch(storage, std::size_t{0})
        , effectiveReceiveBufferBytes((std::min)(
              rtp.effectiveReceiveBufferBytes(),
              rtcp.effectiveReceiveBufferBytes()))
        , cancellableReadTimeoutMs(readTimeoutMs)
        , phaseController(std::move(controller))
        , preferRtcp(false)
        , state(State::Running)
        , cancellationSequence(0)
        , receiveActive{}
    {
    }

    ~Impl()
    {
#ifdef _WIN32
        if (cancellationEvent != WSA_INVALID_EVENT) {
            WSACloseEvent(cancellationEvent);
        }
#else
        if (cancellationRead >= 0) ::close(cancellationRead);
        if (cancellationWrite >= 0) ::close(cancellationWrite);
#endif
    }

    std::shared_ptr<MediaSocketRuntime> runtime;
    MediaUdpSocket rtp;
    MediaUdpSocket rtcp;
    const uint16_t rtpPortValue;
    const uint16_t rtcpPortValue;
#ifdef _WIN32
    const WSAEVENT cancellationEvent;
#else
    const int cancellationRead;
    const int cancellationWrite;
#endif
    std::size_t maximumDatagramBytes;
    MediaOwnedByteStorage receiveScratch;
    int effectiveReceiveBufferBytes;
    int cancellableReadTimeoutMs;
    std::shared_ptr<MediaRtpUdpTransportPhaseController> phaseController;
    bool preferRtcp;
    std::mutex lifecycleMutex;
    std::mutex receiveMutex;
    std::condition_variable receiveReleased;
    std::atomic<State> state;
    std::atomic_uint64_t cancellationSequence;
    std::array<bool, 2> receiveActive;
};

namespace {

::media::Result<MediaRtpUdpTransport> transportError(::media::ErrorInfo error)
{
    return ::media::Result<MediaRtpUdpTransport>::failure(std::move(error));
}

} // namespace

MediaRtpUdpTransport::MediaRtpUdpTransport() noexcept = default;
MediaRtpUdpTransport::MediaRtpUdpTransport(std::shared_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
MediaRtpUdpTransport::~MediaRtpUdpTransport() { close(); }
MediaRtpUdpTransport::MediaRtpUdpTransport(MediaRtpUdpTransport&& other) noexcept
{
    std::lock_guard lock(other.m_handleMutex);
    m_impl = std::move(other.m_impl);
}
MediaRtpUdpTransport& MediaRtpUdpTransport::operator=(MediaRtpUdpTransport&& other) noexcept
{
    if (this != &other) {
        close();
        std::scoped_lock lock(m_handleMutex, other.m_handleMutex);
        m_impl = std::move(other.m_impl);
    }
    return *this;
}

::media::Result<MediaRtpUdpTransport> MediaRtpUdpTransport::open(
    const MediaRtpUdpTransportConfig& config, std::pmr::memory_resource& storage)
{
    if (config.bindAddress.empty() || config.receiveBufferBytes <= 0 ||
        config.maximumDatagramBytes == 0 || config.maximumDatagramBytes > 65'535 ||
        config.cancellableReadTimeoutMs <= 0) {
        return transportError(::media::ErrorInfo::invalidArgument(
            "RTP UDP transport requires explicit address, buffers, datagram limit, and read timeout"));
    }
    if (config.rtpPort == 0 || config.rtpPort == 65'535 ||
        config.rtcpPort != static_cast<uint16_t>(config.rtpPort + 1)) {
        return transportError(::media::ErrorInfo::invalidArgument(
            "RTP and RTCP ports must be an explicit even/odd adjacent pair"));
    }
    if ((config.rtpPort % 2) != 0) {
        return transportError(::media::ErrorInfo::invalidArgument("RTP port must be even"));
    }
    auto runtime = MediaSocketRuntime::create();
    if (!runtime) return transportError(runtime.error());
    auto rtp = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        config.addressFamily, config.bindAddress, config.rtpPort, config.receiveBufferBytes});
    if (!rtp) return transportError(rtp.error());
    auto rtcp = MediaUdpSocket::bind(runtime.value(), MediaUdpSocketConfig{
        config.addressFamily, config.bindAddress, config.rtcpPort, config.receiveBufferBytes});
    if (!rtcp) return transportError(rtcp.error());
#ifdef _WIN32
    const WSAEVENT cancellationEvent = WSACreateEvent();
    if (cancellationEvent == WSA_INVALID_EVENT) {
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation event creation failed", WSAGetLastError()));
    }
    return ::media::Result<MediaRtpUdpTransport>::success(MediaRtpUdpTransport(std::make_shared<Impl>(
        runtime.value(), std::move(rtp.value()), std::move(rtcp.value()), cancellationEvent,
        config.maximumDatagramBytes, storage, config.cancellableReadTimeoutMs,
        config.phaseController)));
#else
    int cancellationPipe[2]{-1, -1};
    if (pipe(cancellationPipe) != 0) {
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation pipe creation failed", errno));
    }
    const int readFlags = fcntl(cancellationPipe[0], F_GETFL, 0);
    const int writeFlags = fcntl(cancellationPipe[1], F_GETFL, 0);
    if (readFlags < 0 || writeFlags < 0 ||
        fcntl(cancellationPipe[0], F_SETFL, readFlags | O_NONBLOCK) != 0 ||
        fcntl(cancellationPipe[1], F_SETFL, writeFlags | O_NONBLOCK) != 0) {
        const int error = errno;
        ::close(cancellationPipe[0]);
        ::close(cancellationPipe[1]);
        return transportError(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation pipe configuration failed", error));
    }
    return ::media::Result<MediaRtpUdpTransport>::success(MediaRtpUdpTransport(std::make_shared<Impl>(
        runtime.value(), std::move(rtp.value()), std::move(rtcp.value()),
        cancellationPipe[0], cancellationPipe[1], config.maximumDatagramBytes, storage,
        config.cancellableReadTimeoutMs, config.phaseController)));
#endif
}

::media::Result<MediaRtpUdpDatagram> MediaRtpUdpTransport::receive()
{
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    return receive(impl->cancellableReadTimeoutMs);
}

::media::Result<MediaRtpUdpDatagram> MediaRtpUdpTransport::receive(int timeoutOverrideMs)
{
    if (timeoutOverrideMs <= 0) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::invalidArgument("RTP UDP receive timeout override must be positive"));
    }
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    std::unique_lock receiveLock(impl->receiveMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    }
    try {
        // The exclusive receiver reuses one buffer for both RTP and RTCP.
        if (impl->receiveScratch.empty())
            impl->receiveScratch = MediaOwnedByteStorage(
                *impl->receiveScratch.resource(), impl->maximumDatagramBytes);
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::allocationFailed("RTP UDP receive storage allocation failed"));
    }
    uint64_t sequence = 0;
    {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::Running) {
            return ::media::Result<MediaRtpUdpDatagram>::failure(
                ::media::ErrorInfo::cancelled("RTP UDP transport is stopped"));
        }
        if (impl->receiveActive[0] || impl->receiveActive[1]) {
            return ::media::Result<MediaRtpUdpDatagram>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP UDP mixed receive conflicts with an active channel receiver"));
        }
        impl->receiveActive.fill(true);
        sequence = impl->cancellationSequence.load(
            std::memory_order_acquire);
    }
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::ReceiveProtected);
    }
    auto finishError = [impl, sequence](::media::ErrorInfo error) {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        if (impl->state.load(std::memory_order_acquire) !=
                Impl::State::Running ||
            impl->cancellationSequence.load(std::memory_order_acquire) !=
                sequence) {
            error = ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled");
        }
        impl->receiveActive.fill(false);
        impl->receiveReleased.notify_all();
        return ::media::Result<MediaRtpUdpDatagram>::failure(std::move(error));
    };
    const auto receiveDatagram = [&](MediaUdpSocket* socket, MediaRtpUdpChannel channel)
        -> ::media::Result<MediaRtpUdpDatagram> {
        // Both platform branches hold lifecycleMutex here; receiveMutex also
        // prevents another receiver from overwriting the borrowed scratch.
        auto networkEvent = socket->consumeNetworkEvent();
        if (!networkEvent) {
            impl->receiveActive.fill(false);
            impl->receiveReleased.notify_all();
            return ::media::Result<MediaRtpUdpDatagram>::failure(networkEvent.error());
        }
        auto received = socket->receiveInto(impl->receiveScratch.view());
        impl->receiveActive.fill(false);
        impl->receiveReleased.notify_all();
        if (!received) return ::media::Result<MediaRtpUdpDatagram>::failure(received.error());
        if (received.value() == 0) return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::invalidArgument("RTP/RTCP datagram is empty"));
        impl->preferRtcp = channel == MediaRtpUdpChannel::Rtp;
        try {
            return ::media::Result<MediaRtpUdpDatagram>::success(MediaRtpUdpDatagram{
                channel, MediaOwnedByteStorage(*impl->receiveScratch.resource(),
                    impl->receiveScratch.view().first(received.value()))});
        } catch (const std::bad_alloc&) {
            return ::media::Result<MediaRtpUdpDatagram>::failure(
                ::media::ErrorInfo::allocationFailed("RTP UDP retained datagram allocation failed"));
        }
    };
#ifdef _WIN32
    MediaUdpSocket* preferred = impl->preferRtcp ? &impl->rtcp : &impl->rtp;
    MediaUdpSocket* secondary = impl->preferRtcp ? &impl->rtp : &impl->rtcp;
    const MediaRtpUdpChannel preferredChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtcp : MediaRtpUdpChannel::Rtp;
    const MediaRtpUdpChannel secondaryChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
    const WSAEVENT events[]{impl->cancellationEvent,
                            reinterpret_cast<WSAEVENT>(preferred->waitHandle()),
                            reinterpret_cast<WSAEVENT>(secondary->waitHandle())};
    const DWORD wait = WSAWaitForMultipleEvents(3, events, FALSE,
        static_cast<DWORD>((std::min)(timeoutOverrideMs, impl->cancellableReadTimeoutMs)), FALSE);
    if (wait == WSA_WAIT_TIMEOUT) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP receive timed out"));
    }
    if (wait == WSA_WAIT_FAILED) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP event wait failed", WSAGetLastError()));
    }
    const DWORD index = wait - WSA_WAIT_EVENT_0;
    if (index == 0) {
        return finishError(::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    MediaUdpSocket* selected = index == 1 ? preferred : secondary;
    const MediaRtpUdpChannel channel = index == 1 ? preferredChannel : secondaryChannel;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::Running ||
        impl->cancellationSequence.load(std::memory_order_acquire) !=
            sequence) {
        impl->receiveActive.fill(false);
        impl->receiveReleased.notify_all();
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    return receiveDatagram(selected, channel);
#else
    MediaUdpSocket* preferred = impl->preferRtcp ? &impl->rtcp : &impl->rtp;
    MediaUdpSocket* secondary = impl->preferRtcp ? &impl->rtp : &impl->rtcp;
    const MediaRtpUdpChannel preferredChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtcp : MediaRtpUdpChannel::Rtp;
    const MediaRtpUdpChannel secondaryChannel = impl->preferRtcp ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
    pollfd descriptors[]{
        {impl->cancellationRead, POLLIN, 0},
        {static_cast<int>(preferred->waitHandle()), POLLIN, 0},
        {static_cast<int>(secondary->waitHandle()), POLLIN, 0}};
    const int wait = poll(descriptors, 3,
        (std::min)(timeoutOverrideMs, impl->cancellableReadTimeoutMs));
    if (wait == 0) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP receive timed out"));
    }
    if (wait < 0) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP poll failed", errno));
    }
    if ((descriptors[0].revents & POLLIN) != 0) {
        return finishError(::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    const short socketFailure = POLLERR | POLLHUP | POLLNVAL;
    if ((descriptors[1].revents & socketFailure) != 0 ||
        (descriptors[2].revents & socketFailure) != 0) {
        return finishError(::media::ErrorInfo::ioFailure("RTP UDP socket poll failed", EIO));
    }
    const bool preferredReadable = (descriptors[1].revents & POLLIN) != 0;
    const bool secondaryReadable = (descriptors[2].revents & POLLIN) != 0;
    if (!preferredReadable && !secondaryReadable) {
        return finishError(::media::ErrorInfo::wouldBlock("RTP UDP poll had no readable datagram"));
    }
    MediaUdpSocket* selected = preferredReadable ? preferred : secondary;
    const MediaRtpUdpChannel channel = preferredReadable ? preferredChannel : secondaryChannel;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state.load(std::memory_order_acquire) != Impl::State::Running ||
        impl->cancellationSequence.load(std::memory_order_acquire) != sequence) {
        impl->receiveActive.fill(false);
        impl->receiveReleased.notify_all();
        return ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::cancelled("RTP UDP receive was cancelled"));
    }
    return receiveDatagram(selected, channel);
#endif
}

::media::Status MediaRtpUdpTransport::releaseReceiveStorage()
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(
        ::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::unique_lock receiveLock(impl->receiveMutex);
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->receiveActive[0] || impl->receiveActive[1])
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP UDP scratch release requires no active receiver"));
    impl->receiveScratch = MediaOwnedByteStorage(
        *impl->receiveScratch.resource(), std::size_t{0});
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::interruptReceive() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(mediaCleanupError(::media::ErrorCode::NotInitialized, "RTP UDP transport is closed", 0));
    Impl::State expected = Impl::State::Running;
    if (impl->state.compare_exchange_strong(
            expected, Impl::State::StopRequested,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        impl->cancellationSequence.fetch_add(
            1, std::memory_order_acq_rel);
    } else if (expected == Impl::State::Aborted) {
        return ::media::Status::failure(
            mediaCleanupError(::media::ErrorCode::Cancelled,
                "RTP UDP transport was aborted during receive interruption", 0));
    }
    return signalCancellation(impl);
}

::media::Status MediaRtpUdpTransport::stop() noexcept
{
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Status::failure(
            mediaCleanupError(::media::ErrorCode::NotInitialized,
                "RTP UDP transport is closed", 0));
    }
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::StopLifetimeAcquired);
    }
    return interruptReceive();
}

::media::Status MediaRtpUdpTransport::reset() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard receiveLock(impl->receiveMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP UDP transport is closed"));
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (impl->state.load(std::memory_order_acquire) == Impl::State::Aborted) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled("Aborted RTP UDP transport cannot be reset"));
    }
    if (impl->state.load(std::memory_order_acquire) !=
            Impl::State::StopRequested ||
        (impl->receiveActive[0] || impl->receiveActive[1])) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP UDP reset requires stopped transport with no active receive"));
    }
    if (auto reset = resetCancellation(impl); !reset) return reset;
    impl->state.store(Impl::State::Running, std::memory_order_release);
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::abort() noexcept
{
    const auto impl = snapshot();
    if (!impl) return ::media::Status::failure(mediaCleanupError(::media::ErrorCode::NotInitialized, "RTP UDP transport is closed", 0));
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::AbortLifetimeAcquired);
    }
    if (impl->state.exchange(
            Impl::State::Aborted, std::memory_order_acq_rel) ==
        Impl::State::Aborted) {
        return ::media::Status::failure(
            mediaCleanupError(::media::ErrorCode::Cancelled, "RTP UDP transport was closed during abort", 0));
    }
    impl->cancellationSequence.fetch_add(1, std::memory_order_acq_rel);
    return signalCancellation(impl);
}

void MediaRtpUdpTransport::close() noexcept
{
    std::shared_ptr<Impl> impl;
    {
        std::lock_guard handleLock(m_handleMutex);
        impl = std::move(m_impl);
    }
    if (!impl) return;
    impl->state.store(Impl::State::Aborted, std::memory_order_release);
    impl->cancellationSequence.fetch_add(1, std::memory_order_acq_rel);
    (void)signalCancellation(impl);
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::CloseReceiveWait);
    }
    std::unique_lock receiveLock(impl->receiveMutex);
    if (impl->phaseController) {
        impl->phaseController->synchronize(
            MediaRtpUdpTransportPhase::CloseReceiveAcquired);
    }
    std::unique_lock lifecycleLock(impl->lifecycleMutex);
    impl->receiveReleased.wait(lifecycleLock, [&] {
        return !impl->receiveActive[0] && !impl->receiveActive[1];
    });
    impl->rtp.close();
    impl->rtcp.close();
}

bool MediaRtpUdpTransport::isOpen() const noexcept
{
    const auto impl = snapshot();
    if (!impl) return false;
    std::lock_guard lock(impl->lifecycleMutex);
    return impl->rtp.isOpen() && impl->rtcp.isOpen();
}

uint16_t MediaRtpUdpTransport::rtpPort() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->rtpPortValue : 0;
}

uint16_t MediaRtpUdpTransport::rtcpPort() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->rtcpPortValue : 0;
}

std::size_t MediaRtpUdpTransport::maximumDatagramBytes() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->maximumDatagramBytes : 0;
}

int MediaRtpUdpTransport::effectiveReceiveBufferBytes() const noexcept
{
    const auto impl = snapshot();
    return impl ? impl->effectiveReceiveBufferBytes : 0;
}

int MediaRtpUdpTransport::effectiveReceiveBufferBytes(MediaRtpUdpChannel channel) const noexcept
{
    const auto impl = snapshot();
    if (!impl) return 0;
    if (channel == MediaRtpUdpChannel::Rtp) return impl->rtp.effectiveReceiveBufferBytes();
    if (channel == MediaRtpUdpChannel::Rtcp) return impl->rtcp.effectiveReceiveBufferBytes();
    return 0;
}

::media::Result<MediaRtpUdpIngressReceiveLease>
MediaRtpUdpTransport::acquireIngressReceiveLease(MediaRtpUdpChannel channel)
{
    const auto impl = snapshot();
    if (!impl) {
        return ::media::Result<MediaRtpUdpIngressReceiveLease>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP UDP ingress receive requires an open transport"));
    }
    if (channel != MediaRtpUdpChannel::Rtp && channel != MediaRtpUdpChannel::Rtcp)
        return ::media::Result<MediaRtpUdpIngressReceiveLease>::failure(
            ::media::ErrorInfo::invalidArgument("Ingress receive requires a fixed RTP or RTCP channel"));
    const std::size_t index = channel == MediaRtpUdpChannel::Rtp ? 0 : 1;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    if (!impl->rtp.isOpen() || !impl->rtcp.isOpen()) {
        return ::media::Result<MediaRtpUdpIngressReceiveLease>::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP UDP ingress receive sockets are closed"));
    }
    std::uint64_t sequence = 0;
    {
        if (impl->state.load(std::memory_order_acquire) !=
                Impl::State::Running || impl->receiveActive[index]) {
            return ::media::Result<MediaRtpUdpIngressReceiveLease>::failure(
                ::media::ErrorInfo::cancelled(
                    "RTP UDP ingress receive is stopped or already active"));
        }
        impl->receiveActive[index] = true;
        sequence = impl->cancellationSequence.load(
            std::memory_order_acquire);
    }
#ifdef _WIN32
    const intptr_t cancellationHandle =
        reinterpret_cast<intptr_t>(impl->cancellationEvent);
#else
    const intptr_t cancellationHandle = impl->cancellationRead;
#endif
    return ::media::Result<MediaRtpUdpIngressReceiveLease>::success(
        MediaRtpUdpIngressReceiveLease(
            std::static_pointer_cast<void>(impl),
            impl.get(),
            impl->rtp.nativeHandle(),
            impl->rtcp.nativeHandle(),
            cancellationHandle,
            impl->cancellableReadTimeoutMs,
            channel,
            sequence,
            &MediaRtpUdpTransport::ingressLeaseCancelled,
            &MediaRtpUdpTransport::ingressLeaseMarkReceived,
            &MediaRtpUdpTransport::ingressLeaseRelease));
}

std::shared_ptr<MediaRtpUdpTransport::Impl> MediaRtpUdpTransport::snapshot() const noexcept
{
    std::lock_guard lock(m_handleMutex);
    return m_impl;
}

bool MediaRtpUdpTransport::ingressLeaseCancelled(
    void* state, std::uint64_t sequence) noexcept
{
    auto* impl = static_cast<Impl*>(state);
    return !impl || impl->state.load(std::memory_order_acquire) !=
            Impl::State::Running ||
        impl->cancellationSequence.load(std::memory_order_acquire) != sequence;
}

void MediaRtpUdpTransport::ingressLeaseMarkReceived(
    void* state, MediaRtpUdpChannel channel) noexcept
{
    auto* impl = static_cast<Impl*>(state);
    if (impl) {
        std::lock_guard lifecycleLock(impl->lifecycleMutex);
        impl->preferRtcp = channel == MediaRtpUdpChannel::Rtp;
    }
}

void MediaRtpUdpTransport::ingressLeaseRelease(void* state, MediaRtpUdpChannel channel) noexcept
{
    auto* impl = static_cast<Impl*>(state);
    if (!impl) return;
    std::lock_guard lifecycleLock(impl->lifecycleMutex);
    impl->receiveActive[channel == MediaRtpUdpChannel::Rtp ? 0 : 1] = false;
    impl->receiveReleased.notify_all();
}

::media::Status MediaRtpUdpTransport::signalCancellation(
    const std::shared_ptr<Impl>& impl) noexcept
{
#ifdef _WIN32
    if (!WSASetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(mediaCleanupError(::media::ErrorCode::IoFailure,
            "RTP UDP cancellation signal failed", WSAGetLastError()));
    }
#else
    const uint8_t signal = 1;
    const ssize_t written = write(impl->cancellationWrite, &signal, sizeof(signal));
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        return ::media::Status::failure(mediaCleanupError(::media::ErrorCode::IoFailure,
            "RTP UDP cancellation signal failed", errno));
    }
#endif
    return ::media::Status::success();
}

::media::Status MediaRtpUdpTransport::resetCancellation(
    const std::shared_ptr<Impl>& impl) noexcept
{
#ifdef _WIN32
    if (!WSAResetEvent(impl->cancellationEvent)) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", WSAGetLastError()));
    }
#else
    uint8_t buffer[64];
    while (read(impl->cancellationRead, buffer, sizeof(buffer)) > 0) {
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "RTP UDP cancellation reset failed", errno));
    }
#endif
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
