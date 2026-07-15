#include "internal/graph/nodes/sync/MediaAvStartupCoordinatorNodePreparation.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/sync/startup/MediaAvStartupGenerationState.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::size_t> requiredCapacity(const MediaNodeOptions* options,
                                               const char* key)
{
    auto value = requiredPositiveIntNodeOption(options,
                                               "MediaAvStartupCoordinatorNode",
                                               key);
    if (!value) return ::media::Result<std::size_t>::failure(value.error());
    return ::media::Result<std::size_t>::success(
        static_cast<std::size_t>(value.value()));
}

bool validEventPort(const MediaPort* port, MediaPortDirection direction) noexcept
{
    return port && port->direction == direction &&
           port->streamKind == MediaStreamKind::Metadata &&
           port->edgeKind == MediaEdgeKind::Event &&
           port->payloadKind == MediaPayloadKind::GraphEvent;
}

::media::Status validatePorts(const MediaNode& node)
{
    if (!validEventPort(node.findInputPort("video"), MediaPortDirection::Input) ||
        !validEventPort(node.findInputPort("audio"), MediaPortDirection::Input) ||
        !validEventPort(node.findInputPort("clock"), MediaPortDirection::Input) ||
        !validEventPort(node.findOutputPort("release"), MediaPortDirection::Output)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MediaAvStartupCoordinatorNode requires complete event ports"));
    }
    return ::media::Status::success();
}

} // namespace

MediaAvStartupCoordinatorNodePreparation::MediaAvStartupCoordinatorNodePreparation(
    std::unique_ptr<MediaAvStartupCoordinator> coordinator,
    std::shared_ptr<MediaAvStartupGenerationState> generationState)
    : m_coordinator(std::move(coordinator))
    , m_generationState(std::move(generationState))
{
}

::media::Result<MediaAvStartupCoordinatorNodePreparation>
prepareMediaAvStartupCoordinatorNode(const MediaNode& node)
{
    if (node.kind != MediaNodeKind::AvStartupCoordinator) {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup node preparation requires coordinator kind"));
    }
    if (auto ports = validatePorts(node); !ports) {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            ports.error());
    }
    const auto* options = &node.options;
    auto requireKey = requiredBoolNodeOption(options, "MediaAvStartupCoordinatorNode",
                                             "av_startup.require_video_key_frame");
    auto trimAudio = requiredBoolNodeOption(options, "MediaAvStartupCoordinatorNode",
                                            "av_startup.trim_audio_to_common_start");
    auto allowDegraded = requiredBoolNodeOption(options, "MediaAvStartupCoordinatorNode",
                                                "av_startup.allow_degraded_clock");
    auto wait = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                "av_startup.maximum_wait_ns");
    auto preroll = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                   "av_startup.preroll_ns");
    auto keyWait = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                   "av_startup.key_frame_wait_ns");
    auto trim = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                "av_startup.maximum_audio_trim_ns");
    auto skew = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                "av_startup.maximum_initial_skew_ns");
    auto gap = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                               "av_startup.maximum_gap_ns");
    auto lead = requiredPositiveInt64NodeOption(options, "MediaAvStartupCoordinatorNode",
                                                "av_startup.output_lead_ns");
    auto videoCapacity = requiredCapacity(options, "av_startup.video_capacity");
    auto audioCapacity = requiredCapacity(options, "av_startup.audio_capacity");
    auto videoBytes = requiredPositiveInt64NodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.video_byte_capacity");
    auto audioBytes = requiredPositiveInt64NodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.audio_byte_capacity");
    auto maximumVideoUnitBytes = requiredPositiveInt64NodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.maximum_video_unit_bytes");
    auto maximumAudioUnitBytes = requiredPositiveInt64NodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.maximum_audio_unit_bytes");
    auto videoIdentity = requiredNodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.video_identity");
    auto audioIdentity = requiredNodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.audio_identity");
    auto topology = requiredNodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.topology");
    auto group = requiredNodeOption(
        options, "MediaAvStartupCoordinatorNode", "av_startup.sync_group");

    if (!requireKey) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(requireKey.error());
    if (!trimAudio) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(trimAudio.error());
    if (!allowDegraded) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(allowDegraded.error());
    if (!wait) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(wait.error());
    if (!preroll) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(preroll.error());
    if (!keyWait) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(keyWait.error());
    if (!trim) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(trim.error());
    if (!skew) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(skew.error());
    if (!gap) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(gap.error());
    if (!lead) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(lead.error());
    if (!videoCapacity) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(videoCapacity.error());
    if (!audioCapacity) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(audioCapacity.error());
    if (!videoBytes) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(videoBytes.error());
    if (!audioBytes) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(audioBytes.error());
    if (!maximumVideoUnitBytes) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(maximumVideoUnitBytes.error());
    if (!maximumAudioUnitBytes) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(maximumAudioUnitBytes.error());
    if (!videoIdentity) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(videoIdentity.error());
    if (!audioIdentity) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(audioIdentity.error());
    if (!topology) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(topology.error());
    if (!group) return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(group.error());

    if (allowDegraded.value()) {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            ::media::ErrorInfo::unsupported(
                "Planned degraded clock startup is not supported"));
    }
    MediaAvSyncTopology topologyValue;
    if (topology.value() == "separate_rtp") {
        topologyValue = MediaAvSyncTopology::SeparateRtpToSeparateRtp;
    } else if (topology.value() == "mpegts") {
        topologyValue = MediaAvSyncTopology::MpegTsToMpegTs;
    } else {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode rejects unknown planned topology"));
    }
    MediaAvSyncGroupKey groupKey(std::move(group).value());
    if (!groupKey.valid()) {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaAvStartupCoordinatorNode requires a valid planned sync group"));
    }
    auto created = MediaAvStartupCoordinator::create(MediaAvStartupConfig{
        requireKey.value(), trimAudio.value(), allowDegraded.value(), topologyValue,
        MediaRunningTime::fromNanoseconds(wait.value()),
        MediaRunningTime::fromNanoseconds(preroll.value()),
        MediaRunningTime::fromNanoseconds(keyWait.value()),
        MediaRunningTime::fromNanoseconds(trim.value()),
        MediaRunningTime::fromNanoseconds(skew.value()),
        MediaRunningTime::fromNanoseconds(gap.value()),
        MediaRunningTime::fromNanoseconds(lead.value()),
        videoCapacity.value(), audioCapacity.value(),
        static_cast<std::uint64_t>(videoBytes.value()),
        static_cast<std::uint64_t>(audioBytes.value()),
        static_cast<std::uint64_t>(maximumVideoUnitBytes.value()),
        static_cast<std::uint64_t>(maximumAudioUnitBytes.value()),
        std::move(videoIdentity).value(), std::move(audioIdentity).value()});
    if (!created) {
        return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::failure(
            created.error().toErrorInfo());
    }
    return ::media::Result<MediaAvStartupCoordinatorNodePreparation>::success(
        MediaAvStartupCoordinatorNodePreparation(
            std::make_unique<MediaAvStartupCoordinator>(std::move(created).value()),
            std::make_shared<MediaAvStartupGenerationState>(std::move(groupKey))));
}

} // namespace media::ffmpeg::graph
