#include "internal/graph/runtime/validation/MediaSourceClockShapeValidator.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status validateRtpInputLiveness(
    const MediaAvSyncGraphShape& shape,
    const MediaAvSyncRuntimeBinding& binding)
{
    if (!binding.plan.rtpInput ||
        !binding.plan.rtpInput->input.maximumExtrapolationNs) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RTP source-clock shape requires maximum extrapolation"));
    }
    const auto inputs = shape.nodes(MediaNodeKind::RawRtpInput);
    if (inputs.size() != 2) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP source-clock shape requires two raw RTP inputs"));
    }
    for (const MediaNode* input : inputs) {
        auto maximumExtrapolation = requiredPositiveInt64NodeOption(
            &input->options, "RawRtpInputNode",
            "rtcp.maximum_extrapolation_ns");
        if (!maximumExtrapolation ||
            maximumExtrapolation.value() !=
                binding.plan.rtpInput->input.maximumExtrapolationNs
                    ->nanoseconds()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Raw RTP input maximum extrapolation differs from planner product"));
        }
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaSourceClockShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    if (!binding.plan.sourceClockMode) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Source-clock shape requires its planner mode"));
    }
    const MediaAvSyncGraphShape shape(graph);
    switch (*binding.plan.sourceClockMode) {
    case MediaAvSyncSourceClockMode::RtpSenderReports: {
        auto cardinality = shape.requireExact({
            {MediaNodeKind::RtpClockGroup, 1, "RTP clock group"},
            {MediaNodeKind::RtpClockSnapshotFanout, 1,
             "RTP clock snapshot fanout"},
            {MediaNodeKind::RtpSourceClockStateAdapter, 1,
             "RTP source-clock adapter"},
            {MediaNodeKind::RtpPacketClockBinder, 2,
             "RTP packet clock binder"},
            {MediaNodeKind::MpegTsDemux, 0, "MPEG-TS demux"},
            {MediaNodeKind::DemuxPacketClockBinder, 0,
             "demux packet clock binder"}},
            "RTP source-clock shape");
        if (!cardinality) return cardinality;
        return validateRtpInputLiveness(shape, binding);
    }
    case MediaAvSyncSourceClockMode::MpegTsPcr:
        return shape.requireExact({
            {MediaNodeKind::RtpClockGroup, 0, "RTP clock group"},
            {MediaNodeKind::RtpClockSnapshotFanout, 0,
             "RTP clock snapshot fanout"},
            {MediaNodeKind::RtpSourceClockStateAdapter, 0,
             "RTP source-clock adapter"},
            {MediaNodeKind::RtpPacketClockBinder, 0,
             "RTP packet clock binder"},
            {MediaNodeKind::MpegTsDemux, 1, "MPEG-TS demux"},
            {MediaNodeKind::DemuxPacketClockBinder, 0,
             "demux packet clock binder"}},
            "MPEG-TS source-clock shape");
    case MediaAvSyncSourceClockMode::DemuxTimestamps: {
        auto cardinality = shape.requireExact({
            {MediaNodeKind::RtpClockGroup, 0, "RTP clock group"},
            {MediaNodeKind::RtpClockSnapshotFanout, 0,
             "RTP clock snapshot fanout"},
            {MediaNodeKind::RtpSourceClockStateAdapter, 0,
             "RTP source-clock adapter"},
            {MediaNodeKind::RtpPacketClockBinder, 0,
             "RTP packet clock binder"},
            {MediaNodeKind::MpegTsDemux, 0, "MPEG-TS demux"},
            {MediaNodeKind::DemuxPacketClockBinder, 2,
             "demux packet clock binder"}},
            "demux timestamp source-clock shape");
        if (!cardinality) return cardinality;
        std::size_t video = 0;
        std::size_t audio = 0;
        for (const MediaNode* node :
             shape.nodes(MediaNodeKind::DemuxPacketClockBinder)) {
            auto decoded =
                MediaDemuxPacketClockBinderNodePlanCodec::decode(*node);
            if (!decoded) {
                return ::media::Status::failure(decoded.error());
            }
            auto exact =
                MediaDemuxPacketClockBinderNodePlanCodec::
                    validateAgainstPlanner(
                        decoded.value(),
                        binding.groupKey,
                        binding.plan);
            if (!exact) return exact;
            if (decoded.value().stream ==
                    MediaScheduledStream::Video) {
                ++video;
            } else {
                ++audio;
            }
        }
        if (video != 1 || audio != 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Demux timestamp source-clock shape requires one binder per A/V stream"));
        }
        return ::media::Status::success();
    }
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "Source-clock shape has an unsupported planner mode"));
}

} // namespace media::ffmpeg::graph
