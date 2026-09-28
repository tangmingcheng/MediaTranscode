#pragma once

#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/planner/realtime/MediaSourcePreparationStoragePlan.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/buffer/MediaRawRtpProbeLease.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"

#include <stop_token>

namespace media::ffmpeg::graph {

struct MediaRtpSourceFirstFrameProbePlan final {
    const MediaDetectedRtpVideoSignaling& signaling;
    const AVCodecParameters& codecParameters;
    const MediaVideoSourcePlan& source;
    MediaRtpVideoPacketizationPolicy packetization;
    MediaRtpDepacketizerConfig depacketizer;
    MediaRtpReorderConfig reorder;
    MediaPreparedRtpAccessUnitEnvelope envelope;
    std::size_t maximumDatagramBytes;
    MediaRational timeBase;
    MediaRational sampleAspectRatio;
    std::chrono::steady_clock::time_point deadline;
    std::stop_token cancellation;
};

enum class MediaRtpSourceFirstFrameDisposition {
    FirstFrame,
    NeedMoreEvidence
};

// Neither disposition publishes a source epoch or produces canonical lineage.
// Decoder packets deliberately have no opaque token. Before any replay of an
// already submitted prefix, the caller must release firstFrame and flush this
// same context through MediaVideoDecoderCodecApi, then use original datagrams.
// Negotiation/copy proof remains the caller's common capability-probe step.
struct MediaRtpSourceFirstFrameEvidence final {
    MediaRtpSourceFirstFrameEvidence(MediaPreparationStorageLease reservation,
        CodecResolverDecoderContextBuildResult owner, MediaHardwareDescriptor contract)
        : storage(std::move(reservation)), decoder(std::move(owner)),
          disposition(MediaRtpSourceFirstFrameDisposition::NeedMoreEvidence),
          frameContract(std::move(contract)) {}
    MediaRtpSourceFirstFrameEvidence(MediaRtpSourceFirstFrameEvidence&&) noexcept = default;
    MediaRtpSourceFirstFrameEvidence& operator=(MediaRtpSourceFirstFrameEvidence&&) = delete;
    MediaRtpSourceFirstFrameEvidence(const MediaRtpSourceFirstFrameEvidence&) = delete;
    MediaRtpSourceFirstFrameEvidence& operator=(const MediaRtpSourceFirstFrameEvidence&) = delete;

    MediaPreparationStorageLease storage;
    CodecResolverDecoderContextBuildResult decoder;
    ::media::ffmpeg::FramePtr firstFrame;
    MediaRtpSourceFirstFrameDisposition disposition;
    MediaHardwareDescriptor frameContract;
    std::size_t inspectedDatagrams = 0;
    std::size_t submittedAccessUnits = 0;
    bool decoderMayHavePendingOutput = false;
    std::optional<std::uint32_t> firstRandomAccessTimestamp;
    std::string insufficiency;
};

class MediaRtpSourceFirstFrameProbe final {
public:
    static ::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>> prepareReplay(
        MediaRtpSourceFirstFrameEvidence evidence,
        const FFmpegInputStreamSnapshot& source);

    // decoder is the unused common-builder result (or the same owner after
    // an explicit caller-controlled flush), never a live production decoder.
    static ::media::Result<MediaRtpSourceFirstFrameEvidence> probe(
        const MediaRawRtpProbeLease& lease,
        const MediaRtpSourceFirstFrameProbePlan& plan,
        CodecResolverDecoderContextBuildResult decoder,
        const std::shared_ptr<MediaPreparationStorageBudget>& budget);
};

} // namespace media::ffmpeg::graph
