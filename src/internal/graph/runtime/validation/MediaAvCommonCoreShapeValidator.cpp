#include "internal/graph/runtime/validation/MediaAvCommonCoreShapeValidator.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/sync/MediaAvSyncSourceClockModeNodeOptionCodec.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

#include <array>
#include <string_view>

namespace media::ffmpeg::graph {
namespace {

struct GroupOptionContract final {
    MediaNodeKind kind;
    std::string_view key;
};

constexpr std::array GroupContracts{
    GroupOptionContract{
        MediaNodeKind::AvOutputScheduler,
        "av_scheduler.sync_group"},
    GroupOptionContract{
        MediaNodeKind::PlaybackEpochBinder,
        "playback_epoch_binder.sync_group"},
    GroupOptionContract{
        MediaNodeKind::AvStartupClock,
        "av_startup_clock.sync_group"},
    GroupOptionContract{
        MediaNodeKind::ActivatedStartupReleaseSequencer,
        "activated_startup_release_sequencer.sync_group"},
    GroupOptionContract{
        MediaNodeKind::AvBoundReleaseExtractor,
        "av_bound_release_extractor.sync_group"},
    GroupOptionContract{
        MediaNodeKind::RtpPacketClockBinder,
        "rtp_clock_binder.sync_group"},
    GroupOptionContract{
        MediaNodeKind::DemuxPacketClockBinder,
        "demux_clock_binder.sync_group"},
    GroupOptionContract{
        MediaNodeKind::LockedPacketGate,
        "locked_packet_gate.sync_group"},
    GroupOptionContract{
        MediaNodeKind::CanonicalInput,
        "canonical_input.sync_group"},
    GroupOptionContract{
        MediaNodeKind::AvStartupCoordinator,
        "av_startup.sync_group"},
    GroupOptionContract{
        MediaNodeKind::AudioDriftController,
        "audio_drift_controller.sync_group"}};

const GroupOptionContract* findContract(
    MediaNodeKind kind,
    std::string_view key) noexcept
{
    for (const auto& contract : GroupContracts) {
        if (contract.kind == kind && contract.key == key) {
            return &contract;
        }
    }
    return nullptr;
}

::media::Status validateGroupReferences(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    constexpr std::string_view Suffix = ".sync_group";
    for (const auto& contract : GroupContracts) {
        for (const MediaNode& node : graph.nodes()) {
            if (node.kind != contract.kind) continue;
            auto group = requiredNodeOption(
                &node.options,
                "MediaAvCommonCoreShapeValidator",
                contract.key.data());
            if (!group) {
                return ::media::Status::failure(group.error());
            }
            if (group.value() != binding.groupKey.value()) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Synchronized node group conflicts with its runtime binding"));
            }
        }
    }
    for (const MediaNode& node : graph.nodes()) {
        for (const auto& [key, value] : node.options.values()) {
            if (!key.ends_with(Suffix)) continue;
            if (!findContract(node.kind, key) || value.empty()) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Synchronized graph contains an unsupported group consumer"));
            }
        }
    }
    return ::media::Status::success();
}

::media::Status validateStartupSourceMode(
    const MediaAvSyncGraphShape& shape,
    const MediaAvSyncRuntimeBinding& binding)
{
    if (!binding.plan.sourceClockMode) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V common core requires its planner source-clock mode"));
    }
    const auto nodes =
        shape.nodes(MediaNodeKind::AvStartupCoordinator);
    if (nodes.size() != 1) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V common core requires one startup coordinator"));
    }
    auto encoded = requiredNodeOption(
        &nodes.front()->options,
        "MediaAvStartupCoordinatorNode",
        "av_startup.source_clock_mode");
    if (!encoded) {
        return ::media::Status::failure(encoded.error());
    }
    auto decoded =
        MediaAvSyncSourceClockModeNodeOptionCodec::decode(
            encoded.value());
    if (!decoded) {
        return ::media::Status::failure(decoded.error());
    }
    if (decoded.value() != *binding.plan.sourceClockMode) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Startup coordinator source clock conflicts with its planner binding"));
    }
    return ::media::Status::success();
}

::media::Status validateReleasedAudioBranch(
    const MediaAvSyncGraphShape& shape,
    const MediaAvSyncRuntimeBinding& binding)
{
    const auto nodes = shape.nodes(MediaNodeKind::AvBoundReleaseExtractor);
    if (nodes.size() != 1) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V common core requires one bound release extractor"));
    }
    auto encoded = requiredNodeOption(
        &nodes.front()->options,
        "MediaAvBoundReleaseExtractorNode",
        "av_bound_release_extractor.audio_branch_mode");
    if (!encoded) return ::media::Status::failure(encoded.error());
    MediaBranchMode mode = MediaBranchMode::Drop;
    if (!parseMediaBranchMode(encoded.value(), mode)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V release extractor audio branch mode is invalid"));
    }
    const auto expected =
        binding.audioExecutionProduct ==
                MediaSynchronizedAudioExecutionProduct::PacketCopy
            ? MediaBranchMode::CopyPacket
            : MediaBranchMode::TranscodeFrame;
    if (mode != expected) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V release extractor audio branch conflicts with its runtime binding"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaAvCommonCoreShapeValidator::validate(
    const MediaGraph& graph,
    const MediaAvSyncRuntimeBinding& binding)
{
    const MediaAvSyncGraphShape shape(graph);
    const auto encoders = shape.nodes(MediaNodeKind::VideoEncode);
    if (encoders.size() != 1) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "A/V common core requires exactly one video encoder"));
    }
    auto filterActive = requiredBoolNodeOption(
        &encoders.front()->options,
        "VideoEncodeNode",
        "pipeline.filter_active");
    if (!filterActive) {
        return ::media::Status::failure(filterActive.error());
    }
    const bool audioTranscode = binding.audioExecutionProduct ==
        MediaSynchronizedAudioExecutionProduct::FrameTranscode;
    auto cardinality = shape.requireExact({
        {MediaNodeKind::SourceClockStateFanout, 1,
         "source-clock state fanout"},
        {MediaNodeKind::LockedPacketGate, 2, "locked packet gate"},
        {MediaNodeKind::CanonicalInput, 2, "canonical input"},
        {MediaNodeKind::AvStartupCoordinator, 1, "startup coordinator"},
        {MediaNodeKind::AvStartupClock, 1, "startup clock"},
        {MediaNodeKind::PlaybackEpochBinder, 1, "playback epoch binder"},
        {MediaNodeKind::ActivatedStartupReleaseSequencer, 1,
         "activated startup release sequencer"},
        {MediaNodeKind::AvBoundReleaseExtractor, 1,
         "bound release extractor"},
        {MediaNodeKind::AudioDriftController, audioTranscode ? 1u : 0u,
         "audio drift controller"},
        {MediaNodeKind::AvOutputScheduler, 1, "A/V output scheduler"},
        {MediaNodeKind::ScheduledOutputRouter, 1,
         "scheduled output router"},
        {MediaNodeKind::CodecResolver, 1, "video codec resolver"},
        {MediaNodeKind::VideoDecode, 1, "video decoder"},
        {MediaNodeKind::HardwareTransfer, 1, "hardware transfer"},
        {MediaNodeKind::VideoFrameRate, 1, "video frame-rate controller"},
        {MediaNodeKind::VideoFilter, filterActive.value() ? 1u : 0u,
         "planner-selected video filter"},
        {MediaNodeKind::VideoEncode, 1, "video encoder"},
        {MediaNodeKind::PacketSourceConfig, audioTranscode ? 0u : 1u,
         "audio packet-copy source config"},
        {MediaNodeKind::AudioCodecResolver, audioTranscode ? 1u : 0u,
         "audio codec resolver"},
        {MediaNodeKind::AudioDecode, audioTranscode ? 1u : 0u, "audio decoder"},
        {MediaNodeKind::AudioStartupTrim, audioTranscode ? 1u : 0u, "audio startup trim"},
        {MediaNodeKind::AudioResample, audioTranscode ? 1u : 0u, "audio resampler"},
        {MediaNodeKind::AudioEncode, audioTranscode ? 1u : 0u, "audio encoder"},
        {MediaNodeKind::EncodedAudioCanonicalizer, audioTranscode ? 1u : 0u,
         "encoded audio canonicalizer"}},
        "A/V common core shape");
    if (!cardinality) return cardinality;
    if (auto group = validateGroupReferences(graph, binding);
        !group) {
        return group;
    }
    if (auto sourceMode = validateStartupSourceMode(shape, binding);
        !sourceMode) {
        return sourceMode;
    }
    return validateReleasedAudioBranch(shape, binding);
}

} // namespace media::ffmpeg::graph
