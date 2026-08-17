#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapterFactory.h"
#include "internal/graph/time/MediaSteadyClock.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

MediaRawRtpPreparedInputBuffer::MediaRawRtpPreparedInputBuffer(
    MediaPreparedRawRtpInput prepared)
    : m_prepared(std::move(prepared))
{
    for (const auto& datagram : m_prepared->datagrams) {
        m_bufferedBytes += datagram.datagram.bytes.size();
    }
    setStreamKind(m_prepared->identity.streamKind);
    setPayloadKind(MediaPayloadKind::FormatContext);
    setDiagnosticName("MediaRawRtpPreparedInputBuffer");
}

MediaRawRtpPreparedInputBuffer::~MediaRawRtpPreparedInputBuffer()
{
    abort();
    if (m_budgetReserved && m_prepared && m_prepared->byteBudget) {
        (void)m_prepared->byteBudget->release(m_bufferedBytes);
        m_budgetReserved = false;
    }
}

::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>
MediaRawRtpPreparedInputBuffer::create(MediaPreparedRawRtpInput prepared)
{
    std::size_t bufferedBytes = 0;
    for (const auto& datagram : prepared.datagrams) {
        bufferedBytes += datagram.datagram.bytes.size();
    }
    const bool validStream =
        prepared.identity.streamKind == MediaStreamKind::Video ||
        prepared.identity.streamKind == MediaStreamKind::Audio;
    const bool validVideoSignaling =
        prepared.identity.streamKind != MediaStreamKind::Video ||
        (prepared.videoSignaling &&
         prepared.videoSignaling->packetCount > 0 &&
         prepared.videoSignaling->datagramBytes > 0);
    if (!prepared.transport.isOpen() || !validStream ||
        prepared.identity.codecName.empty() ||
        prepared.identity.clockRate <= 0 || !validVideoSignaling ||
        !prepared.replayClock || !prepared.byteBudget ||
        !prepared.ingressObservation ||
        prepared.captureReadTimeoutMs <= 0 ||
        bufferedBytes > prepared.byteBudget->snapshot().capacity) {
        return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared input requires open transport and complete planned identity"));
    }
    for (const auto& datagram : prepared.datagrams) {
        if (auto status = prepared.replayClock->observe(
                datagram.observedAtNs); !status) {
            return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
                status.error());
        }
    }
    auto buffer = std::unique_ptr<MediaRawRtpPreparedInputBuffer>(
        new MediaRawRtpPreparedInputBuffer(std::move(prepared)));
    const char* stream = buffer->streamKind() == MediaStreamKind::Video
        ? "video" : "audio";
    if (auto status = buffer->m_prepared->byteBudget->retain(
            bufferedBytes, stream); !status) {
        return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            status.error());
    }
    buffer->m_budgetReserved = true;
    return ::media::Result<std::unique_ptr<MediaRawRtpPreparedInputBuffer>>::success(
        std::move(buffer));
}

MediaBufferType MediaRawRtpPreparedInputBuffer::type() const noexcept
{
    return MediaBufferType::RawRtpPreparedInput;
}

::media::Status MediaRawRtpPreparedInputBuffer::startPreflightCapture()
{
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || m_stopped || m_replayActive ||
        m_captureThread.joinable()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared capture can start exactly once before replay"));
    }
    m_captureThread = std::jthread(
        [owner = this](std::stop_token stopToken) {
            owner->capture(stopToken);
        });
    return ::media::Status::success();
}

::media::Result<MediaPreparedRawRtpReplayInfo>
MediaRawRtpPreparedInputBuffer::beginReplay()
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            *m_captureError);
    }
    if (!m_prepared) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared replay has no owned transport"));
    }
    if (m_replayActive || m_stopped) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared replay can begin only once while running"));
    }
    MediaPreparedRawRtpReplayInfo info;
    info.identity = m_prepared->identity;
    info.videoSignaling = m_prepared->videoSignaling;
    std::int64_t firstNs = 0;
    std::int64_t lastNs = 0;
    for (const auto& datagram : m_prepared->datagrams) {
        if (datagram.datagram.channel == MediaRtpUdpChannel::Rtp) {
            ++info.rtpDatagrams;
        } else {
            ++info.rtcpDatagrams;
        }
        if (firstNs == 0) firstNs = datagram.observedAtNs;
        lastNs = datagram.observedAtNs;
    }
    if (lastNs >= firstNs) {
        info.arrivalSpanMilliseconds = (lastNs - firstNs) / 1'000'000;
    }
    auto activated = m_prepared->replayClock->activate();
    if (!activated) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            activated.error());
    }
    if (auto status = m_prepared->byteBudget->requireSealed(); !status) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            status.error());
    }
    m_replayEpoch = activated.value();
    if (firstNs > 0) {
        info.replayOffsetMilliseconds =
            (firstNs - m_replayEpoch->sourceOriginNs) / 1'000'000;
    }
    m_replayActive = true;
    return ::media::Result<MediaPreparedRawRtpReplayInfo>::success(
        std::move(info));
}

::media::Result<MediaPreparedRawRtpDatagram>
MediaRawRtpPreparedInputBuffer::receive(int timeoutMs)
{
    if (timeoutMs <= 0) {
        return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared replay requires a positive wait timeout"));
    }
    const auto waitDeadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeoutMs);
    std::unique_lock lock(m_mutex);
    while (true) {
        if (m_captureError) {
            return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                *m_captureError);
        }
        if (!m_prepared || !m_replayActive || m_stopped) {
            return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                ::media::ErrorInfo::cancelled(
                    "raw RTP prepared replay was stopped"));
        }
        if (m_prepared->datagrams.empty()) {
            if (!m_preparedQueueConsumed) {
                m_preparedQueueConsumed = true;
                mediaGraphDiagnosticLog(
                    MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::RuntimeNode,
                    "rtp_prepared_queue_consumed stream=" +
                        std::string(m_prepared->identity.streamKind ==
                                MediaStreamKind::Video ? "video" : "audio"));
            }
            if (!m_captureThread.joinable()) {
                if (m_runtimeIngress) {
                    return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                        ::media::ErrorInfo::wouldBlock(
                            "raw RTP prepared replay queue was consumed"));
                }
                return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "raw RTP prepared replay has no active socket capture"));
            }
            if (m_ready.wait_until(lock, waitDeadline) ==
                    std::cv_status::timeout &&
                m_prepared->datagrams.empty()) {
                return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                    ::media::ErrorInfo::wouldBlock(
                        "raw RTP prepared replay timed out"));
            }
            continue;
        }
        if (!m_replayEpoch) {
            return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP prepared replay epoch was not activated"));
        }
        const std::int64_t targetNs = m_replayEpoch->runtimeOriginNs;
        const std::int64_t nowNs = mediaSteadyClockNowNs();
        if (nowNs >= targetNs) {
            MediaPreparedRawRtpDatagram datagram =
                std::move(m_prepared->datagrams.front());
            m_prepared->datagrams.pop_front();
            if (auto status = m_prepared->byteBudget->release(
                    datagram.datagram.bytes.size()); !status) {
                return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                    status.error());
            }
            m_bufferedBytes -= datagram.datagram.bytes.size();
            datagram.observedAtNs = targetNs;
            return ::media::Result<MediaPreparedRawRtpDatagram>::success(
                std::move(datagram));
        }
        const auto target = std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(targetNs));
        const auto wakeAt = (std::min)(waitDeadline, target);
        if (m_ready.wait_until(lock, wakeAt) == std::cv_status::timeout &&
            wakeAt == waitDeadline && mediaSteadyClockNowNs() < targetNs) {
            return ::media::Result<MediaPreparedRawRtpDatagram>::failure(
                ::media::ErrorInfo::wouldBlock(
                    "raw RTP prepared replay timed out"));
        }
    }
}

::media::Status MediaRawRtpPreparedInputBuffer::captureStatus()
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    if (!m_prepared) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "raw RTP prepared capture has no owned transport"));
    }
    return m_prepared->byteBudget->validate();
}

::media::Result<MediaRtpIngressObservation>
MediaRawRtpPreparedInputBuffer::ingressObservation()
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) {
        return ::media::Result<MediaRtpIngressObservation>::failure(
            *m_captureError);
    }
    if (!m_prepared || !m_prepared->ingressObservation) {
        return ::media::Result<MediaRtpIngressObservation>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared input has no ingress observation collector"));
    }
    return m_prepared->ingressObservation->seal();
}

::media::Result<std::size_t>
MediaRawRtpPreparedInputBuffer::preparedByteCapacity() const
{
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || !m_prepared->byteBudget) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared byte capacity requires an owned byte budget"));
    }
    return ::media::Result<std::size_t>::success(
        m_prepared->byteBudget->snapshot().capacity);
}

::media::Result<std::size_t>
MediaRawRtpPreparedInputBuffer::effectiveSocketReceivePayloadBytes() const
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) {
        return ::media::Result<std::size_t>::failure(
            *m_captureError);
    }
    if (!m_prepared || m_stopped || m_replayActive) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP socket capacity requires an active prepared transport before replay"));
    }
    const int capacity = m_prepared->transport.effectiveReceiveBufferBytes();
    if (capacity <= 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared transport has no effective receive capacity"));
    }
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(capacity));
}

::media::Status MediaRawRtpPreparedInputBuffer::configureRuntimeIngress(
    const MediaRtpIngressPlan& plan)
{
    if (auto status = plan.validateProduct(); !status) return status;
    {
        std::scoped_lock lock(m_mutex);
        if (m_captureError) return ::media::Status::failure(*m_captureError);
        if (!m_prepared || m_stopped || m_replayActive || m_runtimeIngress ||
            m_captureThread.joinable()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP runtime ingress can be configured exactly once before replay"));
        }
        if (auto status = m_prepared->byteBudget->requireSealed(); !status) {
            return status;
        }
    }
    auto adapter = MediaRtpIngressAdapterFactory::create(
        std::move(m_prepared->transport), plan);
    if (!adapter) return ::media::Status::failure(adapter.error());
    auto receiver = MediaRtpIngressReceiver::create(
        plan, std::move(adapter).value());
    if (!receiver) return ::media::Status::failure(receiver.error());
    {
        std::scoped_lock lock(m_mutex);
        if (m_captureError) return ::media::Status::failure(*m_captureError);
        m_runtimeIngress.emplace(std::move(receiver).value());
        m_runtimeIngressPlan.emplace(plan);
    }
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedInputBuffer::validateRuntimeIngressPlan(
    const MediaRtpIngressPlan& plan) const
{
    std::scoped_lock lock(m_mutex);
    if (!m_runtimeIngressPlan ||
        !m_runtimeIngressPlan->sameProduct(plan)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP node ingress contract differs from its prepared runtime binding"));
    }
    return ::media::Status::success();
}

bool MediaRawRtpPreparedInputBuffer::preparedReplayDrained() const noexcept
{
    std::scoped_lock lock(m_mutex);
    return m_prepared && m_replayActive && m_prepared->datagrams.empty();
}

::media::Result<MediaRtpIngressBatch>
MediaRawRtpPreparedInputBuffer::receiveRuntimeBatch(int timeoutMilliseconds)
{
    MediaRtpIngressReceiver* receiver = nullptr;
    {
        std::scoped_lock lock(m_mutex);
        if (!m_prepared || !m_replayActive || m_stopped ||
            !m_runtimeIngress || !m_prepared->datagrams.empty()) {
            return ::media::Result<MediaRtpIngressBatch>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP runtime batch receive requires consumed prepared replay and configured ingress"));
        }
        receiver = &*m_runtimeIngress;
    }
    return receiver->receiveNext(timeoutMilliseconds);
}

::media::Status MediaRawRtpPreparedInputBuffer::sealPreflight()
{
    {
        std::scoped_lock lock(m_mutex);
        if (m_captureError) return ::media::Status::failure(*m_captureError);
        if (!m_prepared || m_stopped || m_replayActive ||
            !m_captureThread.joinable()) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "raw RTP prepared capture can be sealed exactly once before replay"));
        }
        m_captureThread.request_stop();
    }
    if (auto status = m_prepared->transport.stop(); !status) return status;
    m_captureThread.join();
    if (auto status = m_prepared->transport.reset(); !status) return status;
    std::scoped_lock lock(m_mutex);
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    return m_prepared->byteBudget->sealPreflight();
}

::media::Status MediaRawRtpPreparedInputBuffer::stop() noexcept
{
    m_captureThread.request_stop();
    {
        std::scoped_lock lock(m_mutex);
        if (m_runtimeIngress) {
            m_stopped = true;
            m_ready.notify_all();
            return m_runtimeIngress->stop();
        }
    }
    MediaRtpUdpTransport* transport = markStopped();
    auto transportStatus = transport
        ? transport->stop()
        : ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared input has no owned transport"));
    if (m_captureThread.joinable()) m_captureThread.join();
    return transportStatus;
}

::media::Status MediaRawRtpPreparedInputBuffer::interruptReceive() noexcept
{
    {
        std::scoped_lock lock(m_mutex);
        if (m_runtimeIngress) return m_runtimeIngress->interruptReceive();
    }
    MediaRtpUdpTransport* transport = markStopped();
    if (!transport) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared input has no owned transport"));
    }
    return transport->interruptReceive();
}

void MediaRawRtpPreparedInputBuffer::abort() noexcept
{
    m_captureThread.request_stop();
    {
        std::scoped_lock lock(m_mutex);
        if (m_runtimeIngress) {
            m_stopped = true;
            (void)m_runtimeIngress->abort();
            m_ready.notify_all();
            return;
        }
    }
    MediaRtpUdpTransport* transport = markStopped();
    if (transport) (void)transport->abort();
    if (m_captureThread.joinable()) m_captureThread.join();
}

MediaRtpUdpTransport*
MediaRawRtpPreparedInputBuffer::markStopped() noexcept
{
    MediaRtpUdpTransport* transport = nullptr;
    {
        std::scoped_lock lock(m_mutex);
        m_stopped = true;
        if (m_prepared) transport = &m_prepared->transport;
    }
    m_ready.notify_all();
    return transport;
}

void MediaRawRtpPreparedInputBuffer::capture(
    std::stop_token stopToken) noexcept
{
    while (!stopToken.stop_requested()) {
        int timeoutMs = 0;
        {
            std::scoped_lock lock(m_mutex);
            if (!m_prepared) return;
            const auto budget = m_prepared->byteBudget->snapshot();
            const std::size_t maximumDatagramBytes =
                m_prepared->transport.maximumDatagramBytes();
            if (maximumDatagramBytes == 0 ||
                budget.retainedBytes > budget.capacity ||
                maximumDatagramBytes >
                    budget.capacity - budget.retainedBytes) {
                return;
            }
            timeoutMs = m_prepared->captureReadTimeoutMs;
        }
        auto received = m_prepared->transport.receive(timeoutMs);
        if (!received) {
            if (received.error().code == ::media::ErrorCode::WouldBlock) {
                continue;
            }
            if (stopToken.stop_requested()) return;
            std::scoped_lock lock(m_mutex);
            m_captureError = received.error();
            if (m_prepared) {
                (void)m_prepared->byteBudget->fail(*m_captureError);
            }
            m_ready.notify_all();
            return;
        }
        const auto observedAt = std::chrono::steady_clock::now();
        const std::int64_t observedAtNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                observedAt.time_since_epoch()).count();
        MediaRtpUdpDatagram datagram = std::move(received).value();
        std::optional<MediaRtpPacket> rtpPacket;
        if (datagram.channel == MediaRtpUdpChannel::Rtp) {
            auto parsed = MediaRtpPacketParser::parse(datagram.bytes);
            if (!parsed) {
                std::scoped_lock lock(m_mutex);
                m_captureError = parsed.error();
                if (m_prepared) {
                    (void)m_prepared->byteBudget->fail(*m_captureError);
                }
                m_ready.notify_all();
                return;
            }
            rtpPacket.emplace(std::move(parsed).value());
        }
        std::scoped_lock lock(m_mutex);
        if (!m_prepared) return;
        if (auto status = m_prepared->replayClock->observe(
                observedAtNs); !status) {
            m_captureError = status.error();
            (void)m_prepared->byteBudget->fail(*m_captureError);
            m_ready.notify_all();
            return;
        }
        if (rtpPacket) {
            if (rtpPacket->payloadType != m_prepared->identity.payloadType) {
                m_captureError = ::media::ErrorInfo::invalidArgument(
                    "raw RTP prepared capture payload type differs from planned identity");
                (void)m_prepared->byteBudget->fail(*m_captureError);
                m_ready.notify_all();
                return;
            }
        }
        const auto sequenceNumber = rtpPacket
            ? std::optional<std::uint16_t>(rtpPacket->sequenceNumber)
            : std::nullopt;
        if (auto status = m_prepared->ingressObservation->observeDatagram(
                datagram.bytes.size(), sequenceNumber,
                observedAtNs); !status) {
            m_captureError = status.error();
            (void)m_prepared->byteBudget->fail(*m_captureError);
            m_ready.notify_all();
            return;
        }
        const char* stream = m_prepared->identity.streamKind ==
                MediaStreamKind::Video ? "video" : "audio";
        if (auto status = m_prepared->byteBudget->observeAndRetain(
                datagram.bytes.size(), stream); !status) {
            m_captureError = status.error();
            m_ready.notify_all();
            return;
        }
        m_bufferedBytes += datagram.bytes.size();
        m_prepared->datagrams.push_back(MediaPreparedRawRtpDatagram{
            std::move(datagram), observedAtNs});
        m_ready.notify_all();
    }
}

} // namespace media::ffmpeg::graph
