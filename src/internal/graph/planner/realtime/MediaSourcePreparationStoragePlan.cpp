#include "internal/graph/planner/realtime/MediaSourcePreparationStoragePlan.h"

#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaAacRtpAuHeaderPlan.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/protocol/rtp/MediaRtpInputProtocolSession.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/planner/video/MediaVideoDecoderPreparationStoragePlan.h"

#include <algorithm>
#include <climits>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaRtpProtocolPreparationStoragePlan>
MediaSourcePreparationStoragePlanner::protocolWorkingSet(
    std::size_t maximumDatagramBytes,
    const MediaRtpReorderConfig& reorder,
    const MediaRtpDepacketizerConfig& depacketizer,
    const MediaPreparedRtpAccessUnitEnvelope& envelope)
{
    using Result = ::media::Result<MediaRtpProtocolPreparationStoragePlan>;
    if (auto valid = envelope.validate(); !valid) return Result::failure(valid.error());
    if (auto valid = MediaRtpDepacketizerFactory::validate(depacketizer); !valid)
        return Result::failure(valid.error());
    if (!maximumDatagramBytes || !reorder.windowPackets || reorder.maximumDelay.count() <= 0 ||
        reorder.payloadType != depacketizer.payloadType ||
        envelope.streamKind != depacketizer.streamKind || envelope.codecName != depacketizer.codecName ||
        envelope.maximumAccessUnitBytes > static_cast<std::uint64_t>(INT_MAX) ||
        (envelope.streamKind == MediaStreamKind::Video &&
            envelope.codecName != "h264" && envelope.codecName != "hevc") ||
        (envelope.streamKind == MediaStreamKind::Audio &&
            envelope.codecName != "aac" && envelope.codecName != "opus"))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Protocol preparation storage requires matching RTP and access-unit facts"));
    // All arithmetic is checked, including sums inside the retention product.
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::add(a, b, "source preparation storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    const auto multiply = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::multiply(a, b, "source preparation storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    // push temporarily inserts one packet beyond the window. Draining moves
    // payload ownership into the result; it does not make another payload copy.
    const auto packets = add(reorder.windowPackets, 1);
    const auto packetPayload = multiply(packets, maximumDatagramBytes);
    // Pending entries and drained vector descriptors can coexist during move.
    const auto packetHeaders = multiply(packets,
        2 * sizeof(MediaRtpPacket) + sizeof(std::chrono::steady_clock::time_point) +
        sizeof(std::uint16_t) + sizeof(MediaRtpDiscontinuity));
    const auto parser = add(packetPayload, packetHeaders);
    // enqueueReordered moves packet payloads but retains result descriptors
    // until return. PendingItem is a variant, not a MediaRtpPacket header.
    // At most N packets and N discontinuities can be produced by one push
    // (N=window+1 >= 2); the caller drains pending before another observation.
    const auto pending = multiply(multiply(2, packets),
        sizeof(MediaRtpInputProtocolSession::PendingItem));
    // preparePacket's byte request remains alive during commitPacket.
    // The caller-owned RAII reservation table has a separate owner budget.
    const auto requests = multiply(envelope.maximumAccessUnitsPerPush,
        sizeof(std::uint64_t));
    const auto au = envelope.maximumAccessUnitBytes;
    const auto emittedPayload = multiply(envelope.maximumAccessUnitsPerPush,
        add(au, AV_INPUT_BUFFER_PADDING_SIZE + sizeof(AVPacket)));
    const auto emitted = add(emittedPayload, multiply(envelope.maximumAccessUnitsPerPush, sizeof(MediaRtpAccessUnit)));
    std::uint64_t signaling = 0;
    std::uint64_t assembly = 0;
    std::uint64_t compressed = add(packetPayload, emittedPayload);
    if (envelope.streamKind == MediaStreamKind::Video) {
        // Each NAL occupies at least one wire byte. Signaling and depayloading
        // can coexist in continuous preparation, so neither replaces the other.
        const auto nalViews = multiply(maximumDatagramBytes, sizeof(MediaRtpNalUnit));
        const auto parameterSets = envelope.codecName == "h264" ? 2u : 3u;
        const auto parameterBytes = multiply(2 * parameterSets + 1, au);
        const auto assemblyPayload = multiply(2, au);
        signaling = add(parameterBytes, nalViews);
        compressed = add(compressed, add(parameterBytes, assemblyPayload));
        assembly = add(add(assemblyPayload, emitted), nalViews);
    } else if (envelope.codecName == "aac") {
        // RFC 3640 high-bitrate mode uses 16-bit AU headers. The parser may
        // allocate all headers before it rejects invalid AU payload sizes.
        // One fragmented AU or one per-AU copy coexists with emitted AVPackets.
        const auto headers = multiply(maximumDatagramBytes / 2,
            sizeof(MediaAacRtpAuHeader));
        assembly = add(add(au, emitted), headers);
        compressed = add(compressed, au);
    } else {
        // Opus has no cross-packet assembly; makeRtpAccessUnit copies its
        // by-value input while creating the independently owning AVPacket.
        assembly = add(au, emitted);
        compressed = add(compressed, au);
    }
    // parseRtpFmtp owns the map plus current token/key/value and trim input.
    // At most one nonempty parameter can originate from each text byte. These
    // are implementation coexistence counts, not empirical capacity factors.
    const auto configuration = add(
        add(multiply(4, add(depacketizer.fmtp.size(), 1)),
            multiply(2, add(depacketizer.codecName.size(), 1))),
        multiply(depacketizer.fmtp.size(), sizeof(MediaRtpFmtpParameters::value_type)));
    const auto maximum = add(add(add(parser, pending), signaling),
        add(add(assembly, requests), configuration));
    if (failure) return Result::failure(failure);
    if (compressed > maximum)
        return Result::failure(::media::ErrorInfo::invalidArgument("Protocol compressed classification exceeds total storage"));
    if (maximum > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Protocol preparation storage exceeds addressable memory"));
    return Result::success({static_cast<std::size_t>(maximum), maximumDatagramBytes,
        static_cast<std::size_t>(packets), static_cast<std::size_t>(au),
        static_cast<std::size_t>(envelope.maximumAccessUnitsPerPush),
        static_cast<std::size_t>(parser), static_cast<std::size_t>(pending),
        static_cast<std::size_t>(requests), static_cast<std::size_t>(signaling),
        static_cast<std::size_t>(assembly), static_cast<std::size_t>(configuration),
        "RTP-window+" + envelope.sizeAuthority + ";owning-payload-coexistence",
        {static_cast<std::size_t>(maximum - compressed), static_cast<std::size_t>(compressed),
         static_cast<std::size_t>(maximum)}});
}

::media::Result<MediaSourcePreparationStoragePlan>
MediaSourcePreparationStoragePlanner::plan(
    std::size_t maximumDatagramBytes,
    const MediaRtpReorderConfig& reorder,
    const MediaRtpDepacketizerConfig& depacketizer,
    const MediaPreparedRtpAccessUnitEnvelope& envelope,
    const MediaDecoderInputRetention& retention,
    const CodecResolverDecoderContextBuildResult& openedDecoder,
    const MediaHardwareDescriptor& frameContract, MediaSourcePreparationInput input)
{
    using Result = ::media::Result<MediaSourcePreparationStoragePlan>;
    if (auto valid = envelope.validate(); !valid) return Result::failure(valid.error());
    if ((input != MediaSourcePreparationInput::RawDatagrams &&
         input != MediaSourcePreparationInput::CompletedAccessUnits) ||
        !maximumDatagramBytes || !reorder.windowPackets ||
        reorder.maximumDelay.count() <= 0 || retention.authority.empty() ||
        retention.threadCount <= 0 || envelope.streamKind != MediaStreamKind::Video ||
        (envelope.codecName != "h264" && envelope.codecName != "hevc") ||
        envelope.maximumAccessUnitBytes > static_cast<std::uint64_t>(INT_MAX) ||
        !openedDecoder.context || openedDecoder.context->extradata_size < 0 ||
        frameContract.frameKind == MediaHardwareFrameKind::Unknown) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Source preparation storage requires explicit RTP video and decoder retention facts"));
    }
    // All arithmetic is checked, including sums inside the retention product.
    ::media::ErrorInfo failure;
    const auto add = [&](std::uint64_t a, std::uint64_t b) {
        auto value = MediaCheckedArithmetic::add(a, b, "source preparation storage");
        if (!value) { failure = value.error(); return std::uint64_t{0}; }
        return value.value();
    };
    auto protocol = protocolWorkingSet(maximumDatagramBytes, reorder, depacketizer, envelope);
    if (!protocol) return Result::failure(protocol.error());
    const auto& workspace = protocol.value();
    const auto packets = workspace.maximumReorderedPackets;
    const auto au = workspace.maximumAccessUnitBytes;
    // Completed AUs borrow the continuous session's protocol products. This
    // consumer creates protocol owners only when replaying raw datagrams.
    const bool raw = input == MediaSourcePreparationInput::RawDatagrams;
    const auto parser = raw ? workspace.parserReorderBytes : 0;
    const auto signaling = raw ? workspace.signalingBytes : 0;
    const auto assembly = raw ? workspace.assemblyBytes : 0;
    const auto configuration = workspace.configurationBytes;
    auto codecStorage = MediaVideoDecoderPreparationStoragePlanner::plan(
        au, retention, openedDecoder, frameContract);
    if (!codecStorage) return Result::failure(codecStorage.error());
    const auto decoder = codecStorage.value().packetBytes;
    const auto ownerBytes = codecStorage.value().ownerBytes;
    const auto softwareBytes = codecStorage.value().softwareFrameBytes;
    // Signaling validation and decode are separate scopes over the immutable
    // lease. Exactly one public AVFrame header is retained by this consumer.
    const auto maximum = add(add(parser, std::max(signaling, assembly)),
        add(add(decoder, sizeof(AVFrame)), add(add(ownerBytes, softwareBytes), configuration)));
    if (failure) return Result::failure(failure);
    if (maximum > std::numeric_limits<std::size_t>::max()) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Source preparation storage exceeds addressable memory"));
    }
    return Result::success({static_cast<std::size_t>(maximum), maximumDatagramBytes,
        static_cast<std::size_t>(packets), static_cast<std::size_t>(au),
        static_cast<std::size_t>(envelope.maximumAccessUnitsPerPush),
        static_cast<std::size_t>(parser), static_cast<std::size_t>(signaling),
        static_cast<std::size_t>(assembly), static_cast<std::size_t>(decoder),
        static_cast<std::size_t>(ownerBytes), static_cast<std::size_t>(softwareBytes),
        static_cast<std::size_t>(configuration),
        std::string(raw ? "RTP-window+wire-NAL-views+Annex-A-AU-envelope;sequential-signaling/decode;"
                        : "completed-AU-reference+configuration+decoder;continuous-protocol-owner-excluded;") +
            retention.authority});
}

} // namespace media::ffmpeg::graph
