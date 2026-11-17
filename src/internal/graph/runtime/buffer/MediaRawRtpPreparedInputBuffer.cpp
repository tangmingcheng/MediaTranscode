#include "internal/graph/utils/MediaCleanupError.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"
#include "internal/graph/protocol/rtp/ingress/MediaRtpIngressAdapterFactory.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

// Called with the prepared buffer mutex held; diagnostics must not alter capture.
void logPreparedCaptureBoundary(const MediaPreparedRawRtpInput& prepared,
    std::size_t bufferedBytes, const MediaRawRtpPreparedByteBudgetSnapshot& budget,
    const char* stage) noexcept
{
    if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
    try {
        std::string tail = " last_rtp=none";
        for (auto index = prepared.datagrams.size(); index > 0; --index) {
            const auto& entry = prepared.datagrams[index - 1];
            if (entry->datagram.channel != MediaRtpUdpChannel::Rtp) continue;
            auto packet = MediaRtpPacketParser::parse(entry->datagram.bytes);
            if (packet) {
                tail = " last_rtp_ssrc=" + std::to_string(packet.value().ssrc) +
                    " last_rtp_sequence=" + std::to_string(packet.value().sequenceNumber) +
                    " last_rtp_observed_ns=" + std::to_string(entry->observedAtNs);
            } else {
                tail = " last_rtp=unparseable";
            }
            break;
        }
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_prepared_capture_boundary stage=" + std::string(stage) +
                " stream=" + mediaGraphDiagnosticStreamKindName(prepared.identity.streamKind) +
                " capacity_bytes=" + std::to_string(budget.capacity) +
                " wire_capacity_bytes=" + std::to_string(budget.wireCapacity) +
                " observed_wire_bytes=" + std::to_string(budget.observedBytes) +
                " retained_bytes=" + std::to_string(budget.retainedBytes) +
                " stream_buffered_bytes=" + std::to_string(bufferedBytes) +
                " maximum_datagram_bytes=" + std::to_string(prepared.transport.maximumDatagramBytes()) +
                " queued_datagrams=" + std::to_string(prepared.datagrams.size()) +
                " probe_active=" + std::to_string(budget.probeActive) + tail);
    } catch (...) {
        // Allocation or formatting failure must not change media failure semantics.
    }
}

} // namespace

MediaRawRtpPreparedInputBuffer::MediaRawRtpPreparedInputBuffer(
    ConstructionKey, MediaPreparationBackingOwner workerBacking, MediaPreparedRawRtpInput prepared)
    : m_workerBacking(std::move(workerBacking)), m_prepared(std::move(prepared))
{
    for (const auto& datagram : m_prepared->datagrams) {
        m_bufferedBytes += datagram->datagram.bytes.size();
    }
    setStreamKind(m_prepared->identity.streamKind);
    setPayloadKind(MediaPayloadKind::FormatContext);
    setDiagnosticName("MediaRawRtpPreparedInputBuffer");
}

::media::Result<std::size_t> MediaRawRtpPreparedInputBuffer::workerStorageBytes()
{
    using Result = ::media::Result<std::size_t>;
    auto geometry = MediaThreadAllocationGeometry::current();
    if (!geometry) return Result::failure(geometry.error());
    auto perWorker = MediaCheckedArithmetic::add(geometry.value().invocationBytes,
        geometry.value().stopStateBytes, "Prepared thread startup allocations");
    if (!perWorker) return Result::failure(perWorker.error());
    constexpr auto workers = std::tuple_size_v<decltype(MediaPreparedRawRtpInput::receiveGeometries)>;
    auto total = MediaCheckedArithmetic::multiply(perWorker.value(), workers,
        "Prepared channel worker allocations");
    if (!total) return Result::failure(total.error());
    if (total.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed("Prepared thread storage exceeds addressable memory"));
    return Result::success(static_cast<std::size_t>(total.value()));
}

MediaRawRtpPreparedInputBuffer::~MediaRawRtpPreparedInputBuffer()
{
    abort();
}

::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>
MediaRawRtpPreparedInputBuffer::create(MediaPreparedRawRtpInput prepared) try
{
    std::size_t bufferedBytes = 0;
    if (!prepared.byteBudget || prepared.datagrams.resource() !=
            &prepared.byteBudget->storageResource()) {
        return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument("prepared RTP queue requires its admitted storage resource"));
    }
    for (std::size_t index = 0; index < prepared.receiveGeometries.size(); ++index) {
        const auto& geometry = prepared.receiveGeometries[index];
        if (auto status = geometry.validateProduct(); !status)
            return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(status.error());
        const auto channel = index == 0 ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp;
        if (geometry.channel() != channel ||
            geometry.maximumDatagramBytes() != prepared.transport.maximumDatagramBytes() ||
            geometry.socketReceiveCapacityBytes() != static_cast<std::size_t>(
                prepared.transport.effectiveReceiveBufferBytes(channel)))
            return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
                ::media::ErrorInfo::invalidArgument("prepared RTP receive geometry differs from its socket readback"));
    }
    std::uint64_t previousOrdinal = 0;
    for (const auto& datagram : prepared.datagrams) {
        if (!datagram || datagram->ordinal <= previousOrdinal ||
            datagram->ordinal > prepared.ingressFrontier ||
            datagram->datagram.bytes.resource() !=
                &prepared.byteBudget->storageResource()) {
            return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
                ::media::ErrorInfo::invalidArgument("prepared RTP payload requires its admitted storage resource"));
        }
        previousOrdinal = datagram->ordinal;
        bufferedBytes += datagram->datagram.bytes.size();
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
        bufferedBytes > prepared.byteBudget->snapshot().wireCapacity) {
        return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared input requires open transport and complete planned identity"));
    }
    for (const auto& datagram : prepared.datagrams) {
        if (auto status = prepared.replayClock->observe(
                datagram->observedAtNs); !status) {
            return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
                status.error());
        }
    }
    const auto allocation = prepared.byteBudget->allocationScope();
    auto workerBytes = workerStorageBytes();
    if (!workerBytes) return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(workerBytes.error());
    const auto* preparedAllocation = std::get_if<MediaPreparedLineageAllocation>(&allocation);
    if (!preparedAllocation || !preparedAllocation->resource)
        return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument("Prepared workers require their preparation allocation scope"));
    auto workerLease = preparedAllocation->resource->reserveStorage(
        workerBytes.value(), MediaStorageAdmission::RequiredAllocation);
    if (!workerLease) return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(workerLease.error());
    auto buffer = std::allocate_shared<MediaRawRtpPreparedInputBuffer>(
        MediaLineageAllocator<MediaRawRtpPreparedInputBuffer>(allocation),
        ConstructionKey{}, MediaPreparationBackingOwner(std::move(workerLease).value()), std::move(prepared));
    return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::success(
        std::move(buffer));
}

catch (const std::bad_alloc&) {
    return ::media::Result<std::shared_ptr<MediaRawRtpPreparedInputBuffer>>::failure(
        ::media::ErrorInfo::allocationFailed("prepared RTP queue allocation failed"));
}

MediaBufferType MediaRawRtpPreparedInputBuffer::type() const noexcept
{
    return MediaBufferType::RawRtpPreparedInput;
}

::media::Status MediaRawRtpPreparedInputBuffer::continuousPreparationStatusLocked(
    const std::shared_ptr<MediaPreparedRtpSourceSession>& expected) const
{
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    // A finite replay may use this prefix. Only continuous preparation requires
    // the original receiver to remain live beyond its raw capture capacity.
    if (m_captureTermination && m_captureTermination->reason == CaptureTermination::Reason::CapacityExhausted)
        return ::media::Status::failure(::media::ErrorInfo::ioFailure(
            "Continuous RTP preparation lost its receiver at the original raw capture capacity"
            " wire_capacity_bytes=" + std::to_string(m_captureTermination->wireCapacityBytes) +
            " observed_wire_bytes=" + std::to_string(m_captureTermination->observedWireBytes) +
            " maximum_datagram_bytes=" + std::to_string(m_captureTermination->maximumDatagramBytes)));
    if (m_stopped || m_captureThread.get_stop_token().stop_requested())
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Continuous RTP preparation receiver was stopped"));
    if (!m_prepared || m_replayActive || !m_captureStarted || m_captureFinished ||
        !m_captureThread.joinable() || m_continuousSource != expected ||
        (m_continuousSource && (m_continuousSource.owner_before(expected) || expected.owner_before(m_continuousSource))) ||
        (!expected && m_runtimeIngress))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Continuous RTP preparation requires its original live receiver and protocol owner"));
    return ::media::Status::success();
}

void MediaRawRtpPreparedInputBuffer::runPreflightWorker(
    void* context, std::stop_token stopToken, std::size_t) noexcept
{
    auto* owner = static_cast<MediaRawRtpPreparedInputBuffer*>(context);
    owner->capture(stopToken);
    std::shared_ptr<MediaPreparedRtpSourceSession> source;
    std::optional<::media::ErrorInfo> failure;
    {
        std::scoped_lock lock(owner->m_mutex);
        owner->m_captureFinished = true;
        source = owner->m_continuousSource;
        failure = owner->m_captureError;
    }
    owner->m_ready.notify_all();
    if (source && failure) source->fail(*failure);
}

void MediaRawRtpPreparedInputBuffer::runRuntimeWorker(
    void* context, std::stop_token stopToken, std::size_t channelIndex) noexcept
{
    static_cast<MediaRawRtpPreparedInputBuffer*>(context)->captureRuntime(stopToken, channelIndex);
}

::media::Status MediaRawRtpPreparedInputBuffer::startPreflightCapture()
{
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || m_stopped || m_replayActive ||
        m_captureThread.joinable()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP prepared capture can start exactly once before replay"));
    }
    m_captureThread = std::jthread(MediaThreadWorkerEntry{&runPreflightWorker, this, 0});
    m_captureStarted = true;
    m_captureFinished = false;
    return ::media::Status::success();
}

::media::Result<MediaRawRtpProbeLease>
MediaRawRtpPreparedInputBuffer::acquireProbeLease(
    const std::shared_ptr<const MediaRawRtpPreparedInputBuffer>& owner) const
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) {
        return ::media::Result<MediaRawRtpProbeLease>::failure(*m_captureError);
    }
    if (owner.get() != this || !m_prepared || m_stopped || m_replayActive) {
        return ::media::Result<MediaRawRtpProbeLease>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP probe requires a live prepared input before replay"));
    }
    return MediaRawRtpProbeLease::capture(
        m_prepared->datagrams, m_prepared->ingressFrontier, m_prepared->byteBudget, owner);
}

::media::Status MediaRawRtpPreparedInputBuffer::waitForProbeData(
    std::uint64_t previousIngressFrontier,
    std::chrono::steady_clock::time_point deadline,
    std::stop_token stopToken)
{
    // Register before taking the lock so return unlocks before joining an
    // in-flight stop callback. The shared mutex prevents a lost cancellation wake.
    std::stop_callback cancellation(stopToken, [this] {
        std::scoped_lock lock(m_mutex);
        m_ready.notify_all();
    });
    std::unique_lock lock(m_mutex);
    if (!m_prepared || m_stopped || m_replayActive ||
        !m_captureStarted ||
        previousIngressFrontier > m_prepared->ingressFrontier) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP probe wait requires its live preflight capture frontier"));
    }
    const auto budget = m_prepared->byteBudget->snapshot();
    if (budget.phase != MediaRawRtpPreparationPhase::Capture || budget.probeActive) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP probe must release its snapshot before waiting for more input"));
    }
    m_ready.wait_until(lock, deadline, [&] {
        return stopToken.stop_requested() || m_stopped || m_replayActive ||
            m_captureError || m_captureFinished ||
            m_prepared->ingressFrontier > previousIngressFrontier;
    });
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    if (auto status = m_prepared->byteBudget->validate(); !status) return status;
    if (m_prepared->byteBudget->snapshot().phase != MediaRawRtpPreparationPhase::Capture) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP probe input wait outlived preflight preparation"));
    }
    if (stopToken.stop_requested() || m_stopped || m_replayActive) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "raw RTP probe input wait was cancelled"));
    }
    if (std::chrono::steady_clock::now() >= deadline) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "raw RTP probe reached its original preparation deadline"));
    }
    if (m_prepared->ingressFrontier > previousIngressFrontier) {
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "raw RTP preflight capture ended before additional probe evidence"));
}

::media::Result<MediaPreparedRawRtpReplayInfo>
MediaRawRtpPreparedInputBuffer::beginReplay()
{
    std::unique_lock lock(m_mutex);
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
        if (datagram->datagram.channel == MediaRtpUdpChannel::Rtp) {
            ++info.rtpDatagrams;
        } else {
            ++info.rtcpDatagrams;
        }
        if (firstNs == 0) firstNs = datagram->observedAtNs;
        lastNs = datagram->observedAtNs;
    }
    if (lastNs >= firstNs) {
        info.arrivalSpanMilliseconds = (lastNs - firstNs) / 1'000'000;
    }
    if (auto status = m_prepared->byteBudget->requireSealed(); !status) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            status.error());
    }
    auto activated = m_prepared->replayClock->activate();
    if (!activated) {
        return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
            activated.error());
    }
    m_replayEpoch = activated.value();
    if (firstNs > 0) {
        info.replayOffsetMilliseconds =
            (firstNs - m_replayEpoch->sourceOriginNs) / 1'000'000;
    }
    if (m_runtimeIngress) {
        try {
            m_captureThread = std::jthread(MediaThreadWorkerEntry{&runRuntimeWorker, this, 0});
            m_secondaryCaptureThread = std::jthread(MediaThreadWorkerEntry{&runRuntimeWorker, this, 1});
        } catch (const std::system_error&) {
            lock.unlock();
            (void)stop();
            return ::media::Result<MediaPreparedRawRtpReplayInfo>::failure(
                ::media::ErrorInfo::allocationFailed("RTP channel receive worker creation failed"));
        }
    }
    m_replayActive = true;
    return ::media::Result<MediaPreparedRawRtpReplayInfo>::success(
        std::move(info));
}

::media::Status MediaRawRtpPreparedInputBuffer::discardReplayMedia()
{
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || !m_replayActive || m_stopped || m_continuousSource)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP replay purge requires an active runtime owner and completed capture"));
    // Preserve ordered RTCP facts; only media preceding this owner boundary retires.
    auto& datagrams = m_prepared->datagrams;
    datagrams.eraseIf([&](const auto& entry) {
        if (entry->datagram.channel != MediaRtpUdpChannel::Rtp) return false;
        m_bufferedBytes -= entry->datagram.bytes.size();
        return true;
    });
    return ::media::Status::success();
}

::media::Result<MediaPreparedRawRtpReplayDatagram>
MediaRawRtpPreparedInputBuffer::receive(int timeoutMs)
{
    if (timeoutMs <= 0) {
        return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP prepared replay requires a positive wait timeout"));
    }
    const auto waitDeadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeoutMs);
    std::unique_lock lock(m_mutex);
    while (true) {
        if (m_captureError) {
            return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                *m_captureError);
        }
        if (!m_prepared || !m_replayActive || m_stopped) {
            return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
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
            if (m_runtimeIngress) {
                return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                    ::media::ErrorInfo::wouldBlock(
                        "raw RTP prepared replay queue was consumed"));
            }
            if (!m_captureThread.joinable()) {
                return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "raw RTP prepared replay has no active socket capture"));
            }
            if (m_ready.wait_until(lock, waitDeadline) ==
                    std::cv_status::timeout &&
                m_prepared->datagrams.empty()) {
                return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                    ::media::ErrorInfo::wouldBlock(
                        "raw RTP prepared replay timed out"));
            }
            continue;
        }
        if (!m_replayEpoch) {
            return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP prepared replay epoch was not activated"));
        }
        const std::int64_t targetNs = m_replayEpoch->runtimeOriginNs;
        const std::int64_t nowNs = mediaSteadyClockNowNs();
        if (nowNs >= targetNs) {
            auto datagram =
                std::move(m_prepared->datagrams.front());
            m_prepared->datagrams.pop_front();
            m_bufferedBytes -= datagram->datagram.bytes.size();
            return ::media::Result<MediaPreparedRawRtpReplayDatagram>::success(
                {std::move(datagram), targetNs});
        }
        const auto target = std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(targetNs));
        const auto wakeAt = (std::min)(waitDeadline, target);
        if (m_ready.wait_until(lock, wakeAt) == std::cv_status::timeout &&
            wakeAt == waitDeadline && mediaSteadyClockNowNs() < targetNs) {
            return ::media::Result<MediaPreparedRawRtpReplayDatagram>::failure(
                ::media::ErrorInfo::wouldBlock(
                    "raw RTP prepared replay timed out"));
        }
    }
}

::media::Status MediaRawRtpPreparedInputBuffer::beginContinuousRuntime(
    const std::shared_ptr<MediaPreparedRtpSourceSession>& source)
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    if (!source || !m_continuousSource || source.get() != m_continuousSource.get() ||
        source.owner_before(m_continuousSource) || m_continuousSource.owner_before(source) ||
        !m_prepared || !m_prepared->datagrams.empty() || !m_continuousIngressPlan ||
        m_stopped || m_replayActive || !m_captureStarted || m_captureFinished)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Continuous RTP runtime requires its exact live source and consumed bootstrap prefix"));
    m_replayActive = true;
    m_preparedQueueConsumed = true;
    return ::media::Status::success();
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
    if (m_captureThread.joinable()) {
        return ::media::Result<MediaRtpIngressObservation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP ingress observation requires completed preflight capture"));
    }
    if (auto status = m_prepared->byteBudget->requireSealed(); !status) {
        return ::media::Result<MediaRtpIngressObservation>::failure(status.error());
    }
    return m_prepared->ingressObservation->seal();
}

::media::Result<MediaPreparedRtpIngressSnapshot>
MediaRawRtpPreparedInputBuffer::ingressSnapshot(
    const std::shared_ptr<const MediaRawRtpPreparedInputBuffer>& owner,
    const std::optional<MediaRtpIngressSnapshotWait>& wait)
{
    using Result = ::media::Result<MediaPreparedRtpIngressSnapshot>;
    // Register before locking: callback completion cannot join while holding
    // the mutex, and cancellation cannot be lost between predicate and sleep.
    std::stop_callback cancellation(wait ? wait->cancellation : std::stop_token{}, [this] {
        std::scoped_lock lock(m_mutex);
        m_ready.notify_all();
    });
    std::unique_lock lock(m_mutex);
    for (;;) {
        if (m_captureError) return Result::failure(*m_captureError);
        if (!owner || owner.get() != this || !m_prepared || !m_prepared->byteBudget ||
            !m_prepared->ingressObservation || m_stopped || m_replayActive)
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP ingress snapshot requires its exact active prepared source before replay"));
        if (auto status = m_prepared->byteBudget->validate(); !status)
            return Result::failure(status.error());
        if (wait && wait->cancellation.stop_requested())
            return Result::failure(::media::ErrorInfo::cancelled("RTP ingress snapshot wait cancelled"));
        if (wait && std::chrono::steady_clock::now() >= wait->deadline)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "RTP ingress snapshot reached the original preparation deadline"));
        auto observation = m_prepared->ingressObservation->snapshot();
        if (observation) {
            const auto& geometry = m_prepared->receiveGeometries[0];
            const auto socketBytes = geometry.socketReceiveCapacityBytes();
            const auto datagramBytes = geometry.maximumDatagramBytes();
            const auto wireBytes = m_prepared->byteBudget->snapshot().wireCapacity;
            if (socketBytes <= 0 || !datagramBytes || !wireBytes || !m_prepared->ingressFrontier)
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "RTP ingress snapshot requires observed source and transport capacity facts"));
            return Result::success({std::move(observation).value(), m_prepared->ingressFrontier,
                owner, static_cast<std::size_t>(socketBytes), wireBytes, datagramBytes, geometry});
        }
        if (!wait || observation.error().code != ::media::ErrorCode::WouldBlock)
            return Result::failure(observation.error());
        const auto budget = m_prepared->byteBudget->snapshot();
        if (!m_captureStarted || m_captureFinished || budget.phase != MediaRawRtpPreparationPhase::Capture || budget.probeActive)
            return Result::failure(::media::ErrorInfo::notInitialized(
                "RTP ingress observation cannot become ready in the current capture phase"));
        const auto frontier = m_prepared->ingressFrontier;
        m_ready.wait_until(lock, wait->deadline, [&] {
            return wait->cancellation.stop_requested() || m_stopped || m_replayActive ||
                m_captureError || m_captureFinished || !m_prepared ||
                m_prepared->ingressFrontier != frontier ||
                !m_prepared->byteBudget->validate();
        });
    }
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
        m_prepared->byteBudget->snapshot().wireCapacity);
}

::media::Result<MediaPreparedRtpStorageReservation>
MediaRawRtpPreparedInputBuffer::preparationStorage() const try
{
    using Result = ::media::Result<MediaPreparedRtpStorageReservation>;
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || !m_prepared->byteBudget)
        return Result::failure(::media::ErrorInfo::notInitialized("Prepared RTP storage has no budget owner"));
    if (auto status = m_prepared->byteBudget->requireSealed(); !status)
        return Result::failure(status.error());
    return Result::success(m_prepared->byteBudget->storageReservation());
}
catch (const std::bad_alloc&) {
    return ::media::Result<MediaPreparedRtpStorageReservation>::failure(
        ::media::ErrorInfo::allocationFailed("Prepared RTP storage evidence allocation failed"));
}

::media::Result<std::size_t>
MediaRawRtpPreparedInputBuffer::maximumDatagramBytes() const
{
    std::scoped_lock lock(m_mutex);
    if (m_captureError) {
        return ::media::Result<std::size_t>::failure(*m_captureError);
    }
    if (!m_prepared || m_stopped || m_replayActive) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP datagram capacity requires an active prepared transport before replay"));
    }
    const auto capacity = m_continuousIngressPlan ? m_continuousIngressPlan->maximumDatagramBytes()
        : m_prepared->transport.maximumDatagramBytes();
    if (capacity == 0) {
        return ::media::Result<std::size_t>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP prepared transport has no datagram capacity"));
    }
    return ::media::Result<std::size_t>::success(capacity);
}

::media::Result<std::uint64_t>
MediaRawRtpPreparedInputBuffer::sealedReplayAccessUnitBound() const
{
    using Result = ::media::Result<std::uint64_t>;
    std::scoped_lock lock(m_mutex);
    if (!m_prepared || m_stopped || m_replayActive || m_captureThread.joinable()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RTP replay retention requires completed capture before replay"));
    }
    if (m_captureError) return Result::failure(*m_captureError);
    if (auto status = m_prepared->byteBudget->requireSealed(); !status) {
        return Result::failure(status.error());
    }
    return accessUnitBoundLocked();
}

::media::Result<std::uint64_t>
MediaRawRtpPreparedInputBuffer::accessUnitBoundLocked() const
{
    using Result = ::media::Result<std::uint64_t>;
    const auto codec = canonicalCodecName(m_prepared->identity.codecName);
    const bool video = codec == "h264" || codec == "hevc";
    if (!video && codec != "aac" && codec != "opus") {
        return Result::failure(::media::ErrorInfo::unsupported(
            "RTP replay retention has no codec completion contract"));
    }
    // One video AU can straddle the sealed replay/live boundary.
    std::uint64_t units = video ? 1 : 0;
    for (const auto& entry : m_prepared->datagrams) {
        if (entry->datagram.channel != MediaRtpUdpChannel::Rtp) continue;
        auto packet = MediaRtpPacketParser::parse(entry->datagram.bytes);
        if (!packet) return Result::failure(packet.error());
        if (packet.value().payloadType != m_prepared->identity.payloadType) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "sealed RTP replay payload identity differs from its plan"));
        }
        std::uint64_t completed = video ? (packet.value().marker ? 1 : 0) : 1;
        if (codec == "aac") {
            auto headers = MediaAacRtpAuHeaderPlanner::plan(packet.value().payload, MediaRuntimeLineageAllocation{});
            if (!headers) return Result::failure(headers.error());
            // Counting every fragment header can overestimate, never undercount.
            completed = headers.value().accessUnits.size();
        }
        auto total = MediaCheckedArithmetic::add(units, completed,
            "sealed RTP replay access-unit retention");
        if (!total) return Result::failure(total.error());
        units = total.value();
    }
    return Result::success(units);
}

::media::Status MediaRawRtpPreparedInputBuffer::installRuntimeIngressLocked(
    const MediaRtpIngressPlan& plan)
{
    if (!m_prepared || m_runtimeIngress || m_rtcpIngress ||
        !m_prepared->receiveGeometries[0].sameProduct(plan.receiveGeometry()))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP receiver installation requires its original channel geometries"));
    for (std::size_t index = 0; index < m_prepared->receiveGeometries.size(); ++index) {
        const auto& geometry = m_prepared->receiveGeometries[index];
        if (auto status = geometry.validateProduct(); !status) return status;
        if (geometry.channel() != (index == 0 ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp))
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "RTP receiver geometry has mismatched channel ownership"));
    }
    if (auto status = m_prepared->transport.releaseReceiveStorage(); !status) return status;
    const auto allocation = m_prepared->byteBudget->allocationScope();
    auto transport = std::allocate_shared<MediaRtpUdpTransport>(
        MediaLineageAllocator<MediaRtpUdpTransport>(allocation), std::move(m_prepared->transport));
    std::array<std::optional<MediaRtpIngressReceiver>, 2> receivers;
    for (std::size_t index = 0; index < receivers.size(); ++index) {
        const auto& geometry = m_prepared->receiveGeometries[index];
        auto adapter = MediaRtpIngressAdapterFactory::create(transport, geometry, allocation);
        if (!adapter) return ::media::Status::failure(adapter.error());
        auto receiver = MediaRtpIngressReceiver::create(geometry, std::move(adapter).value(), allocation);
        if (!receiver) return ::media::Status::failure(receiver.error());
        receivers[index].emplace(std::move(receiver).value());
    }
    m_runtimeIngress.emplace(std::move(*receivers[0]));
    m_rtcpIngress.emplace(std::move(*receivers[1]));
    m_runtimeIngressPlan.emplace(plan);
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedInputBuffer::configureRuntimeIngress(
    const MediaRtpIngressPlan& plan) try
{
    if (auto status = plan.validateProduct(); !status) return status;
    std::unique_lock lock(m_mutex);
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    if (!m_prepared || m_stopped || m_replayActive || m_runtimeIngress ||
        m_captureThread.joinable())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "raw RTP runtime ingress can be configured exactly once before replay"));
    if (auto status = m_prepared->byteBudget->requireSealed(); !status) return status;
    if (auto status = installRuntimeIngressLocked(plan); !status) return status;
    return ::media::Status::success();
}
catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
        "RTP channel receiver allocation failed"));
}

::media::Status MediaRawRtpPreparedInputBuffer::validateRuntimeIngressPlan(
    const MediaRtpIngressPlan& plan) const
{
    std::scoped_lock lock(m_mutex);
    const auto& installed = m_continuousIngressPlan ? m_continuousIngressPlan : m_runtimeIngressPlan;
    if (!installed || !installed->sameProduct(plan)) {
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
    using Result = ::media::Result<MediaRtpIngressBatch>;
    std::unique_lock lock(m_mutex);
    if (timeoutMilliseconds <= 0 || !m_prepared || !m_replayActive || m_stopped ||
        !m_runtimeIngress || !m_rtcpIngress || !m_prepared->datagrams.empty())
        return Result::failure(::media::ErrorInfo::notInitialized(
            "raw RTP runtime batch receive requires consumed prepared replay and configured ingress"));
    m_ready.wait_for(lock, std::chrono::milliseconds(timeoutMilliseconds), [&] {
        return m_stopped || m_captureError || m_readyBatches[0] || m_readyBatches[1];
    });
    if (m_captureError) return Result::failure(*m_captureError);
    if (m_stopped) return Result::failure(::media::ErrorInfo::cancelled("RTP runtime receive was stopped"));
    if (!m_readyBatches[0] && !m_readyBatches[1])
        return Result::failure(::media::ErrorInfo::wouldBlock("RTP runtime receive timed out"));
    std::size_t selected = m_readyBatches[0] ? 0 : 1;
    if (m_readyBatches[0] && m_readyBatches[1] &&
        m_readyBatches[1]->entries().front().observedAtNanoseconds <
            m_readyBatches[0]->entries().front().observedAtNanoseconds) selected = 1;
    auto batch = std::move(*m_readyBatches[selected]);
    m_readyBatches[selected].reset();
    return Result::success(std::move(batch));
}

::media::Status MediaRawRtpPreparedInputBuffer::finishPreflightCapture()
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
    // join does not destroy the stop state. Retire it before constructing the
    // replacement runtime worker, keeping the admitted two-worker upper bound.
    m_captureThread = std::jthread{};
    if (m_captureError) return ::media::Status::failure(*m_captureError);
    logPreparedCaptureBoundary(*m_prepared, m_bufferedBytes,
        m_prepared->byteBudget->snapshot(), "sealed");
    return m_prepared->byteBudget->validate();
}

::media::Status MediaRawRtpPreparedInputBuffer::stop() noexcept
{
    {
        std::scoped_lock lock(m_mutex);
        m_stopped = true;
        m_captureThread.request_stop();
        m_secondaryCaptureThread.request_stop();
    }
    auto first = interruptReceive();
    if (m_captureThread.joinable()) m_captureThread.join();
    if (m_secondaryCaptureThread.joinable()) m_secondaryCaptureThread.join();
    if (m_runtimeIngress) {
        auto status = m_runtimeIngress->stop();
        if (!status && first) first = status;
    }
    if (m_rtcpIngress) {
        auto status = m_rtcpIngress->stop();
        if (!status && first) first = status;
    }
    return first;
}

::media::Status MediaRawRtpPreparedInputBuffer::interruptReceive() noexcept
{
    std::scoped_lock lock(m_mutex);
    m_stopped = true;
    m_captureThread.request_stop();
    m_secondaryCaptureThread.request_stop();
    m_ready.notify_all();
    if (!m_runtimeIngress)
        return m_prepared ? m_prepared->transport.interruptReceive()
            : ::media::Status::failure(mediaCleanupError(::media::ErrorCode::NotInitialized,
                "raw RTP prepared input has no owned transport", 0));
    auto first = m_runtimeIngress->interruptReceive();
    if (m_rtcpIngress) {
        auto status = m_rtcpIngress->interruptReceive();
        if (!status && first) first = status;
    }
    return first;
}

void MediaRawRtpPreparedInputBuffer::abort() noexcept
{
    {
        std::scoped_lock lock(m_mutex);
        m_stopped = true;
        m_captureThread.request_stop();
        m_secondaryCaptureThread.request_stop();
    }
    (void)interruptReceive();
    if (m_captureThread.joinable()) m_captureThread.join();
    if (m_secondaryCaptureThread.joinable()) m_secondaryCaptureThread.join();
    if (m_runtimeIngress) (void)m_runtimeIngress->abort();
    if (m_rtcpIngress) (void)m_rtcpIngress->abort();
    if (!m_runtimeIngress && m_prepared) (void)m_prepared->transport.abort();
}

::media::Status MediaRawRtpPreparedInputBuffer::startContinuousChannelsLocked(std::size_t currentChannel) try
{
    if (m_stopped || currentChannel >= 2 || !m_continuousSource || !m_continuousIngressPlan || m_secondaryCaptureThread.joinable())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Continuous channel handoff requires one original worker"));
    if (auto status = installRuntimeIngressLocked(*m_continuousIngressPlan); !status) return status;
    m_secondaryCaptureThread = std::jthread(MediaThreadWorkerEntry{&runRuntimeWorker, this, 1 - currentChannel});
    return ::media::Status::success();
}

catch (const std::system_error&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("prepared RTP channel worker creation failed"));
}
catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("prepared RTP channel installation allocation failed"));
}

::media::Status MediaRawRtpPreparedInputBuffer::deliverContinuousDatagram(
    const std::shared_ptr<MediaPreparedRtpSourceSession>& source, MediaStreamKind stream,
    MediaRtpUdpChannel channel, std::span<const std::uint8_t> bytes,
    std::int64_t observedAtNs, std::stop_token stopToken)
{
    const std::size_t channelIndex = channel == MediaRtpUdpChannel::Rtp ? 0 : 1;
    const auto& plan = stream == MediaStreamKind::Video ? source->plan().video : *source->plan().audio;
    const auto wakeup = plan.creditWakeups[channelIndex];
    if (!wakeup) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP channel lacks its credit wakeup"));
    std::stop_callback cancellation(stopToken, [wakeup] { wakeup->notify(); });
    bool accepted = false;
    bool observed = false;
    for (;;) {
        if (stopToken.stop_requested()) return ::media::Status::success();
        const auto wakeSequence = wakeup->sequence();
        MediaPreparedRtpIngressProgress progress;
        {
            // Exactly one entry admission/frontier publication is ordered.
            // Socket receive and credit waits never retain either lock.
            std::scoped_lock delivery(m_deliveryMutex);
            std::unique_lock lock(m_mutex);
            if (m_stopped || stopToken.stop_requested()) return ::media::Status::success();
            if (channel != m_prepared->receiveGeometries[channelIndex].channel())
                return ::media::Status::failure(::media::ErrorInfo::internalError("RTP receiver produced a datagram for another channel"));
            if (!accepted) {
                if (m_prepared->ingressFrontier == (std::numeric_limits<std::uint64_t>::max)())
                    return ::media::Status::failure(::media::ErrorInfo::invalidArgument("continuous RTP ingress ordinal overflow"));
                if (!observed && channel == MediaRtpUdpChannel::Rtp) {
                    if (bytes.size() < 4) return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RTP datagram lacks its sequence field"));
                    const auto sequence = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[2]) << 8) |
                        static_cast<std::uint16_t>(bytes[3]));
                    if (auto status = m_prepared->ingressObservation->observeDatagram(bytes.size(), sequence, observedAtNs); !status) return status;
                    observed = true;
                }
                const auto ordinal = m_prepared->ingressFrontier + 1;
                // The batch remains owned by this worker; stop joins it before
                // destroying receiver state. Source work may take generation
                // arbitration, so do not carry the receiver lock into it.
                lock.unlock();
                auto consumed = source->consume(stream, channel, bytes, observedAtNs, ordinal);
                if (!consumed) return ::media::Status::failure(consumed.error());
                progress = consumed.value();
                accepted = progress.state == MediaPreparedRtpIngressState::AcceptedReady ||
                    progress.state == MediaPreparedRtpIngressState::AcceptedWaitingCredit;
                lock.lock();
                // Commit the observed acceptance even if stop arrived while
                // source work was running. A committed datagram is never replayed.
                if (accepted) { m_prepared->ingressFrontier = ordinal; m_ready.notify_all(); }
                if (m_stopped || stopToken.stop_requested()) return ::media::Status::success();
            } else {
                lock.unlock();
                auto resumed = source->resumeDrain(stream, mediaSteadyClockNowNs());
                if (!resumed) return ::media::Status::failure(resumed.error());
                progress = resumed.value();
            }
        }
        if (progress.state == MediaPreparedRtpIngressState::AcceptedReady) break;
        auto timeout = source->creditWaitTimeout(stream, mediaSteadyClockNowNs());
        if (!timeout) return ::media::Status::failure(timeout.error());
        auto waited = wakeup->wait(wakeSequence, MediaNodeDeadlineWakePolicy::InputOrDeadline,
            timeout.value(), nullptr);
        if (!waited) return ::media::Status::failure(waited.error());
    }
    return ::media::Status::success();
}

::media::Status MediaRawRtpPreparedInputBuffer::captureRuntimeBatch(
    std::stop_token stopToken, std::size_t channelIndex)
{
    std::shared_ptr<MediaPreparedRtpSourceSession> source;
    MediaRtpIngressReceiver* receiver;
    MediaStreamKind stream;
    int timeoutMs;
    {
        std::scoped_lock lock(m_mutex);
        if (!m_prepared || m_stopped || !m_runtimeIngress || !m_rtcpIngress)
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "RTP reception requires both fixed channel receivers"));
        source = m_continuousSource;
        stream = m_prepared->identity.streamKind;
        timeoutMs = m_prepared->captureReadTimeoutMs;
        receiver = channelIndex == 0 ? &*m_runtimeIngress : &*m_rtcpIngress;
    }
    if (source && channelIndex == 0) {
        const auto& plan = stream == MediaStreamKind::Video ? source->plan().video : *source->plan().audio;
        const auto wakeup = plan.creditWakeups[channelIndex];
        if (!wakeup) return ::media::Status::failure(::media::ErrorInfo::notInitialized("RTP channel lacks its credit wakeup"));
        std::stop_callback cancellation(stopToken, [wakeup] { wakeup->notify(); });
        for (;;) {
            if (stopToken.stop_requested()) return ::media::Status::success();
            const auto sequence = wakeup->sequence();
            // This RTP owner has not acquired its next batch. Never report
            // processing time from inside a blocked entry's delivery loop.
            if (auto status = source->observeRtpCaptureIdle(stream, mediaSteadyClockNowNs()); !status) return status;
            auto resumed = source->resumeDrain(stream, mediaSteadyClockNowNs());
            if (!resumed) return ::media::Status::failure(resumed.error());
            if (resumed.value().state == MediaPreparedRtpIngressState::AcceptedReady) break;
            auto remaining = source->creditWaitTimeout(stream, mediaSteadyClockNowNs());
            if (!remaining) return ::media::Status::failure(remaining.error());
            auto waited = wakeup->wait(sequence, MediaNodeDeadlineWakePolicy::InputOrDeadline,
                remaining.value(), nullptr);
            if (!waited) return ::media::Status::failure(waited.error());
        }
    }
    if (auto released = receiver->waitForBatchRelease(stopToken); !released) return released;
    if (source) {
        auto timeout = source->receiveTimeoutMs(stream, mediaSteadyClockNowNs(), timeoutMs);
        if (!timeout) return ::media::Status::failure(timeout.error());
        timeoutMs = timeout.value();
    }
    auto batch = receiver->receiveNext(timeoutMs);
    if (!batch) {
        if (stopToken.stop_requested()) return ::media::Status::success();
        if (batch.error().code != ::media::ErrorCode::WouldBlock)
            return ::media::Status::failure(batch.error());
        if (!source) return ::media::Status::success();
        std::scoped_lock delivery(m_deliveryMutex);
        if (channelIndex == 0) {
            if (auto status = source->observeRtpCaptureIdle(stream, mediaSteadyClockNowNs()); !status) return status;
        }
        auto ticked = source->tick(stream, mediaSteadyClockNowNs());
        return ticked ? ::media::Status::success() : ::media::Status::failure(ticked.error());
    }
    if (!source) {
        std::scoped_lock lock(m_mutex);
        if (m_stopped || stopToken.stop_requested()) return ::media::Status::success();
        if (m_readyBatches[channelIndex]) return ::media::Status::failure(
            ::media::ErrorInfo::internalError("RTP channel published a second outstanding arena lease"));
        m_readyBatches[channelIndex].emplace(std::move(batch).value());
        m_ready.notify_all();
        return ::media::Status::success();
    }
    for (const auto& entry : batch.value().entries()) {
        if (entry.channel != (channelIndex == 0 ? MediaRtpUdpChannel::Rtp : MediaRtpUdpChannel::Rtcp))
            return ::media::Status::failure(::media::ErrorInfo::internalError("RTP receiver produced a datagram for another channel"));
        const std::span<const std::uint8_t> bytes{reinterpret_cast<const std::uint8_t*>(entry.bytes.data()), entry.bytes.size()};
        if (auto status = deliverContinuousDatagram(source, stream, entry.channel,
                bytes, entry.observedAtNanoseconds, stopToken); !status) return status;
    }
    auto ticked = source->tick(stream, mediaSteadyClockNowNs());
    return ticked ? ::media::Status::success() : ::media::Status::failure(ticked.error());
}

void MediaRawRtpPreparedInputBuffer::captureRuntime(
    std::stop_token stopToken, std::size_t channelIndex) noexcept
{
    try {
        while (!stopToken.stop_requested()) {
            auto status = captureRuntimeBatch(stopToken, channelIndex);
            if (status) continue;
            if (stopToken.stop_requested()) return;
            std::shared_ptr<MediaPreparedRtpSourceSession> source;
            {
                std::scoped_lock lock(m_mutex);
                if (!m_captureError) m_captureError = status.error();
                source = m_continuousSource;
            }
            m_ready.notify_all();
            (void)interruptReceive();
            if (source) source->fail(status.error());
            return;
        }
    } catch (const std::bad_alloc&) {
        std::shared_ptr<MediaPreparedRtpSourceSession> source;
        const auto error = ::media::ErrorInfo::allocationFailed("RTP receive pump allocation failed");
        {
            std::scoped_lock lock(m_mutex);
            if (!m_captureError) m_captureError = error;
            source = m_continuousSource;
        }
        m_ready.notify_all();
        (void)interruptReceive();
        if (source) source->fail(error);
    }
}

void MediaRawRtpPreparedInputBuffer::capture(
    std::stop_token stopToken) noexcept try
{
    while (!stopToken.stop_requested()) {
        bool continuous;
        {
            std::scoped_lock lock(m_mutex);
            continuous = static_cast<bool>(m_continuousSource);
        }
        if (continuous) {
            std::shared_ptr<MediaPreparedRtpSourceSession> source;
            auto started = ::media::Status::success();
            {
                std::scoped_lock lock(m_mutex);
                if (m_stopped || stopToken.stop_requested()) return;
                source = m_continuousSource;
                started = startContinuousChannelsLocked(0);
                if (!started) { m_captureError = started.error(); m_ready.notify_all(); }
            }
            if (!started) {
                source->fail(started.error());
                (void)interruptReceive();
                return;
            }
            captureRuntime(stopToken, 0);
            return;
        }
        int timeoutMs = 0;
        {
            std::scoped_lock lock(m_mutex);
            if (!m_prepared) return;
            const auto budget = m_prepared->byteBudget->snapshot();
            const std::size_t maximumDatagramBytes =
                m_prepared->transport.maximumDatagramBytes();
            if (maximumDatagramBytes == 0 ||
                budget.observedBytes > budget.wireCapacity ||
                maximumDatagramBytes >
                    budget.wireCapacity - budget.observedBytes) {
                logPreparedCaptureBoundary(*m_prepared, m_bufferedBytes, budget, "capacity_stop");
                m_captureTermination = CaptureTermination{CaptureTermination::Reason::CapacityExhausted,
                    budget.wireCapacity, budget.observedBytes, maximumDatagramBytes};
                m_captureFinished = true;
                if (budget.probeActive) {
                    m_captureError = ::media::ErrorInfo::allocationFailed(
                        "raw RTP capture exhausted shared storage while a probe snapshot was retained");
                    (void)m_prepared->byteBudget->fail(*m_captureError);
                }
                m_ready.notify_all();
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
            std::shared_ptr<MediaPreparedRtpSourceSession> source;
            {
                std::scoped_lock lock(m_mutex);
                m_captureError = received.error();
                source = m_continuousSource;
                if (m_prepared) (void)m_prepared->byteBudget->fail(*m_captureError);
                m_ready.notify_all();
            }
            if (source) source->fail(received.error());
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
        std::unique_lock lock(m_mutex);
        if (!m_prepared || m_stopped || stopToken.stop_requested()) return;
        if (m_continuousSource) {
            auto source = m_continuousSource;
            const auto stream = m_prepared->identity.streamKind;
            const std::size_t currentChannel = datagram.channel == MediaRtpUdpChannel::Rtp ? 0 : 1;
            auto started = startContinuousChannelsLocked(currentChannel);
            lock.unlock();
            if (!started) {
                source->fail(started.error());
                (void)interruptReceive();
                std::scoped_lock failed(m_mutex);
                m_captureError = started.error(); m_ready.notify_all();
                return;
            }
            {
                // Ordinary receive completed before the handoff was visible.
                // Keep its original bytes/time until typed admission accepts it;
                // the other channel already has its independent receiver/pump.
                auto finalDatagram = std::move(datagram);
                auto status = deliverContinuousDatagram(source, stream, finalDatagram.channel,
                    finalDatagram.bytes.view(), observedAtNs, stopToken);
                if (!status) {
                    source->fail(status.error());
                    (void)interruptReceive();
                    std::scoped_lock failed(m_mutex);
                    m_captureError = status.error(); m_ready.notify_all();
                    return;
                }
                rtpPacket.reset();
            }
            captureRuntime(stopToken, currentChannel);
            return;
        }
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
        if (auto status = datagram.channel == MediaRtpUdpChannel::Rtp
                ? m_prepared->ingressObservation->observeDatagram(
                    datagram.bytes.size(), sequenceNumber, observedAtNs) : ::media::Status::success(); !status) {
            m_captureError = status.error();
            (void)m_prepared->byteBudget->fail(*m_captureError);
            m_ready.notify_all();
            return;
        }
        if (auto status = m_prepared->byteBudget->observe(datagram.bytes.size()); !status) {
            m_captureError = status.error();
            m_ready.notify_all();
            return;
        }
        const auto bytes = datagram.bytes.size();
        m_prepared->datagrams.push_back(MediaPreparedRtpDatagramStorage::create(
            m_prepared->byteBudget->storageResource(),
            m_prepared->byteBudget->entryAllocationFacts(),
            std::move(datagram), observedAtNs, ++m_prepared->ingressFrontier));
        m_bufferedBytes += bytes;
        m_ready.notify_all();
    }
}

catch (const std::system_error&) {
    std::scoped_lock lock(m_mutex);
    m_captureError = ::media::ErrorInfo::allocationFailed("prepared RTP channel worker creation failed");
    m_ready.notify_all();
}
catch (const std::bad_alloc&) {
    std::scoped_lock lock(m_mutex);
    m_captureError = ::media::ErrorInfo::allocationFailed("prepared RTP capture allocation failed");
    if (m_prepared) (void)m_prepared->byteBudget->fail(*m_captureError);
    m_ready.notify_all();
}

} // namespace media::ffmpeg::graph
