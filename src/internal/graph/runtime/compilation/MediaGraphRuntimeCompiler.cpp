#include "internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h"
#include "internal/graph/sync/startup/MediaAvStartupVideoPreparationState.h"

#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/compilation/MediaAvSyncRuntimeBootstrap.h"
#include "internal/graph/runtime/compilation/MediaAvGenerationParticipantAssembler.h"
#include "internal/graph/runtime/factory/MediaRuntimeNodeFactory.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNodePlanCodec.h"
#include "internal/graph/nodes/sync/MediaAvSyncSourceClockModeNodeOptionCodec.h"
#include "internal/graph/nodes/sync/MediaDemuxPacketClockBinderNodePlanCodec.h"
#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

#include <chrono>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

class ProductionAvSyncClockSource final : public MediaAvSyncClockSource {
public:
    ::media::Result<MediaAvSyncClockBundle> capture(
        bool requireSharedNtpEpoch) override
    {
        auto masterClock = std::make_shared<MediaSteadyMasterClock>(
            MediaRunningTime::fromNanoseconds(0));
        std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch;
        if (requireSharedNtpEpoch) {
            auto masterNow = masterClock->now();
            if (!masterNow) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    masterNow.error());
            }
            const auto wallNow =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
            auto epoch = MediaSharedNtpEpoch::create(
                masterNow.value(), wallNow);
            if (!epoch) {
                return ::media::Result<MediaAvSyncClockBundle>::failure(
                    epoch.error());
            }
            sharedNtpEpoch = std::make_shared<const MediaSharedNtpEpoch>(
                std::move(epoch).value());
        }
        return ::media::Result<MediaAvSyncClockBundle>::success(
            MediaAvSyncClockBundle{std::move(masterClock),
                                   std::move(sharedNtpEpoch)});
    }
};

bool isLegacyProductionAvSyncAuthority(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::RtpMux:
    case MediaNodeKind::RtpOutput:
    case MediaNodeKind::SdpWriter:
    case MediaNodeKind::PacketNormalize:
    case MediaNodeKind::VideoTimestamp:
    case MediaNodeKind::PacketStartGate:
        return true;
    default:
        return false;
    }
}

} // namespace

::media::Status MediaGraphRuntimeCompiler::validateBindings(const MediaRealtimeExecutableGraph& executable)
{
    constexpr std::string_view SyncGroupSuffix = ".sync_group";
    constexpr std::string_view SchedulerGroupKey = "av_scheduler.sync_group";
    constexpr std::string_view BinderGroupKey = "playback_epoch_binder.sync_group";
    constexpr std::string_view StartupClockGroupKey = "av_startup_clock.sync_group";
    constexpr std::string_view SequencerGroupKey =
        "activated_startup_release_sequencer.sync_group";
    constexpr std::string_view BoundReleaseExtractorGroupKey =
        "av_bound_release_extractor.sync_group";
    constexpr std::string_view RtpBinderGroupKey = "rtp_clock_binder.sync_group";
    constexpr std::string_view DemuxBinderGroupKey =
        "demux_clock_binder.sync_group";
    constexpr std::string_view LockedPacketGateGroupKey =
        "locked_packet_gate.sync_group";
    constexpr std::string_view CoordinatorGroupKey = "av_startup.sync_group";
    constexpr std::string_view AudioDriftControllerGroupKey =
        "audio_drift_controller.sync_group";
    constexpr std::string_view ScheduledRtpSenderGroupKey =
        "scheduled_rtp.sync_group";
    constexpr std::string_view ScheduledTsAdapterGroupKey =
        "scheduled_ts_adapter.sync_group";
    constexpr std::string_view ProjectMpegTsPlanGroupKey =
        "project_mpeg_ts_plan.sync_group";
    constexpr std::string_view MpegTsRtpSdpGroupKey =
        "mpegts_rtp_sdp.sync_group";
    std::unordered_set<std::uint64_t> bindingIds;
    std::size_t schedulerCount = 0;
    std::size_t binderCount = 0;
    std::size_t sequencerCount = 0;
    std::size_t scheduledRtpSenderCount = 0;
    std::size_t scheduledTsAdapterCount = 0;
    std::size_t projectMpegTsPlanSourceCount = 0;
    std::size_t dualMediaSdpPublisherCount = 0;
    std::size_t mpegTsRtpSdpPublisherCount = 0;
    std::size_t projectMpegTsMuxCount = 0;
    std::size_t udpOutputCount = 0;
    std::size_t schedulerReferenceCount = 0;
    std::size_t binderReferenceCount = 0;
    std::size_t sequencerReferenceCount = 0;
    std::size_t scheduledRtpSenderReferenceCount = 0;
    std::size_t scheduledTsAdapterReferenceCount = 0;
    std::size_t projectMpegTsPlanSourceReferenceCount = 0;
    std::size_t mpegTsRtpSdpPublisherReferenceCount = 0;
    std::size_t legacyProductionAuthorityCount = 0;
    std::size_t rtpClockBinderCount = 0;
    std::size_t rtpClockBinderReferenceCount = 0;
    std::size_t rtpClockSnapshotCount = 0;
    std::size_t rtpSourceClockAdapterCount = 0;
    std::size_t rtpClockGroupCount = 0;
    std::size_t mpegTsDemuxCount = 0;
    std::size_t demuxClockBinderCount = 0;
    std::size_t demuxClockBinderReferenceCount = 0;
    std::size_t demuxVideoClockBinderCount = 0;
    std::size_t demuxAudioClockBinderCount = 0;
    for (const auto& binding : executable.inputBindings) {
        if (!binding.nodeId.isValid() || !binding.prepared.valid() ||
            !bindingIds.insert(binding.nodeId.value).second) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MediaGraphRuntime duplicate or invalid prepared input binding"));
        }
        const MediaNode* node = executable.graph.findNode(binding.nodeId);
        if (!node || node->kind != MediaNodeKind::RealtimeInput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MediaGraphRuntime prepared binding target is not RealtimeInput"));
        }
    }
    for (const MediaNode& node : executable.graph.nodes()) {
        if (node.kind == MediaNodeKind::AvOutputScheduler) ++schedulerCount;
        if (node.kind == MediaNodeKind::PlaybackEpochBinder) ++binderCount;
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer)
            ++sequencerCount;
        if (node.kind == MediaNodeKind::ScheduledRtpSender)
            ++scheduledRtpSenderCount;
        if (node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter)
            ++scheduledTsAdapterCount;
        if (node.kind == MediaNodeKind::ProjectMpegTsPlanSource)
            ++projectMpegTsPlanSourceCount;
        if (node.kind == MediaNodeKind::DualMediaSdpPublisher)
            ++dualMediaSdpPublisherCount;
        if (node.kind == MediaNodeKind::MpegTsRtpSdpPublisher)
            ++mpegTsRtpSdpPublisherCount;
        if (node.kind == MediaNodeKind::FileMux)
            ++projectMpegTsMuxCount;
        if (node.kind == MediaNodeKind::FileOutput)
            ++udpOutputCount;
        if (isLegacyProductionAvSyncAuthority(node.kind))
            ++legacyProductionAuthorityCount;
        if (node.kind == MediaNodeKind::RtpPacketClockBinder)
            ++rtpClockBinderCount;
        if (node.kind == MediaNodeKind::RtpClockSnapshotFanout)
            ++rtpClockSnapshotCount;
        if (node.kind == MediaNodeKind::RtpSourceClockStateAdapter)
            ++rtpSourceClockAdapterCount;
        if (node.kind == MediaNodeKind::RtpClockGroup)
            ++rtpClockGroupCount;
        if (node.kind == MediaNodeKind::MpegTsDemux)
            ++mpegTsDemuxCount;
        if (node.kind == MediaNodeKind::ProjectMpegTsPlanSource) {
            if (!executable.avSyncBinding ||
                !executable.avSyncBinding->plan.projectMpegTsOutput ||
                !executable.avSyncBinding->plan.projectMpegTsOutput
                     ->outputMux) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Project MPEG-TS plan source requires its planner output binding"));
            }
            auto decoded =
                MediaProjectMpegTsPlanSourceNodePlanCodec::decode(node);
            if (!decoded) {
                return ::media::Status::failure(decoded.error());
            }
            if (decoded.value().groupKey !=
                    executable.avSyncBinding->groupKey ||
                decoded.value().outputPlan.protocol.muxPlan().parameters()
                        .transportKind !=
                    executable.avSyncBinding->plan.projectMpegTsOutput
                        ->outputMux->parameters().transportKind) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Project MPEG-TS plan source conflicts with its planner transport authority"));
            }
        }
        if (node.kind == MediaNodeKind::DemuxPacketClockBinder) {
            ++demuxClockBinderCount;
            if (!executable.avSyncBinding) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Demux binder requires its A/V planner binding"));
            }
            auto decoded =
                MediaDemuxPacketClockBinderNodePlanCodec::decode(node);
            if (!decoded) {
                return ::media::Status::failure(decoded.error());
            }
            if (auto exact =
                    MediaDemuxPacketClockBinderNodePlanCodec::
                        validateAgainstPlanner(
                            decoded.value(),
                            executable.avSyncBinding->groupKey,
                            executable.avSyncBinding->plan);
                !exact) {
                return exact;
            }
            if (decoded.value().stream == MediaScheduledStream::Video) {
                ++demuxVideoClockBinderCount;
            } else {
                ++demuxAudioClockBinderCount;
            }
        }
        if (node.kind == MediaNodeKind::AvStartupCoordinator) {
            if (!executable.avSyncBinding ||
                !executable.avSyncBinding->plan.sourceClockMode) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "A/V startup coordinator requires its planner source clock binding"));
            }
            auto encoded = requiredNodeOption(
                &node.options, "MediaAvStartupCoordinatorNode",
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
            if (decoded.value() !=
                *executable.avSyncBinding->plan.sourceClockMode) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "A/V startup coordinator source clock conflicts with its planner binding"));
            }
        }
        if (node.kind == MediaNodeKind::RealtimeInput && !bindingIds.contains(node.id.value)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized("MediaGraphRuntime missing prepared RealtimeInput binding"));
        }
        if (node.kind == MediaNodeKind::AudioDriftController &&
            !node.options.has(std::string(AudioDriftControllerGroupKey))) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime audio drift controller requires its planned sync group"));
        }
        for (const auto& [key, value] : node.options.values()) {
            if (!key.ends_with(SyncGroupSuffix)) continue;
            const bool schedulerConsumer =
                node.kind == MediaNodeKind::AvOutputScheduler &&
                key == SchedulerGroupKey;
            const bool binderConsumer =
                node.kind == MediaNodeKind::PlaybackEpochBinder &&
                key == BinderGroupKey;
            const bool startupClockConsumer =
                node.kind == MediaNodeKind::AvStartupClock &&
                key == StartupClockGroupKey;
            const bool sequencerConsumer =
                node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer &&
                key == SequencerGroupKey;
            const bool boundReleaseExtractorConsumer =
                node.kind == MediaNodeKind::AvBoundReleaseExtractor &&
                key == BoundReleaseExtractorGroupKey;
            const bool rtpBinderConsumer =
                node.kind == MediaNodeKind::RtpPacketClockBinder &&
                key == RtpBinderGroupKey;
            const bool demuxBinderConsumer =
                node.kind == MediaNodeKind::DemuxPacketClockBinder &&
                key == DemuxBinderGroupKey;
            const bool lockedPacketGateConsumer =
                node.kind == MediaNodeKind::LockedPacketGate &&
                key == LockedPacketGateGroupKey;
            const bool coordinatorConsumer =
                node.kind == MediaNodeKind::AvStartupCoordinator &&
                key == CoordinatorGroupKey;
            const bool audioDriftControllerConsumer =
                node.kind == MediaNodeKind::AudioDriftController &&
                key == AudioDriftControllerGroupKey;
            const bool scheduledRtpSenderConsumer =
                node.kind == MediaNodeKind::ScheduledRtpSender &&
                key == ScheduledRtpSenderGroupKey;
            const bool scheduledTsAdapterConsumer =
                node.kind == MediaNodeKind::ScheduledTsAccessUnitAdapter &&
                key == ScheduledTsAdapterGroupKey;
            const bool projectMpegTsPlanSourceConsumer =
                node.kind == MediaNodeKind::ProjectMpegTsPlanSource &&
                key == ProjectMpegTsPlanGroupKey;
            const bool mpegTsRtpSdpPublisherConsumer =
                node.kind == MediaNodeKind::MpegTsRtpSdpPublisher &&
                key == MpegTsRtpSdpGroupKey;
            if (!schedulerConsumer && !binderConsumer &&
                !startupClockConsumer && !sequencerConsumer &&
                !boundReleaseExtractorConsumer &&
                !rtpBinderConsumer && !demuxBinderConsumer &&
                !lockedPacketGateConsumer &&
                !coordinatorConsumer && !audioDriftControllerConsumer &&
                !scheduledRtpSenderConsumer &&
                !scheduledTsAdapterConsumer &&
                !projectMpegTsPlanSourceConsumer &&
                !mpegTsRtpSdpPublisherConsumer) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime found an unsupported A/V sync group consumer"));
            }
            if (schedulerConsumer) ++schedulerReferenceCount;
            if (binderConsumer) ++binderReferenceCount;
            if (sequencerConsumer) ++sequencerReferenceCount;
            if (scheduledRtpSenderConsumer)
                ++scheduledRtpSenderReferenceCount;
            if (scheduledTsAdapterConsumer)
                ++scheduledTsAdapterReferenceCount;
            if (projectMpegTsPlanSourceConsumer)
                ++projectMpegTsPlanSourceReferenceCount;
            if (mpegTsRtpSdpPublisherConsumer)
                ++mpegTsRtpSdpPublisherReferenceCount;
            if (rtpBinderConsumer) ++rtpClockBinderReferenceCount;
            if (demuxBinderConsumer) ++demuxClockBinderReferenceCount;
            if (value.empty() || !executable.avSyncBinding) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime synchronized node requires an A/V sync binding"));
            }
            if (value != executable.avSyncBinding->groupKey.value()) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime synchronized node group does not match its binding"));
            }
        }
    }
    if (executable.avSyncBinding) {
        if (!executable.avSyncBinding->groupKey.valid()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime A/V sync binding has an invalid group"));
        }
        if (auto status = MediaAvSyncPlanValidator::validate(
                executable.avSyncBinding->plan); !status) {
            return status;
        }
        if (schedulerCount != 1 || binderCount != 1 || sequencerCount != 1 ||
            schedulerReferenceCount != 1 || binderReferenceCount != 1 ||
            sequencerReferenceCount != 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime A/V sync binding requires exactly one scheduler, binder, and activation release sequencer"));
        }
        if (!executable.avSyncBinding->plan.sourceClockMode) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime A/V sync binding requires its planned source clock"));
        }
        switch (*executable.avSyncBinding->plan.sourceClockMode) {
        case MediaAvSyncSourceClockMode::RtpSenderReports:
            if (rtpClockBinderCount != 2 ||
                rtpClockBinderReferenceCount != 2 ||
                rtpClockSnapshotCount != 1 ||
                rtpSourceClockAdapterCount != 1 ||
                rtpClockGroupCount != 1 ||
                mpegTsDemuxCount != 0 ||
                demuxClockBinderCount != 0 ||
                demuxClockBinderReferenceCount != 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime RTP input requires exactly two RTP binders and no demux binders"));
            }
            break;
        case MediaAvSyncSourceClockMode::MpegTsPcr:
            if (rtpClockBinderCount != 0 ||
                rtpClockBinderReferenceCount != 0 ||
                rtpClockSnapshotCount != 0 ||
                rtpSourceClockAdapterCount != 0 ||
                rtpClockGroupCount != 0 ||
                mpegTsDemuxCount != 1 ||
                demuxClockBinderCount != 0 ||
                demuxClockBinderReferenceCount != 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime MPEG-TS input rejects separate packet clock binders"));
            }
            break;
        case MediaAvSyncSourceClockMode::DemuxTimestamps:
            if (rtpClockBinderCount != 0 ||
                rtpClockBinderReferenceCount != 0 ||
                rtpClockSnapshotCount != 0 ||
                rtpSourceClockAdapterCount != 0 ||
                rtpClockGroupCount != 0 ||
                mpegTsDemuxCount != 0 ||
                demuxClockBinderCount != 2 ||
                demuxClockBinderReferenceCount != 2 ||
                demuxVideoClockBinderCount != 1 ||
                demuxAudioClockBinderCount != 1) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime demux timestamp input requires exactly one binder per A/V stream"));
            }
            break;
        default:
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported(
                    "MediaGraphRuntime source clock mode is unsupported"));
        }
        const bool hasProtocolOutputAuthority =
            scheduledRtpSenderCount != 0 ||
            scheduledRtpSenderReferenceCount != 0 ||
            dualMediaSdpPublisherCount != 0 ||
            scheduledTsAdapterCount != 0 ||
            scheduledTsAdapterReferenceCount != 0 ||
            projectMpegTsPlanSourceCount != 0 ||
            projectMpegTsPlanSourceReferenceCount != 0 ||
            mpegTsRtpSdpPublisherCount != 0 ||
            mpegTsRtpSdpPublisherReferenceCount != 0 ||
            projectMpegTsMuxCount != 0 ||
            udpOutputCount != 0;
        if (executable.avSyncBinding->assemblyMode ==
            MediaAvSyncBindingAssemblyMode::ComponentCore) {
            if (executable.avSyncBinding->outputAdapter ||
                hasProtocolOutputAuthority) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime component A/V sync assembly rejects output adapter facts and protocol output authorities"));
            }
        } else if (executable.avSyncBinding->assemblyMode !=
                   MediaAvSyncBindingAssemblyMode::ProductionProtocolOutput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime A/V sync binding has an invalid assembly mode"));
        } else if (legacyProductionAuthorityCount != 0) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaGraphRuntime production A/V sync assembly rejects legacy output, timestamp, and startup authorities"));
        } else if (!executable.avSyncBinding->outputAdapter) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MediaGraphRuntime production A/V sync assembly requires a planned output adapter"));
        } else if (*executable.avSyncBinding->outputAdapter ==
                   MediaAvSyncOutputAdapterKind::ScheduledSeparateRtp) {
            if (!executable.avSyncBinding->plan.rtpOutput ||
                executable.avSyncBinding->plan.projectMpegTsOutput) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Separate RTP adapter conflicts with its planner output authority"));
            }
            if (scheduledRtpSenderCount != 2 ||
                scheduledRtpSenderReferenceCount != 2 ||
                dualMediaSdpPublisherCount != 1 ||
                projectMpegTsMuxCount != 0 ||
                udpOutputCount != 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime separate RTP output requires exactly two injected senders and one SDP publisher"));
            }
            if (scheduledTsAdapterCount != 0 ||
                scheduledTsAdapterReferenceCount != 0 ||
                projectMpegTsPlanSourceCount != 0 ||
                projectMpegTsPlanSourceReferenceCount != 0 ||
                mpegTsRtpSdpPublisherCount != 0 ||
                mpegTsRtpSdpPublisherReferenceCount != 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime separate RTP output rejects MPEG-TS output authorities"));
            }
        } else if (*executable.avSyncBinding->outputAdapter ==
                   MediaAvSyncOutputAdapterKind::ProjectMpegTs) {
            if (executable.avSyncBinding->plan.rtpOutput ||
                !executable.avSyncBinding->plan.projectMpegTsOutput) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Project MPEG-TS adapter conflicts with its planner output authority"));
            }
            if (scheduledTsAdapterCount != 1 ||
                scheduledTsAdapterReferenceCount != 1 ||
                projectMpegTsPlanSourceCount != 1 ||
                projectMpegTsPlanSourceReferenceCount != 1 ||
                projectMpegTsMuxCount != 1) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime MPEG-TS output requires exactly one scheduled adapter and one plan source"));
            }
            if (scheduledRtpSenderCount != 0 ||
                scheduledRtpSenderReferenceCount != 0 ||
                dualMediaSdpPublisherCount != 0) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MediaGraphRuntime MPEG-TS output rejects RTP output authorities"));
            }
            const auto& outputPlan =
                executable.avSyncBinding->plan.projectMpegTsOutput;
            if (!outputPlan || !outputPlan->outputMux) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "MediaGraphRuntime MPEG-TS output requires its planned transport"));
            }
            switch (outputPlan->outputMux->parameters().transportKind) {
            case MediaOutputTransportKind::UdpDatagrams:
                if (udpOutputCount != 1 ||
                    mpegTsRtpSdpPublisherCount != 0 ||
                    mpegTsRtpSdpPublisherReferenceCount != 0) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "MediaGraphRuntime MPEG-TS UDP output requires one UDP sink and rejects MP2T SDP authority"));
                }
                break;
            case MediaOutputTransportKind::RtpAvp:
                if (udpOutputCount != 0 ||
                    mpegTsRtpSdpPublisherCount != 1 ||
                    mpegTsRtpSdpPublisherReferenceCount != 1) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "MediaGraphRuntime MPEG-TS RTP output requires one MP2T SDP publisher and no UDP sink"));
                }
                break;
            default:
                return ::media::Status::failure(
                    ::media::ErrorInfo::unsupported(
                        "MediaGraphRuntime MPEG-TS transport is unsupported"));
            }
        } else {
            return ::media::Status::failure(
                ::media::ErrorInfo::unsupported(
                    "MediaGraphRuntime A/V sync output adapter is unsupported"));
        }
    } else if (schedulerCount != 0 || binderCount != 0 ||
               sequencerCount != 0 || scheduledRtpSenderCount != 0 ||
               dualMediaSdpPublisherCount != 0 ||
               mpegTsRtpSdpPublisherCount != 0 ||
               scheduledTsAdapterCount != 0 ||
               projectMpegTsPlanSourceCount != 0 ||
               rtpClockBinderCount != 0 ||
               demuxClockBinderCount != 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Synchronized runtime nodes require an A/V sync binding"));
    }
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::compile(
    MediaRealtimeExecutableGraph executable,
    MediaGraph& activeGraph,
    std::vector<MediaPreparedRealtimeInputBinding>& activeBindings,
    std::optional<MediaPlaybackEpochActivationCapability>&
        playbackEpochActivationCapability,
    std::shared_ptr<MediaAvStartupVideoPreparationState>&
        videoPreparationState,
    const std::shared_ptr<MediaAvSyncClockSource>& avSyncClockSource,
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    MediaGraphThreadedExecutor& threadedExecutor,
    MediaRuntimeAcceptanceCollector& acceptanceCollector,
    std::atomic_size_t& queueHighWatermark,
    MediaGraphRuntimeState& state)
{
    mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle, "compile.begin");
    const bool requiresDefaultRegistration =
        executable.avSyncBinding.has_value();
    if (auto valid = validateBindings(executable); !valid) {
        mediaGraphDiagnosticLog(
            context.diagnosticsEnabled(),
            MediaGraphDiagnosticPhase::RuntimeLifecycle,
            std::string("compile.failed error=") + valid.error().describe());
        return valid;
    }
    MediaGraphExecutionContext preparedContext;
    preparedContext.setDiagnosticConfig(context.diagnosticConfig());
    auto compiled = preparedContext.compile(executable.graph);
    if (!compiled) {
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeLifecycle,
                                std::string("compile.failed error=") + compiled.error().describe());
        return compiled;
    }
    std::optional<MediaPlaybackEpochActivationCapability> preparedCapability;
    std::shared_ptr<MediaAvStartupVideoPreparationState> preparedVideoPreparation;
    if (executable.avSyncBinding) {
        ProductionAvSyncClockSource productionClockSource;
        MediaAvSyncClockSource& clockSource = avSyncClockSource
            ? *avSyncClockSource
            : static_cast<MediaAvSyncClockSource&>(productionClockSource);
        auto clocks = MediaAvSyncRuntimeBootstrap::createClocks(
            *executable.avSyncBinding, clockSource);
        if (!clocks) {
            return ::media::Status::failure(clocks.error());
        }
        auto registered = MediaAvSyncRuntimeBootstrap::
            registerGroupAndIssueActivationCapability(
            *executable.avSyncBinding, std::move(clocks).value(),
            preparedContext);
        if (!registered) {
            return ::media::Status::failure(registered.error());
        }
        preparedCapability.emplace(std::move(registered).value());
        const bool preparationPlanned = executable.graph.findOutputPort(
            [&]() {
                for (const auto& node : executable.graph.nodes())
                    if (node.kind == MediaNodeKind::PlaybackEpochBinder)
                        return node.id;
                return MediaNodeId::invalid();
            }(), "preparation") != nullptr;
        if (executable.avSyncBinding->videoPreparationState) {
            preparedVideoPreparation =
                executable.avSyncBinding->videoPreparationState;
        } else if (preparationPlanned) {
            auto created = MediaAvStartupVideoPreparationState::create(
                executable.avSyncBinding->groupKey);
            if (!created) return ::media::Status::failure(created.error());
            preparedVideoPreparation = std::move(created).value();
        }
    }
    const std::vector<MediaNodeId> oldExecutionOrder = context.executionOrder();
    context.shutdownAvSyncGroups();
    threadedExecutor.clear();
    scheduler.clear(oldExecutionOrder);
    activeGraph = std::move(executable.graph);
    preparedContext.rebindCompiledGraph(activeGraph);
    context = std::move(preparedContext);
    activeBindings = std::move(executable.inputBindings);
    playbackEpochActivationCapability = std::move(preparedCapability);
    videoPreparationState = std::move(preparedVideoPreparation);
    acceptanceCollector.reset();
    queueHighWatermark = 0;
    state = requiresDefaultRegistration
        ? MediaGraphRuntimeState::DefaultRegistrationPending
        : MediaGraphRuntimeState::Compiled;
    mediaGraphDiagnosticLog(
        context.diagnosticsEnabled(),
        MediaGraphDiagnosticPhase::RuntimeLifecycle,
        requiresDefaultRegistration
            ? "compile.done state=DefaultRegistrationPending"
            : "compile.done state=Compiled");
    return ::media::Status::success();
}

::media::Status MediaGraphRuntimeCompiler::registerNode(
    MediaGraphScheduler& scheduler,
    std::unique_ptr<MediaRuntimeNode> node)
{
    return scheduler.registerNode(std::move(node));
}

::media::Status MediaGraphRuntimeCompiler::registerDefaults(
    MediaGraphExecutionContext& context,
    MediaGraphScheduler& scheduler,
    std::vector<MediaPreparedRealtimeInputBinding>& inputBindings,
    std::optional<MediaPlaybackEpochActivationCapability>&
        playbackEpochActivationCapability,
    const std::shared_ptr<MediaAvStartupVideoPreparationState>&
        videoPreparationState)
{
    if (!context.compiled() || !context.graph()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphRuntime default registration requires compiled graph"));
    }
    std::vector<std::unique_ptr<MediaRuntimeNode>> preparedNodes;
    preparedNodes.reserve(context.graph()->nodes().size());
    const MediaNode* sequencer = nullptr;
    const MediaNode* avOutputScheduler = nullptr;
    const MediaNode* videoFilter = nullptr;
    const MediaNode* releaseExtractor = nullptr;
    const MediaNode* mpegTsRtpSdpPublisher = nullptr;
    std::vector<const MediaNode*> scheduledRtpSenders;
    std::vector<const MediaNode*> demuxClockBinders;
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer) {
            sequencer = &node;
        }
        if (node.kind == MediaNodeKind::AvOutputScheduler) {
            avOutputScheduler = &node;
        }
        if (node.kind == MediaNodeKind::VideoFilter) videoFilter = &node;
        if (node.kind == MediaNodeKind::AvBoundReleaseExtractor)
            releaseExtractor = &node;
        if (node.kind == MediaNodeKind::ScheduledRtpSender)
            scheduledRtpSenders.push_back(&node);
        if (node.kind == MediaNodeKind::DemuxPacketClockBinder)
            demuxClockBinders.push_back(&node);
        if (node.kind == MediaNodeKind::MpegTsRtpSdpPublisher) {
            if (mpegTsRtpSdpPublisher) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "MP2T SDP runtime rejects duplicate publishers"));
            }
            mpegTsRtpSdpPublisher = &node;
        }
    }
    if (videoPreparationState) {
        if (!sequencer) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Video preparation state requires a sequencer node"));
        }
        if (auto bound = videoPreparationState->bindSequencerWakeup(
                context.sharedNodeWakeup(sequencer->id)); !bound) return bound;
        if (videoFilter) {
            if (auto bound = videoPreparationState->bindFilterWakeup(
                    context.sharedNodeWakeup(videoFilter->id)); !bound)
                return bound;
        }
        if (releaseExtractor) {
            if (auto bound = videoPreparationState->bindExtractorWakeup(
                    context.sharedNodeWakeup(releaseExtractor->id)); !bound)
                return bound;
        }
    }
    if (sequencer) {
        if (!playbackEpochActivationCapability) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Activation release sequencer is missing compiler-issued activation authority"));
        }
        if (scheduler.findNode(sequencer->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Activation release sequencer runtime node is already registered"));
        }
    } else if (playbackEpochActivationCapability) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Compiler-issued activation authority has no activation release sequencer"));
    }
    std::shared_ptr<MediaAvSyncGroupRuntime> scheduledRtpGroup;
    for (const MediaNode* sender : scheduledRtpSenders) {
        if (!sender) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "Scheduled RTP sender registration lost its planned node"));
        }
        if (scheduler.findNode(sender->id)) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender runtime node is already registered without compiler injection"));
        }
        auto groupText = requiredNodeOption(
            &sender->options, "MediaScheduledRtpSenderNode",
            "scheduled_rtp.sync_group");
        if (!groupText) return ::media::Status::failure(groupText.error());
        MediaAvSyncGroupKey groupKey(std::move(groupText).value());
        auto exactGroup = context.findAvSyncGroup(groupKey);
        if (!exactGroup || exactGroup->key() != groupKey) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender compiler injection cannot find its exact registered sync group"));
        }
        if (scheduledRtpGroup && scheduledRtpGroup != exactGroup) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP senders do not share the exact registered sync-group runtime"));
        }
        scheduledRtpGroup = std::move(exactGroup);
    }
    std::shared_ptr<MediaDemuxTimestampClockMapper> demuxMapper;
    std::shared_ptr<MediaAvSyncGroupRuntime> demuxGroup;
    const MediaNode* demuxVideoBinder = nullptr;
    const MediaNode* demuxAudioBinder = nullptr;
    std::optional<MediaDecodedDemuxPacketClockBinderNodePlan>
        demuxVideoPlan;
    std::optional<MediaDecodedDemuxPacketClockBinderNodePlan>
        demuxAudioPlan;
    if (!demuxClockBinders.empty()) {
        if (demuxClockBinders.size() != 2) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Demux timestamp runtime requires exactly two binders"));
        }
        std::optional<MediaDemuxTimestampClockMapperConfig> exactConfig;
        for (const MediaNode* binder : demuxClockBinders) {
            auto decoded =
                MediaDemuxPacketClockBinderNodePlanCodec::decode(*binder);
            if (!decoded) {
                return ::media::Status::failure(decoded.error());
            }
            if (exactConfig &&
                *exactConfig != decoded.value().mapper) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Demux timestamp binders disagree on the planner clock product"));
            }
            exactConfig = decoded.value().mapper;
            auto exactGroup =
                context.findAvSyncGroup(decoded.value().groupKey);
            if (!exactGroup ||
                exactGroup->key() != decoded.value().groupKey ||
                (demuxGroup && demuxGroup != exactGroup)) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Demux timestamp binders require one exact registered sync group"));
            }
            if (auto exact =
                    MediaDemuxPacketClockBinderNodePlanCodec::
                        validateAgainstPlanner(
                            decoded.value(), exactGroup->key(),
                            exactGroup->plan());
                !exact) {
                return exact;
            }
            demuxGroup = std::move(exactGroup);
            if (decoded.value().stream ==
                MediaScheduledStream::Video) {
                if (demuxVideoBinder) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "Demux timestamp runtime rejects duplicate video binders"));
                }
                demuxVideoBinder = binder;
                demuxVideoPlan = std::move(decoded).value();
            } else {
                if (demuxAudioBinder) {
                    return ::media::Status::failure(
                        ::media::ErrorInfo::invalidArgument(
                            "Demux timestamp runtime rejects duplicate audio binders"));
                }
                demuxAudioBinder = binder;
                demuxAudioPlan = std::move(decoded).value();
            }
        }
        if (!exactConfig || !demuxVideoBinder || !demuxAudioBinder ||
            !demuxVideoPlan || !demuxAudioPlan || !demuxGroup) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "Demux timestamp runtime lost its complete injection facts"));
        }
        auto created =
            MediaDemuxTimestampClockMapper::create(*exactConfig);
        if (!created) {
            return ::media::Status::failure(created.error());
        }
        demuxMapper = std::move(created).value();
        const std::weak_ptr<MediaNodeWakeup> videoWakeup =
            context.sharedNodeWakeup(demuxVideoBinder->id);
        const std::weak_ptr<MediaNodeWakeup> audioWakeup =
            context.sharedNodeWakeup(demuxAudioBinder->id);
        auto bound = demuxMapper->bindStateChangeNotifiers(
            [videoWakeup]() noexcept {
                if (auto wakeup = videoWakeup.lock()) wakeup->notify();
            },
            [audioWakeup]() noexcept {
                if (auto wakeup = audioWakeup.lock()) wakeup->notify();
            });
        if (!bound) return bound;
    }
    std::shared_ptr<MediaAvSyncGroupRuntime> mpegTsRtpSdpGroup;
    if (mpegTsRtpSdpPublisher) {
        auto groupText = requiredNodeOption(
            &mpegTsRtpSdpPublisher->options,
            "MediaMpegTsRtpSdpPublisherNode",
            "mpegts_rtp_sdp.sync_group");
        if (!groupText) {
            return ::media::Status::failure(groupText.error());
        }
        MediaAvSyncGroupKey groupKey(std::move(groupText).value());
        mpegTsRtpSdpGroup = context.findAvSyncGroup(groupKey);
        if (!mpegTsRtpSdpGroup ||
            mpegTsRtpSdpGroup->key() != groupKey ||
            !mpegTsRtpSdpGroup->sharedNtpEpoch()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MP2T SDP publisher requires its exact registered RTP output sync group"));
        }
    }
    for (const MediaNode& node : context.graph()->nodes()) {
        if (node.kind == MediaNodeKind::ActivatedStartupReleaseSequencer ||
            node.kind == MediaNodeKind::ScheduledRtpSender ||
            node.kind == MediaNodeKind::DemuxPacketClockBinder ||
            node.kind == MediaNodeKind::MpegTsRtpSdpPublisher)
            continue;
        if (scheduler.findNode(node.id)) continue;
        if (!MediaRuntimeNodeFactory::supported(node.kind)) {
            return ::media::Status::failure(::media::ErrorInfo::unsupported(
                "Default runtime registration encountered an unsupported planned node"));
        }
        MediaPreparedRealtimeInputBinding* binding = nullptr;
        for (auto& candidate : inputBindings) {
            if (candidate.nodeId == node.id) { binding = &candidate; break; }
        }
        auto runtimeNode = MediaRuntimeNodeFactory::create(
            node, binding, videoPreparationState);
        if (!runtimeNode) return ::media::Status::failure(runtimeNode.error());
        mediaGraphDiagnosticLog(context.diagnosticsEnabled(), MediaGraphDiagnosticPhase::RuntimeNode,
                                "register node=" + std::to_string(node.id.value) +
                                    " name=" + node.name +
                                    " kind=" + mediaGraphDiagnosticNodeKindName(node.kind));
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    for (const MediaNode* binder : demuxClockBinders) {
        const auto& decoded =
            binder == demuxVideoBinder
            ? *demuxVideoPlan
            : *demuxAudioPlan;
        auto runtimeNode =
            MediaRuntimeNodeFactory::createDemuxPacketClockBinder(
                *binder, decoded, demuxMapper, demuxGroup);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (mpegTsRtpSdpPublisher) {
        auto runtimeNode =
            MediaRuntimeNodeFactory::createMpegTsRtpSdpPublisher(
                *mpegTsRtpSdpPublisher, mpegTsRtpSdpGroup);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    for (const MediaNode* sender : scheduledRtpSenders) {
        auto runtimeNode = MediaRuntimeNodeFactory::createScheduledRtpSender(
            *sender, scheduledRtpGroup);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    std::shared_ptr<MediaAvSyncGroupRuntime> reacquisitionGroup;
    std::optional<MediaAvReacquisitionAssemblyDependencies>
        reacquisitionDependencies;
    if (playbackEpochActivationCapability) {
        if (!avOutputScheduler) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V reacquisition assembly requires the planned output scheduler"));
        }
        auto groupText = requiredNodeOption(
            &avOutputScheduler->options,
            "MediaAvOutputSchedulerNode",
            "av_scheduler.sync_group");
        if (!groupText) {
            return ::media::Status::failure(groupText.error());
        }
        MediaAvSyncGroupKey groupKey(std::move(groupText).value());
        reacquisitionGroup = context.findAvSyncGroup(groupKey);
        if (!reacquisitionGroup ||
            reacquisitionGroup->key() != groupKey) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V reacquisition assembly requires the exact registered sync group"));
        }
        auto dependencies = MediaAvSyncRuntimeBootstrap::
            reacquisitionAssemblyDependencies(
                *playbackEpochActivationCapability,
                reacquisitionGroup);
        if (!dependencies) {
            return ::media::Status::failure(dependencies.error());
        }
        reacquisitionDependencies.emplace(
            std::move(dependencies).value());
    }
    if (sequencer && !scheduler.findNode(sequencer->id)) {
        auto runtimeNode =
            MediaRuntimeNodeFactory::createActivatedStartupReleaseSequencer(
                *sequencer, std::move(*playbackEpochActivationCapability),
                videoPreparationState);
        if (!runtimeNode) {
            return ::media::Status::failure(runtimeNode.error());
        }
        preparedNodes.push_back(std::move(runtimeNode).value());
    }
    if (reacquisitionDependencies) {
        auto assembler = MediaAvGenerationParticipantAssembler::create(
            reacquisitionDependencies->transitionService->transitionPlan());
        if (!assembler) {
            return ::media::Status::failure(assembler.error());
        }
        for (auto& runtimeNode : preparedNodes) {
            auto registration =
                MediaRuntimeNodeFactory::generationPurgeRegistration(
                    *runtimeNode);
            if (!registration) continue;
            auto registered = assembler.value().registerTarget(
                registration->participant,
                std::move(registration->registration));
            if (!registered) return registered;
        }
        auto participants = assembler.value().seal();
        if (!participants) {
            return ::media::Status::failure(participants.error());
        }
        auto coordinator = MediaAvReacquisitionCoordinator::create(
            std::move(reacquisitionDependencies->transitionService),
            std::move(reacquisitionDependencies->masterClock),
            std::move(participants).value());
        if (!coordinator) {
            return ::media::Status::failure(coordinator.error());
        }
        auto installed = reacquisitionGroup->installReacquisitionCoordinator(
            std::move(coordinator).value());
        if (!installed) return installed;
    }
    auto registered = scheduler.registerNodes(std::move(preparedNodes));
    if (!registered) return registered;
    if (sequencer) playbackEpochActivationCapability.reset();
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
