#include "unit/fixtures/ScheduledRtpOutputIntegrationRuntime.h"

#include "common/AvSyncRuntimeTestSupport.h"
#include "unit/fixtures/ScheduledRtpDecodeSampleFixture.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/segments/MediaScheduledRtpOutputSegmentBuilder.h"
#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace media_transcode::test {
namespace {

using namespace ::media::ffmpeg::graph;

constexpr MediaRunningTime milliseconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class RealtimeIntegrationClock final : public MediaMasterClock {
public:
    RealtimeIntegrationClock() : m_started(std::chrono::steady_clock::now()) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        const auto elapsed = std::chrono::steady_clock::now() - m_started;
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(
                std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                    .count()));
    }

private:
    std::chrono::steady_clock::time_point m_started;
};

struct IntegrationGraph final {
    MediaGraph graph;
    MediaNodeId binder;
    MediaNodeId videoSender;
    MediaNodeId audioSender;
    MediaNodeId publisher;
};

::media::ErrorInfo harnessError(std::string message)
{
    return ::media::ErrorInfo::internalError(std::move(message));
}

MediaEndpoint addSource(
    MediaGraph& graph,
    const char* name,
    const char* port,
    MediaStreamKind stream,
    MediaEdgeKind edge,
    MediaPayloadKind payload,
    bool multiple = false)
{
    const MediaNodeId node = graph.addNode(MediaNodeKind::DebugDump, name);
    graph.addOutputPort(
        node, port, stream, edge, payload, true, multiple);
    return {node, port};
}

::media::Result<IntegrationGraph> buildIntegrationGraph(
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    IntegrationGraph fixture;
    const MediaEndpoint epoch = addSource(
        fixture.graph, "decode-epoch", "activated",
        MediaStreamKind::Metadata, MediaEdgeKind::Event,
        MediaPayloadKind::GraphEvent, true);
    const MediaEndpoint videoCodec = addSource(
        fixture.graph, "decode-video-codec", "codec",
        MediaStreamKind::Video, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);
    const MediaEndpoint audioCodec = addSource(
        fixture.graph, "decode-audio-codec", "codec",
        MediaStreamKind::Audio, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecContext);
    const MediaEndpoint scheduledVideo = addSource(
        fixture.graph, "decode-video-scheduled", "scheduled",
        MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);
    const MediaEndpoint scheduledAudio = addSource(
        fixture.graph, "decode-audio-scheduled", "scheduled",
        MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket,
        MediaPayloadKind::Packet);

    const MediaNodeId schedulerVideo = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "decode-scheduler-video");
    const MediaNodeId schedulerAudio = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "decode-scheduler-audio");
    const MediaNodeId scheduler = fixture.graph.addNode(
        MediaNodeKind::AvOutputScheduler, "decode-scheduler");
    fixture.binder = fixture.graph.addNode(
        MediaNodeKind::PlaybackEpochBinder, "decode-binder");
    const MediaNodeId schedulerSink = fixture.graph.addNode(
        MediaNodeKind::DebugDump, "decode-scheduler-sink");
    fixture.graph.setNodeOption(
        scheduler, "av_scheduler.sync_group", plan.groupKey.value());
    fixture.graph.setNodeOption(
        fixture.binder, "playback_epoch_binder.sync_group",
        plan.groupKey.value());
    fixture.graph.addOutputPort(
        schedulerVideo, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    fixture.graph.addOutputPort(
        schedulerAudio, "packet", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    fixture.graph.addInputPort(
        scheduler, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    fixture.graph.addInputPort(
        scheduler, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    fixture.graph.addOutputPort(
        scheduler, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    fixture.graph.addInputPort(
        schedulerSink, "scheduled", MediaStreamKind::Any,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, true, true);
    const auto queue = MediaGraphBuildSupport::blockingQueuePolicy(8);
    fixture.graph.connect(
        schedulerVideo, "packet", scheduler, "video", "decode-video", queue);
    fixture.graph.connect(
        schedulerAudio, "packet", scheduler, "audio", "decode-audio", queue);
    fixture.graph.connect(
        scheduler, "scheduled", schedulerSink, "scheduled",
        "decode-scheduled", queue);
    addPlaybackEpochReleaseBoundary(fixture.graph, fixture.binder);

    auto output = MediaScheduledRtpOutputSegmentBuilder::build(
        fixture.graph,
        {"decode-output", epoch, videoCodec, audioCodec,
         scheduledVideo, scheduledAudio},
        plan);
    if (!output) {
        return ::media::Result<IntegrationGraph>::failure(output.error());
    }
    fixture.videoSender = output.value().videoSender;
    fixture.audioSender = output.value().audioSender;
    fixture.publisher = output.value().sdpPublisher;
    return ::media::Result<IntegrationGraph>::success(std::move(fixture));
}

::media::Status pushActivationAndCodecs(
    MediaGraphExecutionContext& context,
    MediaNodeId videoSender,
    MediaNodeId audioSender,
    const MediaAvSyncGroupKey& groupKey,
    AVCodecContext& videoCodec,
    AVCodecContext& audioCodec)
{
    const MediaPlaybackEpoch epoch{milliseconds(0), milliseconds(0), 1};
    const MediaAudioPlaybackOrigin audioOrigin{
        1, milliseconds(0), milliseconds(0), 0, audioCodec.sample_rate};
    auto videoActivation = MediaPlaybackEpochActivatedBuffer::create(
        groupKey, epoch, audioOrigin);
    auto audioActivation = MediaPlaybackEpochActivatedBuffer::create(
        groupKey, epoch, audioOrigin);
    auto videoMetadata = FFmpegBufferFactory::borrowCodecContext(&videoCodec);
    auto audioMetadata = FFmpegBufferFactory::borrowCodecContext(&audioCodec);
    if (!videoActivation || !audioActivation || !videoMetadata ||
        !audioMetadata) {
        return ::media::Status::failure(
            !videoActivation ? videoActivation.error()
            : !audioActivation ? audioActivation.error()
            : !videoMetadata ? videoMetadata.error()
                             : audioMetadata.error());
    }
    MediaChannel* videoEpoch = context.findInputChannel(videoSender, "epoch");
    MediaChannel* audioEpoch = context.findInputChannel(audioSender, "epoch");
    MediaChannel* videoCodecInput =
        context.findInputChannel(videoSender, "codec");
    MediaChannel* audioCodecInput =
        context.findInputChannel(audioSender, "codec");
    if (!videoEpoch || !audioEpoch || !videoCodecInput || !audioCodecInput ||
        !videoEpoch->push(std::move(videoActivation).value()) ||
        !audioEpoch->push(std::move(audioActivation).value()) ||
        !videoCodecInput->push(std::move(videoMetadata).value()) ||
        !audioCodecInput->push(std::move(audioMetadata).value())) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP decode could not queue activation and codec inputs"));
    }
    return ::media::Status::success();
}

::media::Status verifyAndRestoreDescription(
    MediaGraphExecutionContext& context,
    MediaNodeId publisher,
    const char* port,
    MediaScheduledStream expectedStream)
{
    MediaChannel* input = context.findInputChannel(publisher, port);
    MediaBufferRef description;
    if (!input || !input->tryPop(description)) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP sender did not emit its opened description"));
    }
    const auto* typed = dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(
        description.get());
    if (!typed || typed->stream() != expectedStream ||
        typed->generation() != 1) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP sender emitted an invalid opened description"));
    }
    return input->push(std::move(description));
}

} // namespace

ScheduledRtpOutputIntegrationRuntime::ScheduledRtpOutputIntegrationRuntime(
    std::unique_ptr<MediaGraphRuntime> runtime,
    MediaNodeId videoSender,
    MediaNodeId audioSender,
    MediaRunningTime videoLead,
    MediaRunningTime audioLead) noexcept
    : m_runtime(std::move(runtime)),
      m_videoSender(videoSender),
      m_audioSender(audioSender),
      m_videoLead(videoLead),
      m_audioLead(audioLead)
{
}

ScheduledRtpOutputIntegrationRuntime::ScheduledRtpOutputIntegrationRuntime(
    ScheduledRtpOutputIntegrationRuntime&&) noexcept = default;

ScheduledRtpOutputIntegrationRuntime&
ScheduledRtpOutputIntegrationRuntime::operator=(
    ScheduledRtpOutputIntegrationRuntime&&) noexcept = default;

ScheduledRtpOutputIntegrationRuntime::~ScheduledRtpOutputIntegrationRuntime()
{
    if (m_runtime) m_runtime->abort();
}

::media::Result<ScheduledRtpOutputIntegrationRuntime>
ScheduledRtpOutputIntegrationRuntime::openSendersAndPublish(
    const MediaRealtimeAvSyncRuntimePlan& plan,
    AVCodecContext& videoCodec,
    AVCodecContext& audioCodec)
{
    using RuntimeResult =
        ::media::Result<ScheduledRtpOutputIntegrationRuntime>;
    const auto* separate = std::get_if<MediaSeparateRtpOutputRuntimePlan>(
        &plan.protocolOutput);
    if (!separate) {
        return RuntimeResult::failure(harnessError(
            "scheduled RTP decode requires a separate RTP output plan"));
    }
    auto graph = buildIntegrationGraph(plan);
    if (!graph) return RuntimeResult::failure(graph.error());
    auto clock = std::make_shared<RealtimeIntegrationClock>();
    auto runtime = std::make_unique<MediaGraphRuntime>(
        std::make_shared<FixedAvSyncClockSource>(clock));
    MediaRealtimeExecutableGraph executable;
    executable.graph = std::move(graph.value().graph);
    executable.avSyncBinding.emplace(MediaAvSyncRuntimeBinding{
        plan.groupKey, plan.synchronization, plan.transition});
    if (auto compiled = runtime->compile(std::move(executable)); !compiled) {
        return RuntimeResult::failure(compiled.error());
    }
    if (auto registered = runtime->registerDefaultRuntimeNodes();
        !registered) {
        return RuntimeResult::failure(registered.error());
    }
    const MediaPlaybackEpoch epoch{milliseconds(0), milliseconds(0), 1};
    if (!activateInitialThroughRelease(
            *runtime, graph.value().binder, plan.groupKey, epoch,
            audioCodec.sample_rate)) {
        return RuntimeResult::failure(harnessError(
            "scheduled RTP decode could not activate the sync group"));
    }
    auto* video = dynamic_cast<MediaScheduledRtpSenderNode*>(
        runtime->scheduler().findNode(graph.value().videoSender));
    auto* audio = dynamic_cast<MediaScheduledRtpSenderNode*>(
        runtime->scheduler().findNode(graph.value().audioSender));
    auto* publisher = dynamic_cast<MediaDualMediaSdpPublisherNode*>(
        runtime->scheduler().findNode(graph.value().publisher));
    if (!video || !audio || !publisher) {
        return RuntimeResult::failure(harnessError(
            "scheduled RTP decode did not receive production runtime nodes"));
    }
    auto& context = runtime->context();
    if (auto started = video->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto started = audio->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto queued = pushActivationAndCodecs(
            context, graph.value().videoSender, graph.value().audioSender,
            plan.groupKey, videoCodec, audioCodec); !queued) {
        return RuntimeResult::failure(queued.error());
    }
    auto videoOpened = video->process(context);
    auto audioOpened = audio->process(context);
    if (!videoOpened || !audioOpened) {
        return RuntimeResult::failure(
            !videoOpened ? videoOpened.error() : audioOpened.error());
    }
    if (auto checked = verifyAndRestoreDescription(
            context, graph.value().publisher, "video",
            MediaScheduledStream::Video); !checked) {
        return RuntimeResult::failure(checked.error());
    }
    if (auto checked = verifyAndRestoreDescription(
            context, graph.value().publisher, "audio",
            MediaScheduledStream::Audio); !checked) {
        return RuntimeResult::failure(checked.error());
    }
    if (auto started = publisher->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    auto published = publisher->process(context);
    if (!published ||
        published.value().state != MediaNodeProcessState::Finished) {
        return RuntimeResult::failure(
            !published ? published.error()
                       : harnessError(
                             "scheduled RTP decode publisher did not finish"));
    }
    return RuntimeResult::success(ScheduledRtpOutputIntegrationRuntime(
        std::move(runtime), graph.value().videoSender,
        graph.value().audioSender, separate->video.senderLead,
        separate->audio.senderLead));
}

::media::Status ScheduledRtpOutputIntegrationRuntime::sendAccessUnits(
    const ScheduledRtpDecodeSampleFixture& sample)
{
    auto* video = dynamic_cast<MediaScheduledRtpSenderNode*>(
        m_runtime->scheduler().findNode(m_videoSender));
    auto* audio = dynamic_cast<MediaScheduledRtpSenderNode*>(
        m_runtime->scheduler().findNode(m_audioSender));
    if (!video || !audio) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP decode lost its sender nodes"));
    }
    const auto started = std::chrono::steady_clock::now();
    std::uint64_t sequence = 1;
    for (const auto& unit : sample.accessUnits()) {
        std::this_thread::sleep_until(
            started + std::chrono::nanoseconds(
                unit.dispatchOffset.nanoseconds()));
        const bool isVideo = unit.stream == MediaScheduledStream::Video;
        const MediaNodeId senderId = isVideo ? m_videoSender : m_audioSender;
        MediaScheduledRtpSenderNode* sender = isVideo ? video : audio;
        const MediaRunningTime lead = isVideo ? m_videoLead : m_audioLead;
        auto packet = FFmpegBufferFactory::clonePacket(
            unit.packet.get(),
            isVideo ? MediaStreamKind::Video : MediaStreamKind::Audio);
        if (!packet) return ::media::Status::failure(packet.error());
        auto dispatch = unit.presentationOnMaster.checkedSubtract(lead);
        if (!dispatch) return ::media::Status::failure(dispatch.error());
        auto scheduled = MediaScheduledAccessUnit::create({
            std::move(packet).value(), unit.stream,
            unit.presentationOnMaster, dispatch.value(),
            unit.presentationOnMaster, dispatch.value(), milliseconds(1), 1,
            MediaSourceAccessUnitSequence(sequence++), std::nullopt,
            std::nullopt,
            isVideo
                ? std::optional<MediaVideoSyncDecisionKind>(
                      MediaVideoSyncDecisionKind::Display)
                : std::nullopt});
        if (!scheduled) return ::media::Status::failure(scheduled.error());
        MediaChannel* input =
            m_runtime->context().findInputChannel(senderId, "scheduled");
        if (!input || !input->push(std::move(scheduled).value())) {
            return ::media::Status::failure(harnessError(
                "scheduled RTP decode could not queue an access unit"));
        }
        for (int attempt = 0; input->size() != 0 && attempt != 3; ++attempt) {
            auto processed = sender->process(m_runtime->context());
            if (!processed) {
                return ::media::Status::failure(processed.error());
            }
        }
        if (input->size() != 0) {
            return ::media::Status::failure(harnessError(
                "scheduled RTP sender did not consume its access unit"));
        }
    }
    return ::media::Status::success();
}

} // namespace media_transcode::test
