#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"

#include "internal/graph/nodes/mux/CloseOnceOutputByteSink.h"
#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportEmissionOrigin.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

::media::Result<MediaBufferRef> codecParameters(const MediaBufferRef& buffer)
{
    if (const auto* parameters =
            dynamic_cast<const FFmpegCodecParametersBuffer*>(buffer.get());
        parameters && parameters->parameters()) {
        return ::media::Result<MediaBufferRef>::success(buffer);
    }
    const auto* context = dynamic_cast<const FFmpegCodecContextBuffer*>(
        buffer.get());
    if (!context || !context->context()) {
        return ::media::Result<MediaBufferRef>::failure(invalid(
            "project MPEG-TS mux session requires FFmpeg codec context or parameters"));
    }
    auto parameters = FFmpegCodecParametersMaterializer::fromContext(
        *context->context());
    if (!parameters) {
        return ::media::Result<MediaBufferRef>::failure(
            parameters.error());
    }
    auto materialized = makeMediaBufferRef<FFmpegCodecParametersBuffer>(
        std::move(parameters).value());
    materialized->setStreamKind(buffer->streamKind());
    materialized->setPayloadKind(MediaPayloadKind::CodecParameters);
    return ::media::Result<MediaBufferRef>::success(std::move(materialized));
}

class GenerationBorrowedByteSink final : public MediaOutputByteSink {
public:
    explicit GenerationBorrowedByteSink(MediaOutputByteSink& sink)
        : m_sink(sink)
    {
    }

    ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) override
    {
        return m_sink.write(bytes);
    }

    ::media::Status flush() override { return m_sink.flush(); }
    ::media::Status close() override { return ::media::Status::success(); }

private:
    MediaOutputByteSink& m_sink;
};

} // namespace

ProjectMpegTsMuxSessionAdapter::ProjectMpegTsMuxSessionAdapter(
    std::shared_ptr<MediaProtocolOutputGenerationState> generationState)
    : m_generationState(std::move(generationState))
{
}

ProjectMpegTsMuxSessionAdapter::~ProjectMpegTsMuxSessionAdapter()
{
    abort();
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
ProjectMpegTsMuxSessionAdapter::generationPurgeTarget() const noexcept
{
    return m_generationState;
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindResource(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (m_failure) return terminalStatus();
    if (dynamic_cast<MediaTsMuxRuntimePlanBuffer*>(buffer.get())) {
        return bindRuntimePlan(context, buffer);
    }
    if (m_state != State::Acquiring) {
        return fail(invalid(
            "project MPEG-TS mux session received a late resource binding"));
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
    const auto* runtimePlan = dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(
        buffer.get());
    if (!runtimePlan) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed runtime plan"));
    }
    auto group = context.findAvSyncGroup(runtimePlan->group());
    const auto snapshot = group
        ? group->epochTransitionSnapshot()
        : MediaAvEpochTransitionSnapshot{};
    if (!group || snapshot.poisoned || !snapshot.outputPermitted ||
        !snapshot.playbackEpoch ||
        *snapshot.playbackEpoch != runtimePlan->epoch()) {
        return fail(invalid(
            "project MPEG-TS mux runtime plan is not the active permitted generation"));
    }
    if (m_epoch) {
        if (!m_group || *m_group != runtimePlan->group() ||
            runtimePlan->epoch().generation <= m_epoch->generation) {
            return fail(invalid(
                "project MPEG-TS mux session rejects duplicate or stale runtime plan"));
        }
        discardGenerationSession();
    }
    m_plan = runtimePlan->plan();
    m_epoch = runtimePlan->epoch();
    m_group = runtimePlan->group();
    auto permitted = permitRuntimePlanGeneration(m_epoch->generation);
    if (!permitted) return fail(permitted.error());
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
    auto parameters = codecParameters(buffer);
    if (!parameters) return fail(parameters.error());
    switch (buffer->streamKind()) {
    case MediaStreamKind::Video:
        if (m_videoConfig) {
            return fail(invalid(
                "project MPEG-TS mux session received duplicate video configuration"));
        }
        m_videoConfig = std::move(parameters).value();
        break;
    case MediaStreamKind::Audio:
        if (m_audioConfig) {
            return fail(invalid(
                "project MPEG-TS mux session received duplicate audio configuration"));
        }
        m_audioConfig = std::move(parameters).value();
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
    auto emissionOrigin = mediaTsTransportEmissionOrigin(*m_plan, *m_epoch);
    if (!emissionOrigin) return fail(emissionOrigin.error());

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
        std::make_unique<GenerationBorrowedByteSink>(*m_sink)});
    if (!session) return fail(session.error());
    m_session = std::move(session).value();
    auto reservation =
        m_generationState->reserveCommit(m_epoch->generation);
    if (!reservation) return fail(reservation.error());
    auto started = m_session->start(emissionOrigin.value());
    if (!started) return fail(started.error());
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
    if (!m_generationState) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux generation state is missing"));
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
    const auto* accessUnit = dynamic_cast<const MediaTsAccessUnitBuffer*>(
        buffer.get());
    if (!accessUnit) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed access unit"));
    }
    auto view = accessUnit->view();
    if (!view) return fail(view.error());
    if (!outputPermitted(context)) {
        if (m_epoch &&
            view.value().generation <= m_epoch->generation) {
            return ::media::Status::success();
        }
        return fail(invalid(
            "project MPEG-TS mux rejects an unpermitted future generation"));
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) return fail(binding.error());
    auto unit = validateAccessUnit(buffer);
    if (!unit) return fail(unit.error());
    auto reservation =
        m_generationState->reserveCommit(view.value().generation);
    if (!reservation) return ::media::Status::success();
    auto written = m_session->writeAccessUnit(view.value());
    if (!written) return fail(written.error());
    m_nextTransportDeadline = written.value().nextDeadline;
    m_mediaTimelineStarted = true;
    return ::media::Status::success();
}

::media::Result<MediaMuxSessionPollResult>
ProjectMpegTsMuxSessionAdapter::poll(MediaGraphExecutionContext& context)
{
    if (m_failure) {
        return ::media::Result<MediaMuxSessionPollResult>::failure(*m_failure);
    }
    if (m_state == State::Active && !outputPermitted(context)) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
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
    if (m_state != State::Active || !m_session || !m_group) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot poll outside its active state"));
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    auto binding = validateExecutionBinding(context);
    if (!binding) {
        auto status = fail(binding.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    if (!m_mediaTimelineStarted) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    if (!m_plan || !m_nextTransportDeadline) {
        auto status = fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session has no transport poll deadline"));
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    auto runtime = context.findAvSyncGroup(*m_group);
    auto now = runtime->clock()->now();
    if (!now) {
        auto status = fail(now.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    auto safeDeadline = m_nextTransportDeadline->checkedAdd(
        m_plan->transportDecodeLead());
    if (!safeDeadline) {
        auto status = fail(safeDeadline.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    if (now.value() < safeDeadline.value()) {
        return ::media::Result<MediaMuxSessionPollResult>::success({
            false,
            MediaNodeProcessResult::DeadlineWait{
                *m_group, safeDeadline.value()}});
    }
    auto reservation =
        m_generationState->reserveCommit(m_epoch->generation);
    if (!reservation) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    auto polled = m_session->poll(now.value());
    if (!polled) {
        auto status = fail(polled.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    m_nextTransportDeadline = polled.value().nextDeadline;
    auto nextSafeDeadline = m_nextTransportDeadline->checkedAdd(
        m_plan->transportDecodeLead());
    if (!nextSafeDeadline) {
        auto status = fail(nextSafeDeadline.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(status.error());
    }
    return ::media::Result<MediaMuxSessionPollResult>::success({
        polled.value().packetsWritten != 0,
        MediaNodeProcessResult::DeadlineWait{
            *m_group, nextSafeDeadline.value()}});
}

bool ProjectMpegTsMuxSessionAdapter::bindingsReady() const noexcept
{
    return m_state == State::Active && m_session && !m_failure;
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
    auto reservation =
        m_generationState->reserveCommit(m_epoch->generation);
    if (!reservation) return fail(reservation.error());
    auto status = m_session->finish();
    if (!status) return fail(status.error());
    auto sinkStatus = m_sink->close();
    if (!sinkStatus) return fail(sinkStatus.error());
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
    }
    if (m_sink) (void)m_sink->close();
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

::media::Status
ProjectMpegTsMuxSessionAdapter::permitRuntimePlanGeneration(
    std::uint64_t generation)
{
    if (!m_generationState) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux generation state is missing"));
    }
    return m_generationState->permitActivatedGeneration(generation);
}

bool ProjectMpegTsMuxSessionAdapter::outputPermitted(
    MediaGraphExecutionContext& context) const noexcept
{
    if (!m_group || !m_epoch || !m_generationState) {
        return false;
    }
    auto group = context.findAvSyncGroup(*m_group);
    if (!group) return false;
    const auto snapshot = group->epochTransitionSnapshot();
    return !snapshot.poisoned && snapshot.outputPermitted &&
           snapshot.playbackEpoch &&
           snapshot.playbackEpoch->generation == m_epoch->generation;
}

void ProjectMpegTsMuxSessionAdapter::discardGenerationSession() noexcept
{
    if (m_session) m_session->abort();
    m_session.reset();
    m_plan.reset();
    m_epoch.reset();
    m_group.reset();
    m_nextTransportDeadline.reset();
    m_mediaTimelineStarted = false;
    m_state = State::Acquiring;
}

} // namespace media::ffmpeg::graph
