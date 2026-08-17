#include "internal/graph/planner/realtime/MediaRawRtpInputPreparer.h"

#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/protocol/rtp/MediaRtpReorderBuffer.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoFrameRateObserver.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo incompleteSignalingError(
    std::string reason,
    const MediaRtpVideoSignalingEvidence& evidence,
    std::size_t packetCount,
    std::size_t datagramBytes)
{
    reason += "; matching_rtp_packets=" + std::to_string(packetCount) +
        " matching_datagram_bytes=" + std::to_string(datagramBytes) +
        " parameter_sets=";
    bool emitted = false;
    const auto append = [&reason, &emitted](const char* name, bool present) {
        if (!present) return;
        if (emitted) reason += ",";
        reason += name;
        emitted = true;
    };
    append("vps", evidence.hasVps);
    append("sps", evidence.hasSps);
    append("pps", evidence.hasPps);
    if (!emitted) reason += "none";
    return ::media::ErrorInfo::notInitialized(std::move(reason));
}

::media::ErrorInfo payloadTypeConflictError(
    std::uint8_t expectedPayloadType,
    std::uint8_t observedPayloadType)
{
    return ::media::ErrorInfo::invalidArgument(
        "raw RTP video signaling identity conflict: expected payload type " +
        std::to_string(expectedPayloadType) + " but observed " +
        std::to_string(observedPayloadType));
}

::media::Result<bool> rtcpBelongsToSsrc(
    std::span<const std::uint8_t> datagram,
    const MediaRtcpCompoundPolicy& policy,
    const std::optional<std::uint32_t>& expectedSsrc)
{
    auto parsed = MediaRtcpCompoundParser::parse(datagram, policy);
    if (!parsed) {
        return ::media::Result<bool>::failure(parsed.error());
    }
    if (!expectedSsrc) {
        return ::media::Result<bool>::success(false);
    }
    bool referencesExpected = false;
    const auto requireIdentity = [&](std::uint32_t ssrc) {
        if (ssrc != *expectedSsrc) return false;
        referencesExpected = true;
        return true;
    };
    for (const MediaRtcpPacket& packet : parsed.value()) {
        if (packet.senderReport &&
            !requireIdentity(packet.senderReport->ssrc)) {
            return ::media::Result<bool>::success(false);
        }
        if (packet.receiverReportSsrc &&
            !requireIdentity(*packet.receiverReportSsrc)) {
            return ::media::Result<bool>::success(false);
        }
        for (const MediaRtcpSdesChunk& chunk : packet.sdesChunks) {
            if (!requireIdentity(chunk.ssrc)) {
                return ::media::Result<bool>::success(false);
            }
        }
        for (const std::uint32_t ssrc : packet.byeSources) {
            if (!requireIdentity(ssrc)) {
                return ::media::Result<bool>::success(false);
            }
        }
    }
    return ::media::Result<bool>::success(referencesExpected);
}

} // namespace

::media::Result<MediaPreparedRawRtpProbe> MediaRawRtpInputPreparer::prepare(
    const MediaRawRtpProbePlan& plan)
{
    const auto& video = std::visit(
        [](const auto& streams) -> const MediaRawRtpPreparedStreamPlan& {
            return streams.video;
        },
        plan.streams);
    const auto* audioVideo = std::get_if<
        MediaRawRtpProbePlan::AudioVideo>(&plan.streams);
    const MediaRawRtpPreparedStreamPlan* audio = audioVideo
        ? &audioVideo->audio
        : nullptr;
    if (plan.openTimeoutMs <= 0 || plan.analyzeDurationUs <= 0 ||
        plan.maximumBufferedBytes == 0 ||
        plan.reorderWindowPackets == 0 || plan.maximumReorderDelayMs <= 0 ||
        !plan.packetizationPolicy || !plan.rtcpPolicy ||
        video.identity.streamKind != MediaStreamKind::Video ||
        video.identity.codecName.empty() ||
        video.identity.clockRate <= 0 ||
        plan.maximumBufferedBytes >
            static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP probe requires explicit positive time and byte limits"));
    }
    if (audio &&
        (audio->identity.streamKind != MediaStreamKind::Audio ||
         audio->identity.codecName.empty() ||
         audio->identity.clockRate <= 0)) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            ::media::ErrorInfo::invalidArgument(
                "raw RTP companion audio requires complete planned identity and byte capacity"));
    }
    auto observer = MediaRtpVideoSignalingObserver::create(
        video.identity.codecName, video.identity.payloadType,
        video.identity.clockRate,
        *plan.packetizationPolicy);
    if (!observer) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            observer.error());
    }
    auto frameRateObserver = MediaRtpVideoFrameRateObserver::create(
        video.identity.clockRate);
    if (!frameRateObserver) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            frameRateObserver.error());
    }
    auto byteBudget = MediaRawRtpPreparedByteBudget::create(
        plan.maximumBufferedBytes);
    if (!byteBudget) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            byteBudget.error());
    }
    auto replayClock =
        std::make_shared<MediaRawRtpPreparedReplayClock>();
    auto videoIngressObservation =
        std::make_shared<MediaRtpIngressObservationCollector>();
    std::optional<MediaRtpUdpTransport> audioTransport;
    std::optional<MediaPreparedRealtimeInput> preparedAudio;
    if (audio) {
        auto openedAudio = MediaRtpUdpTransport::open(audio->transport);
        if (!openedAudio) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                openedAudio.error());
        }
        audioTransport.emplace(std::move(openedAudio).value());
    }
    auto opened = MediaRtpUdpTransport::open(video.transport);
    if (!opened) {
        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
            opened.error());
    }
    MediaRtpUdpTransport transport = std::move(opened).value();
    if (audio) {
        auto createdAudio = MediaPreparedRealtimeInput::createRawRtp({
            std::move(*audioTransport), {}, audio->identity,
            std::nullopt, replayClock, byteBudget.value(),
            std::make_shared<MediaRtpIngressObservationCollector>(),
            audio->transport.cancellableReadTimeoutMs});
        if (!createdAudio) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                createdAudio.error());
        }
        preparedAudio.emplace(std::move(createdAudio).value());
        if (auto status = preparedAudio->startRawRtpPreflightCapture();
            !status) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                status.error());
        }
    }
    MediaRtpReorderBuffer reorder(MediaRtpReorderConfig{
        plan.reorderWindowPackets,
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::milliseconds(plan.maximumReorderDelayMs)),
        video.identity.payloadType});
    std::deque<MediaPreparedRawRtpDatagram> buffered;
    std::size_t bufferedBytes = 0;
    std::size_t packetCount = 0;
    bool bufferedRtcpValidated = false;

    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    std::optional<Clock::time_point> firstMatchingPacketAt;
    bool signalingComplete = false;
    const auto observeReordered = [&](MediaRtpReorderResult reordered)
        -> ::media::Status {
        for (const auto& discontinuity : reordered.discontinuities) {
            if (discontinuity.reason ==
                MediaRtpDiscontinuityReason::SequenceGap) {
                observer.value().discontinuity();
            }
        }
        for (const MediaRtpPacket& packet : reordered.packets) {
            auto observation = observer.value().observe(packet);
            if (!observation) {
                return ::media::Status::failure(observation.error());
            }
            if (observation.value().epochChanged) {
                buffered.clear();
                bufferedBytes = 0;
                packetCount = 0;
                bufferedRtcpValidated = false;
                firstMatchingPacketAt = Clock::now();
                signalingComplete = false;
                videoIngressObservation->reset();
            }
            signalingComplete = signalingComplete ||
                observation.value().complete;
            if (auto status = frameRateObserver.value().observe(packet);
                !status) {
                return status;
            }
        }
        return ::media::Status::success();
    };
    while (true) {
        if (preparedAudio) {
            if (auto status = preparedAudio->rawRtpCaptureStatus(); !status) {
                return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                    status.error());
            }
        }
        const auto now = Clock::now();
        const auto openElapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(now - startedAt);
        if (openElapsed.count() >= plan.openTimeoutMs) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                incompleteSignalingError(
                    "raw RTP video signaling probe reached open timeout before complete parameter sets",
                    observer.value().evidence(), packetCount, bufferedBytes));
        }
        if (firstMatchingPacketAt &&
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - *firstMatchingPacketAt).count() >= plan.analyzeDurationUs) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                incompleteSignalingError(
                    "raw RTP video signaling probe reached analysis limit before complete parameter sets",
                    observer.value().evidence(), packetCount, bufferedBytes));
        }

        int timeoutMs = std::min(
            video.transport.cancellableReadTimeoutMs,
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
        if (const auto reorderDeadline = reorder.nextDeadline()) {
            const auto remaining = std::chrono::duration_cast<
                std::chrono::milliseconds>(*reorderDeadline - now);
            timeoutMs = std::min(timeoutMs,
                remaining.count() > 0
                    ? static_cast<int>(remaining.count())
                    : 1);
        }
        auto received = transport.receive(timeoutMs);
        if (!received) {
            if (received.error().code == ::media::ErrorCode::WouldBlock) {
                auto expired = reorder.expire(Clock::now());
                if (!expired) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        expired.error());
                }
                if (auto status = observeReordered(
                        std::move(expired).value()); !status) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        status.error());
                }
                if (!signalingComplete) continue;
            } else {
                return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                    received.error());
            }
        } else {
            MediaRtpUdpDatagram datagram = std::move(received).value();
            if (auto status = byteBudget.value()->observe(
                    datagram.bytes.size()); !status) {
                return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                    status.error());
            }
            const auto observedAt = Clock::now();
            const std::int64_t observedAtNs =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    observedAt.time_since_epoch()).count();
            bool retainDatagram = true;
            std::optional<std::uint16_t> observedSequenceNumber;
            if (datagram.channel == MediaRtpUdpChannel::Rtp) {
                auto packet = MediaRtpPacketParser::parse(datagram.bytes);
                if (!packet) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        packet.error());
                }
                if (packet.value().payloadType !=
                    video.identity.payloadType) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        payloadTypeConflictError(
                            video.identity.payloadType,
                            packet.value().payloadType));
                }
                observedSequenceNumber = packet.value().sequenceNumber;
                if (!firstMatchingPacketAt) firstMatchingPacketAt = observedAt;
                auto reordered = reorder.push(
                    std::move(packet).value(), observedAt);
                if (!reordered) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        reordered.error());
                }
                if (auto status = observeReordered(
                        std::move(reordered).value()); !status) {
                    return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                        status.error());
                }
                if (!bufferedRtcpValidated &&
                    observer.value().evidence().ssrc) {
                    std::deque<MediaPreparedRawRtpDatagram> validated;
                    for (auto& queued : buffered) {
                        if (queued.datagram.channel !=
                                MediaRtpUdpChannel::Rtcp) {
                            validated.push_back(std::move(queued));
                            continue;
                        }
                        auto belongs = rtcpBelongsToSsrc(
                            queued.datagram.bytes, *plan.rtcpPolicy,
                            observer.value().evidence().ssrc);
                        if (!belongs) {
                            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                                belongs.error());
                        }
                        if (belongs.value()) {
                            validated.push_back(std::move(queued));
                        } else {
                            bufferedBytes -= queued.datagram.bytes.size();
                        }
                    }
                    buffered = std::move(validated);
                    bufferedRtcpValidated = true;
                }
            } else {
                if (observer.value().evidence().ssrc) {
                    auto belongs = rtcpBelongsToSsrc(
                        datagram.bytes, *plan.rtcpPolicy,
                        observer.value().evidence().ssrc);
                    if (!belongs) {
                        return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                            belongs.error());
                    }
                    retainDatagram = belongs.value();
                }
            }
            if (auto status = videoIngressObservation->observeDatagram(
                    datagram.bytes.size(), observedSequenceNumber,
                    observedAtNs); !status) {
                return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                    status.error());
            }
            if (retainDatagram) {
                bufferedBytes += datagram.bytes.size();
                if (datagram.channel == MediaRtpUdpChannel::Rtp) ++packetCount;
                buffered.push_back(MediaPreparedRawRtpDatagram{
                    std::move(datagram), observedAtNs});
            }
        }
        const auto sourceFrameRate = frameRateObserver.value().frameRate();
        if (!signalingComplete || !sourceFrameRate) continue;

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
            std::move(transport), std::move(buffered), video.identity,
            detected.value(), replayClock, byteBudget.value(),
            videoIngressObservation,
            video.transport.cancellableReadTimeoutMs});
        if (!prepared) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                prepared.error());
        }
        if (auto status = prepared.value().startRawRtpPreflightCapture();
            !status) {
            return ::media::Result<MediaPreparedRawRtpProbe>::failure(
                status.error());
        }
        if (preparedAudio) {
            return ::media::Result<MediaPreparedRawRtpProbe>::success(
                MediaPreparedRawRtpAudioVideoProbe{
                    std::move(signaling), *sourceFrameRate,
                    std::move(prepared).value(),
                    std::move(*preparedAudio)});
        }
        return ::media::Result<MediaPreparedRawRtpProbe>::success(
            MediaPreparedRawRtpVideoOnlyProbe{
                std::move(signaling), *sourceFrameRate,
                std::move(prepared).value()});
    }
}

} // namespace media::ffmpeg::graph
