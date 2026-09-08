#include "internal/graph/protocol/rtp/ingress/windows/MediaWindowsRtpIngressAdapter.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressStorage.h"

#include <array>
#include <atomic>
#include <chrono>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#endif

namespace media::ffmpeg::graph {

struct MediaWindowsRtpIngressAdapter::ReceiveState final {
#ifdef _WIN32
    struct Operation final {
        OVERLAPPED overlapped{};
        WSABUF buffer{};
        std::size_t slotIndex = 0;
        MediaRtpUdpChannel channel = MediaRtpUdpChannel::Rtp;
        bool pending = false;
    };

    struct StagedDatagram final {
        std::size_t slotIndex;
        MediaRtpUdpChannel channel;
        std::size_t bytes;
        std::int64_t observedAtNanoseconds;
    };

    ReceiveState(
        HANDLE completionPortValue,
        MediaRtpUdpIngressReceiveLease receiveLease,
        std::size_t descriptorCapacity)
        : completionPort(completionPortValue),
          lease(std::move(receiveLease)),
          staged(descriptorCapacity)
    {
        operations[0].channel = MediaRtpUdpChannel::Rtp;
        operations[1].channel = MediaRtpUdpChannel::Rtcp;
    }

    ~ReceiveState()
    {
        if (completionPort) CloseHandle(completionPort);
    }

    HANDLE completionPort = nullptr;
    std::optional<MediaRtpUdpIngressReceiveLease> lease;
    std::array<Operation, 2> operations{};
    std::vector<StagedDatagram> staged;
    MediaRtpIngressStorage* storage = nullptr;
#endif
    std::atomic<std::uint64_t> receiveCalls{0};
    std::atomic<std::uint64_t> completionWaits{0};
    std::atomic<std::uint64_t> completionDatagrams{0};
    std::atomic<std::uint64_t> drainedDatagrams{0};
    std::atomic<std::uint64_t> wouldBlocks{0};
    std::atomic<std::uint64_t> cancellations{0};
    std::atomic<std::uint64_t> truncations{0};
    std::atomic<std::uint64_t> pressureFailures{0};
    std::atomic<bool> terminated{false};
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
        "Windows RTP ingress batch receive was cancelled");
}

#ifdef _WIN32

constexpr ULONG_PTR kRtpCompletionKey = 1;
constexpr ULONG_PTR kRtcpCompletionKey = 2;
constexpr ULONG_PTR kCancellationCompletionKey = 3;

SOCKET socketFor(
    const MediaRtpUdpIngressReceiveLease& lease,
    MediaRtpUdpChannel channel) noexcept
{
    return static_cast<SOCKET>(
        channel == MediaRtpUdpChannel::Rtp
            ? lease.rtpHandle()
            : lease.rtcpHandle());
}

ULONG_PTR completionKey(MediaRtpUdpChannel channel) noexcept
{
    return channel == MediaRtpUdpChannel::Rtp
        ? kRtpCompletionKey
        : kRtcpCompletionKey;
}

std::int64_t steadyNowNanoseconds() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif

} // namespace

MediaWindowsRtpIngressAdapter::MediaWindowsRtpIngressAdapter(
    MediaRtpUdpTransport transport,
    std::unique_ptr<ReceiveState> receiveState,
    MediaRtpIngressPlanFacts planFacts) noexcept
    : m_transport(std::move(transport)),
      m_receiveState(std::move(receiveState)),
      m_planFacts(planFacts)
{
}

MediaWindowsRtpIngressAdapter::~MediaWindowsRtpIngressAdapter()
{
    (void)terminate(true);
    logDiagnostics("destroyed");
}

::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>
MediaWindowsRtpIngressAdapter::create(
    MediaRtpUdpTransport transport,
    const MediaRtpIngressPlan& plan)
{
#ifndef _WIN32
    (void)transport;
    (void)plan;
    return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
        ::media::ErrorInfo::notInitialized(
            "Windows RTP ingress adapter is unavailable on this platform"));
#else
    if (auto status = plan.validateProduct(); !status) {
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            status.error());
    }
    if (plan.adapterKind() !=
            MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue ||
        plan.storageOwnership() !=
            MediaRtpIngressStorageOwnership::ReusableMessageArena ||
        plan.cancellationContract() !=
            MediaRtpIngressCancellationContract::CompletionQueueWake ||
        plan.completionEvidence() !=
            MediaRtpIngressCompletionEvidence::OverlappedCompletionPort ||
        plan.descriptorCapacity() < 2 || !transport.isOpen() ||
        transport.effectiveReceiveBufferBytes() <= 0 ||
        plan.socketReceiveCapacityBytes() != static_cast<std::size_t>(
            transport.effectiveReceiveBufferBytes())) {
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            invalidAdapter(
                "Windows RTP ingress adapter requires the exact planner-selected IOCP and socket contract"));
    }
    auto leaseResult = transport.acquireIngressReceiveLease();
    if (!leaseResult) {
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            leaseResult.error());
    }
    auto lease = std::move(leaseResult).value();
    HANDLE completionPort = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!completionPort) {
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::ioFailure(
                "Windows RTP ingress completion port creation failed",
                static_cast<int>(GetLastError())));
    }
    const auto closePort = [&completionPort]() noexcept {
        if (completionPort) CloseHandle(completionPort);
        completionPort = nullptr;
    };
    const SOCKET rtp = static_cast<SOCKET>(lease.rtpHandle());
    const SOCKET rtcp = static_cast<SOCKET>(lease.rtcpHandle());
    if (!CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(rtp), completionPort,
            kRtpCompletionKey, 0) ||
        !CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(rtcp), completionPort,
            kRtcpCompletionKey, 0)) {
        const int error = static_cast<int>(GetLastError());
        closePort();
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::ioFailure(
                "Windows RTP ingress socket association with IOCP failed",
                error));
    }
    try {
        auto state = std::make_unique<ReceiveState>(
            completionPort, std::move(lease), plan.descriptorCapacity());
        completionPort = nullptr;
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::success(
            std::unique_ptr<MediaRtpIngressAdapter>(
                new MediaWindowsRtpIngressAdapter(
                    std::move(transport), std::move(state), plan.facts())));
    } catch (const std::bad_alloc&) {
        closePort();
        return ::media::Result<std::unique_ptr<MediaRtpIngressAdapter>>::failure(
            ::media::ErrorInfo::notInitialized(
                "Windows RTP ingress reusable completion allocation failed"));
    }
#endif
}

MediaRtpIngressAdapterKind
MediaWindowsRtpIngressAdapter::kind() const noexcept
{
    return MediaRtpIngressAdapterKind::WindowsOverlappedCompletionQueue;
}

::media::Result<std::size_t> MediaWindowsRtpIngressAdapter::receive(
    MediaRtpIngressStorage& storage,
    int timeoutMilliseconds)
{
#ifndef _WIN32
    (void)storage;
    (void)timeoutMilliseconds;
    return ::media::Result<std::size_t>::failure(
        ::media::ErrorInfo::notInitialized(
            "Windows RTP ingress adapter is unavailable on this platform"));
#else
    if (timeoutMilliseconds <= 0 || !m_receiveState ||
        m_receiveState->terminated.load(std::memory_order_acquire) ||
        !m_receiveState->lease ||
        storage.descriptorCapacity() != m_planFacts.descriptorCapacity ||
        storage.maximumDatagramBytes() != m_planFacts.maximumDatagramBytes ||
        (m_receiveState->storage && m_receiveState->storage != &storage)) {
        return ::media::Result<std::size_t>::failure(
            invalidAdapter(
                "Windows RTP ingress storage differs from the initialized planner product"));
    }
    m_receiveState->storage = &storage;
    m_receiveState->receiveCalls.fetch_add(1, std::memory_order_relaxed);
    auto& lease = *m_receiveState->lease;
    std::size_t stagedCount = 0;

    const auto failStaged = [&](::media::ErrorInfo error) {
        for (std::size_t index = 0; index < stagedCount; ++index) {
            if (auto status = storage.releaseReceiveSlot(
                    m_receiveState->staged[index].slotIndex);
                !status) {
                error = status.error();
                break;
            }
        }
        return ::media::Result<std::size_t>::failure(std::move(error));
    };
    const auto postOperation = [&](ReceiveState::Operation& operation)
        -> ::media::Status {
        if (operation.pending) return ::media::Status::success();
        auto slot = storage.acquireReceiveSlot();
        if (!slot) {
            if (slot.error().code == ::media::ErrorCode::WouldBlock) {
                return ::media::Status::success();
            }
            return ::media::Status::failure(slot.error());
        }
        operation.overlapped = OVERLAPPED{};
        operation.slotIndex = slot.value().slotIndex;
        operation.buffer.buf = reinterpret_cast<char*>(
            slot.value().bytes.data());
        operation.buffer.len = static_cast<ULONG>(
            slot.value().bytes.size());
        DWORD flags = 0;
        DWORD received = 0;
        const int result = WSARecv(
            socketFor(lease, operation.channel),
            &operation.buffer, 1, &received, &flags,
            &operation.overlapped, nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            const int error = WSAGetLastError();
            (void)storage.releaseReceiveSlot(operation.slotIndex);
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Windows RTP ingress overlapped receive submission failed",
                error));
        }
        operation.pending = true;
        return ::media::Status::success();
    };

    for (auto& operation : m_receiveState->operations) {
        if (auto status = postOperation(operation); !status) {
            return failStaged(status.error());
        }
    }
    if (!m_receiveState->operations[0].pending &&
        !m_receiveState->operations[1].pending) {
        m_receiveState->pressureFailures.fetch_add(
            1, std::memory_order_relaxed);
        return failStaged(::media::ErrorInfo::wouldBlock(
            "Windows RTP ingress has no free receive descriptor"));
    }

    std::array<OVERLAPPED_ENTRY, 2> completions{};
    ULONG removed = 0;
    m_receiveState->completionWaits.fetch_add(
        1, std::memory_order_relaxed);
    if (!GetQueuedCompletionStatusEx(
            m_receiveState->completionPort,
            completions.data(), static_cast<ULONG>(completions.size()),
            &removed, static_cast<DWORD>(timeoutMilliseconds), FALSE)) {
        const DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT) {
            m_receiveState->wouldBlocks.fetch_add(
                1, std::memory_order_relaxed);
            return failStaged(::media::ErrorInfo::wouldBlock(
                "Windows RTP ingress completion wait timed out"));
        }
        return failStaged(::media::ErrorInfo::ioFailure(
            "Windows RTP ingress completion wait failed",
            static_cast<int>(error)));
    }

    bool cancellationObserved = false;
    std::array<bool, 2> completedChannels{};
    for (ULONG index = 0; index < removed; ++index) {
        if (completions[index].lpOverlapped == nullptr &&
            completions[index].lpCompletionKey ==
                kCancellationCompletionKey) {
            cancellationObserved = true;
            continue;
        }
        ReceiveState::Operation* operation = nullptr;
        for (auto& candidate : m_receiveState->operations) {
            if (&candidate.overlapped == completions[index].lpOverlapped) {
                operation = &candidate;
                break;
            }
        }
        if (!operation || !operation->pending) {
            return failStaged(::media::ErrorInfo::internalError(
                "Windows RTP ingress IOCP returned an unknown completion"));
        }
        DWORD bytes = 0;
        DWORD flags = 0;
        operation->pending = false;
        if (!WSAGetOverlappedResult(
                socketFor(lease, operation->channel),
                &operation->overlapped, &bytes, FALSE, &flags)) {
            const int error = WSAGetLastError();
            (void)storage.releaseReceiveSlot(operation->slotIndex);
            if (error == WSA_OPERATION_ABORTED && lease.cancelled()) {
                cancellationObserved = true;
                continue;
            }
            if (error == WSAEMSGSIZE) {
                m_receiveState->truncations.fetch_add(
                    1, std::memory_order_relaxed);
            }
            return failStaged(::media::ErrorInfo::ioFailure(
                "Windows RTP ingress overlapped receive failed", error));
        }
        if (bytes == 0 || bytes > operation->buffer.len) {
            (void)storage.releaseReceiveSlot(operation->slotIndex);
            m_receiveState->truncations.fetch_add(
                1, std::memory_order_relaxed);
            return failStaged(::media::ErrorInfo::ioFailure(
                "Windows RTP ingress datagram was empty or truncated",
                WSAEMSGSIZE));
        }
        const std::size_t channelIndex =
            operation->channel == MediaRtpUdpChannel::Rtp ? 0 : 1;
        completedChannels[channelIndex] = true;
        m_receiveState->staged[stagedCount++] = {
            operation->slotIndex, operation->channel,
            static_cast<std::size_t>(bytes), steadyNowNanoseconds()};
    }
    if (cancellationObserved || lease.cancelled()) {
        m_receiveState->cancellations.fetch_add(
            1, std::memory_order_relaxed);
        return failStaged(cancelledReceive());
    }

    for (std::size_t channelIndex = 0;
         channelIndex < completedChannels.size(); ++channelIndex) {
        if (!completedChannels[channelIndex]) continue;
        const MediaRtpUdpChannel channel = channelIndex == 0
            ? MediaRtpUdpChannel::Rtp
            : MediaRtpUdpChannel::Rtcp;
        while (stagedCount < m_receiveState->staged.size()) {
            auto slot = storage.acquireReceiveSlot();
            if (!slot) {
                if (slot.error().code == ::media::ErrorCode::WouldBlock) break;
                return failStaged(slot.error());
            }
            const int received = recv(
                socketFor(lease, channel),
                reinterpret_cast<char*>(slot.value().bytes.data()),
                static_cast<int>(slot.value().bytes.size()), 0);
            if (received == SOCKET_ERROR) {
                const int error = WSAGetLastError();
                (void)storage.releaseReceiveSlot(slot.value().slotIndex);
                if (error == WSAEWOULDBLOCK) break;
                if (error == WSAEMSGSIZE) {
                    m_receiveState->truncations.fetch_add(
                        1, std::memory_order_relaxed);
                }
                return failStaged(::media::ErrorInfo::ioFailure(
                    "Windows RTP ingress nonblocking batch drain failed",
                    error));
            }
            if (received == 0) {
                (void)storage.releaseReceiveSlot(slot.value().slotIndex);
                return failStaged(::media::ErrorInfo::ioFailure(
                    "Windows RTP ingress received an empty datagram",
                    WSAEMSGSIZE));
            }
            m_receiveState->staged[stagedCount++] = {
                slot.value().slotIndex, channel,
                static_cast<std::size_t>(received),
                steadyNowNanoseconds()};
            m_receiveState->drainedDatagrams.fetch_add(
                1, std::memory_order_relaxed);
        }
    }

    for (auto& operation : m_receiveState->operations) {
        if (auto status = postOperation(operation); !status) {
            return failStaged(status.error());
        }
    }
    for (std::size_t index = 0; index < stagedCount; ++index) {
        const auto& datagram = m_receiveState->staged[index];
        if (auto status = storage.commitReceivedSlot(
                datagram.slotIndex, datagram.channel, datagram.bytes,
                datagram.observedAtNanoseconds); !status) {
            return ::media::Result<std::size_t>::failure(status.error());
        }
        lease.markReceived(datagram.channel);
    }
    m_receiveState->completionDatagrams.fetch_add(
        stagedCount, std::memory_order_relaxed);
    return ::media::Result<std::size_t>::success(stagedCount);
#endif
}

::media::Status
MediaWindowsRtpIngressAdapter::interruptReceive() noexcept
{
    auto status = m_transport.interruptReceive();
#ifdef _WIN32
    if (m_receiveState && m_receiveState->completionPort &&
        !PostQueuedCompletionStatus(
            m_receiveState->completionPort, 0,
            kCancellationCompletionKey, nullptr) && status) {
        status = ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Windows RTP ingress completion wake failed",
            static_cast<int>(GetLastError())));
    }
#endif
    return status;
}

::media::Status MediaWindowsRtpIngressAdapter::stop() noexcept
{
    auto status = terminate(false);
    logDiagnostics("stopped");
    return status;
}

::media::Status MediaWindowsRtpIngressAdapter::abort() noexcept
{
    auto status = terminate(true);
    logDiagnostics("aborted");
    return status;
}

::media::Status MediaWindowsRtpIngressAdapter::terminate(
    bool aborting) noexcept
{
    if (!m_receiveState || m_receiveState->terminated.exchange(
            true, std::memory_order_acq_rel)) {
        return ::media::Status::success();
    }
    ::media::Status first = aborting
        ? m_transport.abort()
        : m_transport.stop();
#ifdef _WIN32
    if (m_receiveState->lease) {
        auto& lease = *m_receiveState->lease;
        for (auto& operation : m_receiveState->operations) {
            if (!operation.pending) continue;
            if (!CancelIoEx(
                    reinterpret_cast<HANDLE>(
                        socketFor(lease, operation.channel)),
                    &operation.overlapped)) {
                const DWORD error = GetLastError();
                if (error != ERROR_NOT_FOUND && first) {
                    first = ::media::Status::failure(
                        ::media::ErrorInfo::ioFailure(
                            "Windows RTP ingress cancellation failed",
                            static_cast<int>(error)));
                }
            }
        }
        (void)PostQueuedCompletionStatus(
            m_receiveState->completionPort, 0,
            kCancellationCompletionKey, nullptr);
        while (m_receiveState->operations[0].pending ||
               m_receiveState->operations[1].pending) {
            OVERLAPPED_ENTRY entries[2]{};
            ULONG removed = 0;
            if (!GetQueuedCompletionStatusEx(
                    m_receiveState->completionPort, entries, 2,
                    &removed, INFINITE, FALSE)) {
                if (first) {
                    first = ::media::Status::failure(
                        ::media::ErrorInfo::ioFailure(
                            "Windows RTP ingress cancellation drain failed",
                            static_cast<int>(GetLastError())));
                }
                break;
            }
            for (ULONG index = 0; index < removed; ++index) {
                if (!entries[index].lpOverlapped) continue;
                for (auto& operation : m_receiveState->operations) {
                    if (&operation.overlapped !=
                            entries[index].lpOverlapped ||
                        !operation.pending) {
                        continue;
                    }
                    operation.pending = false;
                    if (m_receiveState->storage) {
                        (void)m_receiveState->storage->releaseReceiveSlot(
                            operation.slotIndex);
                    }
                }
            }
        }
        m_receiveState->lease.reset();
    }
#endif
    return first;
}

void MediaWindowsRtpIngressAdapter::logDiagnostics(
    const char* stage) noexcept
{
    if (!m_receiveState ||
        m_receiveState->diagnosticsLogged.exchange(
            true, std::memory_order_acq_rel)) {
        return;
    }
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        std::string("rtp_ingress_adapter stage=") + stage +
            " adapter=windows_overlapped_completion_queue" +
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
            " completion_waits=" + std::to_string(
                m_receiveState->completionWaits.load(std::memory_order_relaxed)) +
            " completed_datagrams=" + std::to_string(
                m_receiveState->completionDatagrams.load(std::memory_order_relaxed)) +
            " drained_datagrams=" + std::to_string(
                m_receiveState->drainedDatagrams.load(std::memory_order_relaxed)) +
            " would_block=" + std::to_string(
                m_receiveState->wouldBlocks.load(std::memory_order_relaxed)) +
            " cancellations=" + std::to_string(
                m_receiveState->cancellations.load(std::memory_order_relaxed)) +
            " truncations=" + std::to_string(
                m_receiveState->truncations.load(std::memory_order_relaxed)) +
            " pressure_failures=" + std::to_string(
                m_receiveState->pressureFailures.load(std::memory_order_relaxed)) +
            " storage_reuse=preallocated post_start_allocation_path=none");
}

} // namespace media::ffmpeg::graph
