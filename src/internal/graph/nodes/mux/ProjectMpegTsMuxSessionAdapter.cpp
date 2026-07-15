#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"

#include "internal/graph/nodes/mux/CloseOnceOutputByteSink.h"
#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

} // namespace

ProjectMpegTsMuxSessionAdapter::ProjectMpegTsMuxSessionAdapter() = default;

ProjectMpegTsMuxSessionAdapter::~ProjectMpegTsMuxSessionAdapter()
{
    abort();
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindResource(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (m_failure) return terminalStatus();
    if (m_state != State::Acquiring) {
        return fail(invalid(
            "project MPEG-TS mux session received a late resource binding"));
    }
    if (dynamic_cast<MediaTsMuxRuntimePlanBuffer*>(buffer.get())) {
        return bindRuntimePlan(context, buffer);
    }
    if (dynamic_cast<MediaOutputByteSinkBuffer*>(buffer.get())) {
        auto status = bindSink(buffer);
        return status ? tryActivate(context) : status;
    }
    return fail(invalid(
        "project MPEG-TS mux session requires a runtime plan or output byte sink"));
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindRuntimePlan(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (m_plan) {
        return fail(invalid(
            "project MPEG-TS mux session received a duplicate runtime plan"));
    }
    const auto* runtimePlan = dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(
        buffer.get());
    if (!runtimePlan) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed runtime plan"));
    }
    m_plan = runtimePlan->plan();
    m_epoch = runtimePlan->epoch();
    m_group = runtimePlan->group();
    auto binding = validateExecutionBinding(context);
    if (!binding) return fail(binding.error());
    return tryActivate(context);
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindSink(
    const MediaBufferRef& buffer)
{
    if (m_sink) {
        return fail(invalid(
            "project MPEG-TS mux session received a duplicate output sink"));
    }
    auto* sinkBuffer = dynamic_cast<MediaOutputByteSinkBuffer*>(buffer.get());
    if (!sinkBuffer) {
        return fail(invalid(
            "project MPEG-TS mux session requires an output byte sink"));
    }
    auto sink = sinkBuffer->takeSink();
    if (!sink) return fail(sink.error());
    m_sink = std::make_unique<CloseOnceOutputByteSink>(
        std::move(sink).value());
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindStreamConfig(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (m_failure) return terminalStatus();
    if (m_state != State::Acquiring) {
        return fail(invalid(
            "project MPEG-TS mux session received a late stream configuration"));
    }
    const auto* parameters = dynamic_cast<const FFmpegCodecParametersBuffer*>(
        buffer.get());
    if (!parameters || !parameters->parameters()) {
        return fail(invalid(
            "project MPEG-TS mux session requires FFmpeg codec parameters"));
    }
    switch (buffer->streamKind()) {
    case MediaStreamKind::Video:
        if (m_videoConfig) {
            return fail(invalid(
                "project MPEG-TS mux session received duplicate video configuration"));
        }
        m_videoConfig = buffer;
        break;
    case MediaStreamKind::Audio:
        if (m_audioConfig) {
            return fail(invalid(
                "project MPEG-TS mux session received duplicate audio configuration"));
        }
        m_audioConfig = buffer;
        break;
    default:
        return fail(invalid(
            "project MPEG-TS mux session configuration must be video or audio"));
    }
    return tryActivate(context);
}

::media::Status ProjectMpegTsMuxSessionAdapter::tryActivate(
    MediaGraphExecutionContext& context)
{
    if (m_state != State::Acquiring || !m_plan || !m_epoch || !m_group ||
        !m_sink || !m_videoConfig || !m_audioConfig) {
        return ::media::Status::success();
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) return fail(binding.error());

    const auto* videoBuffer = dynamic_cast<const FFmpegCodecParametersBuffer*>(
        m_videoConfig.get());
    const auto* audioBuffer = dynamic_cast<const FFmpegCodecParametersBuffer*>(
        m_audioConfig.get());
    auto video = MediaTsFfmpegStreamConfigMaterializer::video(
        *m_plan, *videoBuffer->parameters());
    if (!video) return fail(video.error());
    auto audio = MediaTsFfmpegStreamConfigMaterializer::audio(
        *m_plan, *audioBuffer->parameters());
    if (!audio) return fail(audio.error());

    auto session = MediaTsMuxSession::create(MediaTsMuxSession::Binding{
        *m_plan, *m_epoch, std::move(video).value(), std::move(audio).value(),
        std::move(m_sink)});
    if (!session) return fail(session.error());
    m_session = std::move(session).value();
    auto started = m_session->start(m_epoch->masterRelease);
    if (!started) return fail(started.error());
    m_nextDeadline = m_epoch->masterRelease;
    m_videoConfig.reset();
    m_audioConfig.reset();
    m_state = State::Active;
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::validateExecutionBinding(
    MediaGraphExecutionContext& context) const
{
    if (!m_group || !m_epoch) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session has no runtime plan binding"));
    }
    auto group = context.findAvSyncGroup(*m_group);
    if (!group) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session A/V sync group is not registered"));
    }
    if (group->lifecycleState() !=
        MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session A/V sync group is not active"));
    }
    auto activeEpoch = group->playbackEpoch();
    if (!activeEpoch) return ::media::Status::failure(activeEpoch.error());
    if (activeEpoch.value() != *m_epoch) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS mux session playback epoch does not match its runtime plan"));
    }
    if (!group->clock()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session master clock is missing"));
    }
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::validateAccessUnit(
    const MediaBufferRef& buffer) const
{
    const auto* accessUnit = dynamic_cast<const MediaTsAccessUnitBuffer*>(
        buffer.get());
    if (!accessUnit) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS mux session requires a typed access unit"));
    }
    const auto view = accessUnit->view();
    if (!view) return ::media::Status::failure(view.error());
    if (!m_epoch || view.value().generation != m_epoch->generation) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS access unit generation does not match its playback epoch"));
    }
    auto lead = view.value().dispatchOnMaster.checkedSubtract(
        view.value().emitOnMaster);
    if (!lead || !m_plan || lead.value() != m_plan->transportDecodeLead()) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS access unit transport decode lead does not match its plan"));
    }
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::write(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (m_failure) return terminalStatus();
    if (m_state != State::Active || !m_session) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot write before activation"));
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) return fail(binding.error());
    auto unit = validateAccessUnit(buffer);
    if (!unit) return fail(unit.error());
    const auto* accessUnit = dynamic_cast<const MediaTsAccessUnitBuffer*>(
        buffer.get());
    auto view = accessUnit->view();
    if (!view) return fail(view.error());
    auto written = m_session->writeAccessUnit(view.value());
    if (!written) return fail(written.error());
    m_nextDeadline = written.value().nextDeadline;
    return ::media::Status::success();
}

::media::Result<MediaMuxSessionPollResult>
ProjectMpegTsMuxSessionAdapter::poll(MediaGraphExecutionContext& context)
{
    if (m_failure) {
        return ::media::Result<MediaMuxSessionPollResult>::failure(*m_failure);
    }
    if (m_state == State::Acquiring) {
        if (m_group) {
            auto binding = validateExecutionBinding(context);
            if (!binding) {
                auto status = fail(binding.error());
                return ::media::Result<MediaMuxSessionPollResult>::failure(
                    status.error());
            }
        }
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    if (m_state != State::Active || !m_session || !m_nextDeadline || !m_group) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot poll outside its active state"));
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) {
        auto status = fail(binding.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    auto group = context.findAvSyncGroup(*m_group);
    auto now = group->clock()->now();
    if (!now) {
        auto status = fail(now.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    if (now.value() < *m_nextDeadline) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, MediaNodeProcessResult::DeadlineWait{*m_group,
                                                         *m_nextDeadline}});
    }
    auto advanced = m_session->advanceThrough(now.value());
    if (!advanced) {
        auto status = fail(advanced.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    m_nextDeadline = advanced.value().nextDeadline;
    return ::media::Result<MediaMuxSessionPollResult>::success(
        {advanced.value().packetsWritten != 0,
         MediaNodeProcessResult::DeadlineWait{*m_group, *m_nextDeadline}});
}

::media::Status ProjectMpegTsMuxSessionAdapter::flush(
    MediaGraphExecutionContext& context)
{
    if (m_failure) return terminalStatus();
    if (m_state != State::Active) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot flush before activation"));
    }
    auto polled = poll(context);
    return polled ? ::media::Status::success()
                  : ::media::Status::failure(polled.error());
}

::media::Status ProjectMpegTsMuxSessionAdapter::finish(
    MediaGraphExecutionContext& context)
{
    if (m_failure) return terminalStatus();
    if (m_state == State::Acquiring || !m_session) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot finish before complete binding"));
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) return fail(binding.error());
    auto status = m_session->finish();
    if (!status) return fail(status.error());
    m_state = State::Finished;
    m_resourcesClosed = true;
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::fail(::media::ErrorInfo error)
{
    if (!m_failure) m_failure = std::move(error);
    m_state = State::Poisoned;
    closeOwnedResources();
    return terminalStatus();
}

::media::Status ProjectMpegTsMuxSessionAdapter::terminalStatus() const
{
    return ::media::Status::failure(*m_failure);
}

void ProjectMpegTsMuxSessionAdapter::closeOwnedResources() noexcept
{
    if (m_resourcesClosed) return;
    m_resourcesClosed = true;
    if (m_session) {
        m_session->abort();
    } else if (m_sink) {
        m_sink->close();
    }
}

void ProjectMpegTsMuxSessionAdapter::abort() noexcept
{
    closeOwnedResources();
    if (m_state != State::Finished && !m_failure) {
        m_failure = ::media::ErrorInfo::cancelled(
            "project MPEG-TS mux session aborted");
        m_state = State::Poisoned;
    }
}

} // namespace media::ffmpeg::graph
