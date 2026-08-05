#include "internal/graph/planner/realtime/MediaRawRtpInputPreparer.h"

#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaPreparedRawRtpProbe> MediaRawRtpInputPreparer::prepare(
    const MediaRawRtpProbePlan& plan)
{
    if (plan.openTimeoutMs <= 0 || plan.analyzeDurationUs <= 0 ||
        plan.maximumBufferedBytes == 0 ||
        plan.maximumBufferedBytes >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP probe requires explicit positive time and byte limits"));
    }
    auto observer = MediaRtpVideoSignalingObserver::create(
        plan.codecName, plan.payloadType, plan.clockRate);
    if (!observer) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            observer.error());
    }
    auto opened = MediaRtpUdpTransport::open(plan.transport);
    if (!opened) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            opened.error());
    }
    MediaRtpUdpTransport transport = std::move(opened).value();
    std::deque<MediaRtpUdpDatagram> buffered;
    std::size_t bufferedBytes = 0;
    std::size_t packetCount = 0;
    bool mediaEpochEstablished = false;

    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    std::optional<Clock::time_point> firstMatchingPacketAt;
    while (true) {
        const auto now = Clock::now();
        const auto openElapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(now - startedAt);
        if (openElapsed.count() >= plan.openTimeoutMs) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP video signaling probe reached open timeout before complete parameter sets"));
        }
        if (firstMatchingPacketAt &&
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - *firstMatchingPacketAt).count() >= plan.analyzeDurationUs) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                ::media::ErrorInfo::notInitialized(
                    "raw RTP video signaling probe reached analysis limit before complete parameter sets"));
        }

        int timeoutMs = std::min(
            plan.transport.cancellableReadTimeoutMs,
            std::max(1, plan.openTimeoutMs -
                static_cast<int>(openElapsed.count())));
        if (firstMatchingPacketAt) {
            const auto analysisElapsedUs =
                std::chrono::duration_cast<std::chrono::microseconds>(
                    now - *firstMatchingPacketAt).count();
            const auto remainingUs =
                static_cast<std::int64_t>(plan.analyzeDurationUs) -
                analysisElapsedUs;
            timeoutMs = std::min(timeoutMs, static_cast<int>(
                std::max<std::int64_t>(1, (remainingUs + 999) / 1000)));
        }
        auto received = transport.receive(timeoutMs);
        if (!received) {
            if (received.error().code == ::media::ErrorCode::WouldBlock) {
                continue;
            }
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                received.error());
        }
        MediaRtpUdpDatagram datagram = std::move(received).value();
        if (datagram.channel == MediaRtpUdpChannel::Rtcp) {
            if (bufferedBytes + datagram.bytes.size() >
                plan.maximumBufferedBytes) {
                return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                    ::media::ErrorInfo::allocationFailed(
                        "raw RTP signaling probe exceeded buffered byte limit"));
            }
            bufferedBytes += datagram.bytes.size();
            buffered.push_back(std::move(datagram));
            continue;
        }

        auto packet = MediaRtpPacketParser::parse(datagram.bytes);
        if (!packet) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                packet.error());
        }
        if (packet.value().payloadType != plan.payloadType) {
            continue;
        }
        if (!firstMatchingPacketAt) firstMatchingPacketAt = Clock::now();
        auto observation = observer.value().observe(packet.value());
        if (!observation) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                observation.error());
        }
        if (!mediaEpochEstablished || observation.value().epochChanged) {
            buffered.clear();
            bufferedBytes = 0;
            packetCount = 0;
            mediaEpochEstablished = true;
            firstMatchingPacketAt = Clock::now();
        }
        if (bufferedBytes + datagram.bytes.size() >
            plan.maximumBufferedBytes) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                ::media::ErrorInfo::allocationFailed(
                    "raw RTP signaling probe exceeded buffered byte limit"));
        }
        bufferedBytes += datagram.bytes.size();
        ++packetCount;
        buffered.push_back(std::move(datagram));
        if (!observation.value().complete) continue;

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt).count();
        auto detected = observer.value().detected(
            packetCount, bufferedBytes, elapsed);
        if (!detected) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                detected.error());
        }
        MediaDetectedRtpVideoSignaling signaling = detected.value();
        auto prepared = MediaPreparedRealtimeInput::createRawRtp({
            std::move(transport), std::move(buffered), detected.value()});
        if (!prepared) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                prepared.error());
        }
        return ::media::Result<MediaPreparedRawRtpProbe>::success({
            std::move(signaling), std::move(prepared).value()});
    }
}

} // namespace media::ffmpeg::graph
