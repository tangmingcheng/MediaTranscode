#include "internal/graph/nodes/demux/MediaTsDemuxNodePlanDecoder.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"

#include <type_traits>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::uint64_t> requiredGeneration(
    const MediaNodeOptions* options,
    const char* key)
{
    auto value = requiredNonNegativeIntNodeOption(
        options, "MpegTsDemuxNode", key);
    if (!value) {
        return ::media::Result<std::uint64_t>::failure(value.error());
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(value.value()));
}

::media::Result<MediaTsInitialPacketRetentionLimit> retentionLimit(
    const MediaNodeOptions* options,
    const char* packetCapacityKey,
    const char* byteCapacityKey,
    const char* maximumPacketBytesKey)
{
    auto packetCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", packetCapacityKey);
    if (!packetCapacity) {
        return ::media::Result<MediaTsInitialPacketRetentionLimit>::failure(
            packetCapacity.error());
    }
    auto byteCapacity = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", byteCapacityKey);
    if (!byteCapacity) {
        return ::media::Result<MediaTsInitialPacketRetentionLimit>::failure(
            byteCapacity.error());
    }
    auto maximumPacketBytes = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", maximumPacketBytesKey);
    if (!maximumPacketBytes) {
        return ::media::Result<MediaTsInitialPacketRetentionLimit>::failure(
            maximumPacketBytes.error());
    }
    return ::media::Result<MediaTsInitialPacketRetentionLimit>::success({
        static_cast<std::size_t>(packetCapacity.value()),
        static_cast<std::uint64_t>(byteCapacity.value()),
        static_cast<std::uint64_t>(maximumPacketBytes.value())});
}

} // namespace

::media::Result<MediaTsDemuxNodeRuntimePlan>
MediaTsDemuxNodePlanDecoder::decode(
    const MediaNodeOptions* options,
    const MediaTsRuntimeBinding& binding)
{
    auto gap = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.maximum_pcr_gap_27mhz");
    auto packetStride = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.packet_stride");
    auto evidenceCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.evidence_timeline_capacity");
    auto projectionCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.projection_capacity");
    auto regression = requiredPositiveInt64NodeOption(
        options, "MpegTsDemuxNode", "mpegts.maximum_position_regression_bytes");
    auto provenanceCapacity = requiredPositiveIntNodeOption(
        options, "MpegTsDemuxNode", "mpegts.pes_provenance_capacity");
    auto sourceGeneration = requiredGeneration(
        options, "mpegts.initial_source_generation");
    auto rawGeneration = requiredGeneration(
        options, "mpegts.initial_raw_generation");
    if (!gap || !packetStride || !evidenceCapacity || !projectionCapacity ||
        !regression || !provenanceCapacity || !sourceGeneration ||
        !rawGeneration) {
        if (!gap) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(gap.error());
        if (!packetStride) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(packetStride.error());
        if (!evidenceCapacity) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(evidenceCapacity.error());
        if (!projectionCapacity) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(projectionCapacity.error());
        if (!regression) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(regression.error());
        if (!provenanceCapacity) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(provenanceCapacity.error());
        if (!sourceGeneration) return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(sourceGeneration.error());
        return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(rawGeneration.error());
    }
    if (auto status = MediaTsRuntimeBindingCodec::validate(
            binding, static_cast<std::size_t>(provenanceCapacity.value()));
        !status) {
        return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(
            status.error());
    }
    auto videoRetention = retentionLimit(
        options,
        "mpegts.initial_acquiring_video_packet_capacity",
        "mpegts.initial_acquiring_video_byte_capacity",
        "mpegts.maximum_acquiring_video_packet_bytes");
    if (!videoRetention) {
        return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(
            videoRetention.error());
    }
    return std::visit(
        [&](const auto& selected)
            -> ::media::Result<MediaTsDemuxNodeRuntimePlan> {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding, MediaTsVideoOnlyRuntimeBinding>) {
                return ::media::Result<MediaTsDemuxNodeRuntimePlan>::success({
                    gap.value(),
                    static_cast<std::size_t>(packetStride.value()),
                    static_cast<std::size_t>(evidenceCapacity.value()),
                    static_cast<std::size_t>(projectionCapacity.value()),
                    static_cast<std::uint64_t>(regression.value()),
                    static_cast<std::size_t>(provenanceCapacity.value()),
                    sourceGeneration.value(), rawGeneration.value(),
                    MediaTsVideoOnlyProgramClockPolicy{
                        static_cast<std::uint16_t>(selected.programNumber),
                        static_cast<std::uint16_t>(selected.programMapPid),
                        selected.pcrPid, selected.video.pid, gap.value()},
                    MediaTsVideoOnlyPacketRetentionPlan{
                        videoRetention.value()}});
            } else {
                auto audioRetention = retentionLimit(
                    options,
                    "mpegts.initial_acquiring_audio_packet_capacity",
                    "mpegts.initial_acquiring_audio_byte_capacity",
                    "mpegts.maximum_acquiring_audio_packet_bytes");
                if (!audioRetention) {
                    return ::media::Result<MediaTsDemuxNodeRuntimePlan>::failure(
                        audioRetention.error());
                }
                return ::media::Result<MediaTsDemuxNodeRuntimePlan>::success({
                    gap.value(),
                    static_cast<std::size_t>(packetStride.value()),
                    static_cast<std::size_t>(evidenceCapacity.value()),
                    static_cast<std::size_t>(projectionCapacity.value()),
                    static_cast<std::uint64_t>(regression.value()),
                    static_cast<std::size_t>(provenanceCapacity.value()),
                    sourceGeneration.value(), rawGeneration.value(),
                    MediaTsAudioVideoProgramClockPolicy{
                        static_cast<std::uint16_t>(selected.programNumber),
                        static_cast<std::uint16_t>(selected.programMapPid),
                        selected.pcrPid, selected.video.pid,
                        selected.audio.pid, gap.value()},
                    MediaTsAudioVideoPacketRetentionPlan{
                        videoRetention.value(), audioRetention.value()}});
            }
        },
        binding);
}

} // namespace media::ffmpeg::graph
