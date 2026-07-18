#include "unit/fixtures/ScheduledRtpOutputIntegrationRuntime.h"

#include "common/AvSyncRuntimeTestSupport.h"
#include "unit/fixtures/ScheduledRtpDecodeSampleFixture.h"
#include "unit/fixtures/ScheduledRtpOutputIntegrationGraph.h"

#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/nodes/sync/MediaAvOutputSchedulerNode.h"
#include "internal/graph/nodes/sync/MediaScheduledOutputRouterNode.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
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

::media::ErrorInfo harnessError(std::string message)
{
    return ::media::ErrorInfo::internalError(std::move(message));
}

::media::Status pushActivationAndCodecs(
    MediaGraphExecutionContext& context,
    MediaNodeId videoSender,
    MediaNodeId audioSender,
    const MediaAvSyncGroupKey& groupKey,
    AVCodecContext& videoCodec,
    AVCodecContext& audioCodec,
    const MediaPlaybackEpoch& epoch)
{
    const MediaAudioPlaybackOrigin audioOrigin{
        epoch.generation, epoch.sourceStart, epoch.masterRelease, 0,
        audioCodec.sample_rate};
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

::media::Result<MediaBufferRef> canonicalAccessUnit(
    const ScheduledRtpDecodeAccessUnit& unit,
    MediaRunningTime senderLead,
    std::uint64_t sequence)
{
    auto packet = FFmpegBufferFactory::clonePacket(
        unit.packet.get(),
        unit.stream == MediaScheduledStream::Video
            ? MediaStreamKind::Video : MediaStreamKind::Audio);
    if (!packet) return ::media::Result<MediaBufferRef>::failure(packet.error());
    auto dispatch = unit.presentationOnMaster.checkedSubtract(senderLead);
    if (!dispatch) {
        return ::media::Result<MediaBufferRef>::failure(dispatch.error());
    }
    auto lineage = createMediaCanonicalLineage(
        unit.presentationOnMaster, dispatch.value(), milliseconds(1),
        MediaDecodeOrderMode::ReorderedRequiresDecodeTime,
        unit.stream == MediaScheduledStream::Video
            ? "scheduled-rtp-decode.video" : "scheduled-rtp-decode.audio",
        MediaSourceAccessUnitSequence(sequence),
        MediaTimeMappingConfidence::Locked, 1);
    if (!lineage) {
        return ::media::Result<MediaBufferRef>::failure(lineage.error());
    }
    return MediaCanonicalAccessUnitBuffer::create(
        std::move(packet).value(), std::move(lineage).value());
}

::media::Status waitForNodeDeadline(
    const MediaMasterClock& clock,
    const MediaNodeProcessResult::DeadlineWait& wait)
{
    auto now = clock.now();
    if (!now) return ::media::Status::failure(now.error());
    if (wait.masterDeadline <= now.value()) return ::media::Status::success();
    auto remaining = wait.masterDeadline.checkedSubtract(now.value());
    if (!remaining) return ::media::Status::failure(remaining.error());
    std::this_thread::sleep_for(
        std::chrono::nanoseconds(remaining.value().nanoseconds()));
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
    std::shared_ptr<MediaMasterClock> clock,
    MediaNodeId scheduler,
    MediaNodeId router,
    MediaNodeId videoSender,
    MediaNodeId audioSender,
    MediaRunningTime videoLead,
    MediaRunningTime audioLead) noexcept
    : m_runtime(std::move(runtime)),
      m_clock(std::move(clock)),
      m_scheduler(scheduler),
      m_router(router),
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
    auto graph = ScheduledRtpOutputIntegrationGraphBuilder::build(plan);
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
    auto now = clock->now();
    if (!now) return RuntimeResult::failure(now.error());
    auto release = now.value().checkedAdd(milliseconds(2'000));
    if (!release) return RuntimeResult::failure(release.error());
    const MediaPlaybackEpoch epoch{milliseconds(0), release.value(), 1};
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
    auto* scheduler = dynamic_cast<MediaAvOutputSchedulerNode*>(
        runtime->scheduler().findNode(graph.value().scheduler));
    auto* router = dynamic_cast<MediaScheduledOutputRouterNode*>(
        runtime->scheduler().findNode(graph.value().router));
    auto* publisher = dynamic_cast<MediaDualMediaSdpPublisherNode*>(
        runtime->scheduler().findNode(graph.value().publisher));
    if (!video || !audio || !scheduler || !router || !publisher) {
        return RuntimeResult::failure(harnessError(
            "scheduled RTP decode did not receive production runtime nodes"));
    }
    auto& context = runtime->context();
    if (auto started = scheduler->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto started = router->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto started = video->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto started = audio->start(context); !started) {
        return RuntimeResult::failure(started.error());
    }
    if (auto queued = pushActivationAndCodecs(
            context, graph.value().videoSender, graph.value().audioSender,
            plan.groupKey, videoCodec, audioCodec, epoch); !queued) {
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
        std::move(runtime), std::move(clock), graph.value().scheduler,
        graph.value().router, graph.value().videoSender,
        graph.value().audioSender, separate->video.senderLead,
        separate->audio.senderLead));
}

::media::Status ScheduledRtpOutputIntegrationRuntime::sendAccessUnits(
    const ScheduledRtpDecodeSampleFixture& sample)
{
    auto* scheduler = dynamic_cast<MediaAvOutputSchedulerNode*>(
        m_runtime->scheduler().findNode(m_scheduler));
    auto* router = dynamic_cast<MediaScheduledOutputRouterNode*>(
        m_runtime->scheduler().findNode(m_router));
    auto* video = dynamic_cast<MediaScheduledRtpSenderNode*>(
        m_runtime->scheduler().findNode(m_videoSender));
    auto* audio = dynamic_cast<MediaScheduledRtpSenderNode*>(
        m_runtime->scheduler().findNode(m_audioSender));
    if (!scheduler || !router || !video || !audio || !m_clock) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP decode lost its sender nodes"));
    }
    MediaChannel* videoInput =
        m_runtime->context().findInputChannel(m_scheduler, "video");
    MediaChannel* audioInput =
        m_runtime->context().findInputChannel(m_scheduler, "audio");
    if (!videoInput || !audioInput) {
        return ::media::Status::failure(harnessError(
            "scheduled RTP decode lost its canonical scheduler inputs"));
    }
    std::uint64_t sequence = 1;
    for (const auto& unit : sample.accessUnits()) {
        const bool isVideo = unit.stream == MediaScheduledStream::Video;
        const MediaRunningTime lead = isVideo ? m_videoLead : m_audioLead;
        auto canonical = canonicalAccessUnit(unit, lead, sequence++);
        if (!canonical) return ::media::Status::failure(canonical.error());
        MediaChannel* input = isVideo ? videoInput : audioInput;
        if (!input->push(std::move(canonical).value())) {
            return ::media::Status::failure(harnessError(
                "scheduled RTP decode could not queue a canonical access unit"));
        }
    }
    videoInput->close();
    audioInput->close();

    bool videoFinished = false;
    bool audioFinished = false;
    constexpr std::size_t maximumSteps = 50'000;
    for (std::size_t step = 0; step < maximumSteps; ++step) {
        std::array<::media::Result<MediaNodeProcessResult>, 4> processed{
            scheduler->process(m_runtime->context()),
            router->process(m_runtime->context()),
            video->process(m_runtime->context()),
            audio->process(m_runtime->context())};
        for (const auto& result : processed) {
            if (!result) return ::media::Status::failure(result.error());
        }
        videoFinished = videoFinished ||
            processed[2].value().state == MediaNodeProcessState::Finished;
        audioFinished = audioFinished ||
            processed[3].value().state == MediaNodeProcessState::Finished;
        if (videoFinished && audioFinished) {
            return ::media::Status::success();
        }

        const bool progressed = std::any_of(
            processed.begin(), processed.end(), [](const auto& result) {
                return result.value().state == MediaNodeProcessState::Progress;
            });
        if (progressed) continue;
        const MediaNodeProcessResult::DeadlineWait* earliest = nullptr;
        for (const auto& result : processed) {
            if (!result.value().deadlineWait) continue;
            if (!earliest || result.value().deadlineWait->masterDeadline <
                                 earliest->masterDeadline) {
                earliest = &*result.value().deadlineWait;
            }
        }
        if (!earliest) {
            return ::media::Status::failure(harnessError(
                "scheduled RTP decode runtime stalled without a node deadline"));
        }
        if (auto waited = waitForNodeDeadline(*m_clock, *earliest); !waited) {
            return waited;
        }
    }
    return ::media::Status::failure(harnessError(
        "scheduled RTP decode runtime exceeded its process-step budget"));
}

} // namespace media_transcode::test
