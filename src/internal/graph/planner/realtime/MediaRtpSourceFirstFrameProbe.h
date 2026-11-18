#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include "internal/graph/builder/codec/CodecResolverDecoderContextBuilder.h"
#include "internal/graph/planner/realtime/MediaSourcePreparationStoragePlan.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpVideoSignalingFacts.h"
#include "internal/graph/runtime/buffer/MediaRawRtpProbeLease.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"
#include "internal/graph/planner/video/MediaVideoDecoderPreparationStoragePlan.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"

#include "internal/graph/planner/realtime/MediaRtpSourceIdentityProbe.h"

#include <stop_token>
#include <span>

namespace media::ffmpeg::graph {

struct MediaRtpSourceFirstFrameProbePlan final {
    MediaRtpSourceProbeGoal goal;
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
    MediaSourcePreparationInput input;
};

enum class MediaRtpSourceFirstFrameDisposition {
    FirstFrame,
    NeedMoreEvidence
};

// Member order keeps the reservation alive until after codec destruction.
// Moving this owner across retries never releases and reacquires its budget.
struct MediaRtpSourceDecoderProbeOwner final {
    MediaRtpSourceDecoderProbeOwner(MediaPreparationStorageLease reservation,
        CodecResolverDecoderContextBuildResult codec)
        : storage(std::move(reservation)), decoder(std::move(codec)) {}
    MediaRtpSourceDecoderProbeOwner(MediaRtpSourceDecoderProbeOwner&&) noexcept = default;
    MediaRtpSourceDecoderProbeOwner& operator=(MediaRtpSourceDecoderProbeOwner&&) = delete;
    MediaPreparationStorageLease storage;
    CodecResolverDecoderContextBuildResult decoder;
};

// Neither disposition publishes a source epoch or produces canonical lineage.
// Identity probes attach tokens only to cloned packet headers. Before replay of an
// already submitted prefix, discard the old codec through restartDecoder.
// A fresh common-builder context owns no pending frames from the probe.
// Negotiation/copy proof remains the caller's common capability-probe step.
struct MediaRtpSourceFirstFrameEvidence final {
    MediaRtpSourceFirstFrameEvidence(MediaPreparationStorageLease reservation,
        CodecResolverDecoderContextBuildResult owner, MediaHardwareDescriptor contract,
        const MediaSourcePreparationStoragePlan& admitted)
        : storage(std::move(reservation)), decoder(std::move(owner)),
          disposition(MediaRtpSourceFirstFrameDisposition::NeedMoreEvidence),
          frameContract(std::move(contract)), maximumAccessUnitBytes(admitted.maximumAccessUnitBytes),
          decoderStorage{admitted.decoderPacketBytes, admitted.sourceOwnerBytes, admitted.softwareFrameBytes} {}
    MediaRtpSourceFirstFrameEvidence(MediaRtpSourceFirstFrameEvidence&&) noexcept = default;
    MediaRtpSourceFirstFrameEvidence& operator=(MediaRtpSourceFirstFrameEvidence&&) = delete;
    MediaRtpSourceFirstFrameEvidence(const MediaRtpSourceFirstFrameEvidence&) = delete;
    MediaRtpSourceFirstFrameEvidence& operator=(const MediaRtpSourceFirstFrameEvidence&) = delete;

    MediaPreparationStorageLease storage;
    std::shared_ptr<MediaRtpSourceIdentityProbe> identityProbe;
    CodecResolverDecoderContextBuildResult decoder;
    ::media::ffmpeg::FramePtr firstFrame;
    MediaRtpSourceFirstFrameDisposition disposition;
    MediaHardwareDescriptor frameContract;
    std::size_t maximumAccessUnitBytes;
    MediaVideoDecoderPreparationStoragePlan decoderStorage;
    std::size_t inspectedDatagrams = 0;
    std::size_t submittedAccessUnits = 0;
    bool decoderMayHavePendingOutput = false;
    std::optional<std::uint32_t> firstRandomAccessTimestamp;
    std::string insufficiency;
};

class MediaRtpSourceFirstFrameProbe final {
public:
    // The caller must first release all external probe frame references.
    // The existing lease remains held across codec destruction and reopening.
    static ::media::Status restartDecoder(MediaRtpSourceFirstFrameEvidence& evidence,
        const FFmpegInputStreamSnapshot& source, const MediaVideoSourcePlan& plan,
        const MediaPreparationControl& control);
    static ::media::Result<std::shared_ptr<MediaPreparedVideoDecoder>> prepareReplay(
        MediaRtpSourceFirstFrameEvidence evidence,
        const FFmpegInputStreamSnapshot& source, const MediaVideoSourcePlan& plan,
        const MediaPreparationControl& control);

    // decoder is an unused common-builder result, never a live or previously
    // submitted production decoder.
    static ::media::Result<MediaRtpSourceFirstFrameEvidence> probe(
        const MediaRawRtpProbeLease& lease,
        const MediaRtpSourceFirstFrameProbePlan& plan,
        MediaRtpSourceDecoderProbeOwner owner);

    // References completed AUs admitted by the continuous protocol owner;
    // decoding does not consume or modify the runtime prefix.
    static ::media::Result<MediaRtpSourceFirstFrameEvidence> probe(
        std::span<const MediaBufferRef> packets,
        const MediaRtpSourceFirstFrameProbePlan& plan,
        MediaRtpSourceDecoderProbeOwner owner);
};

} // namespace media::ffmpeg::graph
