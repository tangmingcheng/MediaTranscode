#include "internal/graph/planner/realtime/MediaCompositionProducerResidencePlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"

namespace media::ffmpeg::graph {
::media::Result<MediaCompositionProducerResidence> MediaCompositionProducerResidencePlanner::plan(
    const MediaCompositionProducerHolderFacts& facts, MediaNodeKind producer,
    MediaStreamKind stream, MediaPayloadKind payload,
    const MediaCompositionPacketResidenceCapacity& capacity)
{
    using Result = ::media::Result<MediaCompositionProducerResidence>;
    const auto* holders = stream == MediaStreamKind::Video ? &facts.video
        : stream == MediaStreamKind::Audio && facts.audio ? &*facts.audio : nullptr;
    if (!holders || holders->decoder.authority.empty() || !holders->maximumAtomicInputCompletion)
        return Result::failure(::media::ErrorInfo::invalidArgument("Source residence requires authoritative stream holders"));
    // These are actual FFmpegNodeRuntime members: pending output and receive reservation.
    std::uint64_t minimumProgress = 2;
    std::uint64_t additional = 0;
    if (payload == MediaPayloadKind::Frame) {
        if (stream == MediaStreamKind::Video && producer != MediaNodeKind::VideoFilter &&
            facts.filterRetainedInputFrames) {
            auto retained = MediaCheckedArithmetic::add(minimumProgress, *facts.filterRetainedInputFrames,
                "source filter retained input and decoder receive working set");
            if (!retained) return Result::failure(retained.error());
            minimumProgress = retained.value();
        }
    } else if (payload == MediaPayloadKind::Packet) {
        auto internal = MediaCheckedArithmetic::add(holders->decoder.mainHandoffPackets,
            holders->decoder.frameWorkerPackets, "source decoder handoff and frame workers");
        if (!internal) return Result::failure(internal.error());
        internal = MediaCheckedArithmetic::add(internal.value(), holders->decoder.serialPrivatePackets,
            "source decoder internal packet owners");
        if (!internal) return Result::failure(internal.error());
        additional = internal.value();
        auto retained = MediaCheckedArithmetic::add(capacity.startupPackets, capacity.acquiringPackets,
            "prepared source acquisition and startup packet owners");
        if (!retained) return Result::failure(retained.error());
        retained = MediaCheckedArithmetic::add(retained.value(), additional,
            "source prepared and decoder packet owners");
        if (!retained) return Result::failure(retained.error());
        // VideoDecodeNode/AudioDecodeNode pendingPacket may coexist with codec inputs.
        retained = MediaCheckedArithmetic::add(retained.value(), 1, "source decoder pending packet");
        if (!retained) return Result::failure(retained.error());
        minimumProgress = retained.value();
        if (producer == MediaNodeKind::RawRtpInput) {
            retained = MediaCheckedArithmetic::add(minimumProgress, holders->maximumAtomicInputCompletion,
                "prepared source atomic input completion");
            if (!retained) return Result::failure(retained.error());
            minimumProgress = retained.value();
            retained = MediaCheckedArithmetic::add(additional, holders->maximumAtomicInputCompletion,
                "source codec-owned inputs and atomic completion");
            if (!retained) return Result::failure(retained.error());
            additional = retained.value();
        }
    } else return Result::failure(::media::ErrorInfo::invalidArgument("Source residence requires packet or frame payload"));
    return Result::success({additional, minimumProgress});
}
} // namespace media::ffmpeg::graph
