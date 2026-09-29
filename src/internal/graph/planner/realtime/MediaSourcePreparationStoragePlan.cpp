#include "internal/graph/planner/realtime/MediaSourcePreparationStoragePlan.h"

#include "internal/graph/protocol/rtp/MediaRtpNalUnitParser.h"
#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "internal/graph/utils/MediaVideoSurfaceFootprint.h"

#include <algorithm>
#include <climits>
#include <limits>

namespace media::ffmpeg::graph {

::media::Result<MediaSourcePreparationStoragePlan>
MediaSourcePreparationStoragePlanner::plan(
    std::size_t maximumDatagramBytes,
    const MediaRtpReorderConfig& reorder,
    const MediaRtpDepacketizerConfig& depacketizer,
    const MediaPreparedRtpAccessUnitEnvelope& envelope,
    const MediaDecoderInputRetention& retention,
    const CodecResolverDecoderContextBuildResult& openedDecoder,
    const MediaHardwareDescriptor& frameContract)
{
    using Result = ::media::Result<MediaSourcePreparationStoragePlan>;
    if (auto valid = envelope.validate(); !valid) return Result::failure(valid.error());
    if (!maximumDatagramBytes || !reorder.windowPackets ||
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
    const auto au = envelope.maximumAccessUnitBytes;
    // Each NAL consumes at least one wire byte. This bounds borrowed NAL views
    // without inventing a packetization-specific aggregation count.
    const auto nalViews = multiply(maximumDatagramBytes, sizeof(MediaRtpNalUnit));
    const auto parameterSets = envelope.codecName == "h264" ? 2u : 3u;
    // Fragment + retained sets + detected()'s copy of the retained sets.
    const auto signaling = add(multiply(2 * parameterSets + 1, au), nalViews);
    const auto emitted = multiply(envelope.maximumAccessUnitsPerPush,
        add(au, AV_INPUT_BUFFER_PADDING_SIZE + sizeof(AVPacket) + sizeof(MediaRtpAccessUnit)));
    // Assembler and fragment can coexist; pending AU aliases emitted ownership.
    const auto assembly = add(add(multiply(2, au), emitted), nalViews);
    const auto retainedPackets = add(add(retention.mainHandoffPackets,
        retention.frameWorkerPackets), retention.serialPrivatePackets);
    const auto decoder = multiply(retainedPackets,
        add(au, AV_INPUT_BUFFER_PADDING_SIZE + sizeof(AVPacket)));
    // parseRtpFmtp owns the map plus current token/key/value and trim input.
    // At most one nonempty parameter can originate from each text byte. These
    // are implementation coexistence counts, not empirical capacity factors.
    const auto configuration = add(
        add(multiply(4, add(depacketizer.fmtp.size(), 1)),
            multiply(2, add(depacketizer.codecName.size(), 1))),
        multiply(depacketizer.fmtp.size(), sizeof(MediaRtpFmtpParameters::value_type)));
    // Common builder's callback state contains exactly one AVPixelFormat;
    // control-block/allocator overhead is outside this logical byte scope.
    auto ownerBytes = add(sizeof(AVCodecContext),
        add(openedDecoder.context->extradata_size, AV_INPUT_BUFFER_PADDING_SIZE));
    // Handoff retains an authoritative source-parameter copy until its single
    // production claimant has verified the immutable input snapshot.
    ownerBytes = add(ownerBytes, add(sizeof(AVCodecParameters),
        add(openedDecoder.context->extradata_size, AV_INPUT_BUFFER_PADDING_SIZE)));
    if (openedDecoder.context->nb_coded_side_data < 0 ||
        (openedDecoder.context->nb_coded_side_data && !openedDecoder.context->coded_side_data))
        return Result::failure(::media::ErrorInfo::invalidArgument("Decoder coded side data is invalid"));
    for (int i = 0; i < openedDecoder.context->nb_coded_side_data; ++i) {
        const auto& side = openedDecoder.context->coded_side_data[i];
        if (side.size && !side.data)
            return Result::failure(::media::ErrorInfo::invalidArgument("Decoder coded side data has no storage"));
        // The decoder and retained source parameters each own this payload.
        ownerBytes = add(ownerBytes, multiply(2, add(sizeof(AVPacketSideData),
            add(side.size, AV_INPUT_BUFFER_PADDING_SIZE))));
    }
    if (openedDecoder.context.get_deleter().callbackOwner)
        ownerBytes = add(ownerBytes, sizeof(AVPixelFormat));
    if (openedDecoder.hardwareDevice) ownerBytes = add(ownerBytes, sizeof(AVBufferRef));
    if (openedDecoder.context->hw_device_ctx) ownerBytes = add(ownerBytes, sizeof(AVBufferRef));
    std::uint64_t softwareBytes = 0;
    if (frameContract.frameKind == MediaHardwareFrameKind::Software) {
        auto footprint = MediaVideoSurfaceFootprint::logicalBytes(
            frameContract.size.width, frameContract.size.height, frameContract.pixelFormat);
        if (!footprint) return Result::failure(footprint.error());
        softwareBytes = footprint.value();
    }
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
        "RTP-window+wire-NAL-views+Annex-A-AU-envelope;sequential-signaling/decode;" +
            retention.authority});
}

} // namespace media::ffmpeg::graph
