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

ProjectMpegTsGenerationSessionState::
    ProjectMpegTsGenerationSessionState() = default;

ProjectMpegTsGenerationSessionState::
    ~ProjectMpegTsGenerationSessionState() = default;

void ProjectMpegTsGenerationSessionState::
    resetForGenerationPurge() noexcept
{
    if (session) session->abort();
    session.reset();
    plan.reset();
    epoch.reset();
    group.reset();
    nextTransportDeadline.reset();
    mediaTimelineStarted = false;
    generation.store(0, std::memory_order_release);
    state = State::Acquiring;
}

ProjectMpegTsMuxSessionAdapter::ProjectMpegTsMuxSessionAdapter(
    ProjectMpegTsGenerationAuthority authority)
    : m_generationState(std::move(authority.generationState))
    , m_generationSession(std::move(authority.generationSession))
    , m_state(m_generationSession->state)
    , m_plan(m_generationSession->plan)
    , m_epoch(m_generationSession->epoch)
    , m_group(m_generationSession->group)
    , m_session(m_generationSession->session)
    , m_nextTransportDeadline(
          m_generationSession->nextTransportDeadline)
    , m_mediaTimelineStarted(
          m_generationSession->mediaTimelineStarted)
    , m_generation(m_generationSession->generation)
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
    bool failed = false;
    bool acquiring = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        failed = m_failure.has_value();
        acquiring = m_state == State::Acquiring;
    }
    if (failed) return terminalStatus();
    if (dynamic_cast<MediaTsMuxRuntimePlanBuffer*>(buffer.get())) {
        return bindRuntimePlan(context, buffer);
    }
    if (!acquiring) {
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
    if (!group) {
        return fail(invalid(
            "project MPEG-TS mux runtime plan sync group is not registered"));
    }
    m_plannedGroup = runtimePlan->group();
    std::optional<::media::ErrorInfo> activationFailure;
    {
        auto permitted = m_generationState->permitActivatedGeneration(
            *group, runtimePlan->epoch().generation,
            runtimePlan->completedTransitionSequence());
        if (!permitted) return fail(permitted.error());
        if (m_epoch || m_group || m_plan || m_session) {
            activationFailure = invalid(
                "project MPEG-TS mux session rejects an uncleared runtime generation");
        } else {
            m_plan = runtimePlan->plan();
            m_epoch = runtimePlan->epoch();
            m_group = runtimePlan->group();
            m_generation.store(
                runtimePlan->epoch().generation, std::memory_order_release);
        }
    }
    if (activationFailure) return fail(std::move(*activationFailure));
    return tryActivate(context);
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindSink(
    const MediaBufferRef& buffer)
{
    bool duplicate = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        duplicate = m_sink != nullptr;
    }
    if (duplicate) {
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
    std::optional<::media::ErrorInfo> failure;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_failure) {
            failure = *m_failure;
        } else if (m_state != State::Acquiring) {
            failure = invalid(
                "project MPEG-TS mux session received a late output sink");
        } else if (m_sink) {
            failure = invalid(
                "project MPEG-TS mux session received a duplicate output sink");
        } else {
            m_sink = std::make_unique<CloseOnceOutputByteSink>(
                std::move(sink).value());
        }
    }
    if (failure) return fail(std::move(*failure));
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::bindStreamConfig(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    bool failed = false;
    bool acquiring = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        failed = m_failure.has_value();
        acquiring = m_state == State::Acquiring;
    }
    if (failed) return terminalStatus();
    if (!acquiring) {
        return fail(invalid(
            "project MPEG-TS mux session received a late stream configuration"));
    }
    auto parameters = codecParameters(buffer);
    if (!parameters) return fail(parameters.error());
    std::optional<::media::ErrorInfo> failure;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_failure) {
            failure = *m_failure;
        } else if (m_state != State::Acquiring) {
            failure = invalid(
                "project MPEG-TS mux session received a late stream configuration");
        } else {
            switch (buffer->streamKind()) {
            case MediaStreamKind::Video:
                if (m_videoConfig) {
                    failure = invalid(
                        "project MPEG-TS mux session received duplicate video configuration");
                } else {
                    m_videoConfig = std::move(parameters).value();
                }
                break;
            case MediaStreamKind::Audio:
                if (m_audioConfig) {
                    failure = invalid(
                        "project MPEG-TS mux session received duplicate audio configuration");
                } else {
                    m_audioConfig = std::move(parameters).value();
                }
                break;
            default:
                failure = invalid(
                    "project MPEG-TS mux session configuration must be video or audio");
                break;
            }
        }
    }
    if (failure) return fail(std::move(*failure));
    return tryActivate(context);
}

::media::Status ProjectMpegTsMuxSessionAdapter::tryActivate(
    MediaGraphExecutionContext& context)
{
    std::optional<MediaAvSyncGroupKey> plannedGroup;
    bool ready = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        plannedGroup = m_plannedGroup;
        ready = m_plannedGroup && m_sink && m_videoConfig && m_audioConfig;
    }
    if (!ready) {
        return ::media::Status::success();
    }
    auto group = context.findAvSyncGroup(*plannedGroup);
    if (!group) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session A/V sync group is not registered"));
    }
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) return ::media::Status::success();
    std::optional<::media::ErrorInfo> failure;
    {
        auto current = m_generationState->reserveCommit(*group, generation);
        if (!current) {
            return current.error().code == ::media::ErrorCode::Cancelled
                ? ::media::Status::success()
                : fail(current.error());
        }
        if (m_state != State::Acquiring || !m_plan || !m_epoch || !m_group ||
            !m_plannedGroup || !m_sink || !m_videoConfig || !m_audioConfig) {
            return ::media::Status::success();
        }
        if (m_epoch->generation != generation ||
            *m_group != *m_plannedGroup) {
            failure = invalid(
                "project MPEG-TS mux session differs from its current authority");
        } else {
            auto emissionOrigin =
                mediaTsTransportEmissionOrigin(*m_plan, *m_epoch);
            const auto* videoBuffer =
                dynamic_cast<const FFmpegCodecParametersBuffer*>(
                    m_videoConfig.get());
            const auto* audioBuffer =
                dynamic_cast<const FFmpegCodecParametersBuffer*>(
                    m_audioConfig.get());
            if (!emissionOrigin) {
                failure = emissionOrigin.error();
            } else {
                auto video = MediaTsFfmpegStreamConfigMaterializer::video(
                    *m_plan, *videoBuffer->parameters());
                auto audio = MediaTsFfmpegStreamConfigMaterializer::audio(
                    *m_plan, *audioBuffer->parameters());
                if (!video) {
                    failure = video.error();
                } else if (!audio) {
                    failure = audio.error();
                } else {
                    auto session = MediaTsMuxSession::create(
                        MediaTsMuxSession::Binding{
                            *m_plan, *m_epoch, std::move(video).value(),
                            std::move(audio).value(),
                            std::make_unique<GenerationBorrowedByteSink>(
                                *m_sink)});
                    if (!session) {
                        failure = session.error();
                    } else if (auto started = session.value()->start(
                                   emissionOrigin.value());
                               !started) {
                        failure = started.error();
                    } else {
                        m_session = std::move(session).value();
                        m_state = State::Active;
                    }
                }
            }
        }
    }
    if (failure) return fail(std::move(*failure));
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::validateAccessUnitLocked(
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
    const auto* accessUnit = dynamic_cast<const MediaTsAccessUnitBuffer*>(
        buffer.get());
    if (!accessUnit) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed access unit"));
    }
    auto view = accessUnit->view();
    if (!view) return fail(view.error());
    if (!m_plannedGroup) return ::media::Status::success();
    auto group = context.findAvSyncGroup(*m_plannedGroup);
    if (!group) return fail(::media::ErrorInfo::notInitialized(
        "project MPEG-TS mux session A/V sync group is not registered"));
    std::optional<::media::ErrorInfo> failure;
    {
        auto reservation =
            m_generationState->reserveCommit(
                *group, view.value().generation);
        if (!reservation) return ::media::Status::success();
        if (m_failure) {
            failure = *m_failure;
        } else if (m_state != State::Active || !m_session) {
            failure = ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session cannot write before activation");
        } else if (auto unit = validateAccessUnitLocked(buffer); !unit) {
            failure = unit.error();
        } else {
            auto written = m_session->writeAccessUnit(view.value());
            if (!written) {
                failure = written.error();
            } else {
                m_nextTransportDeadline = written.value().nextDeadline;
                m_mediaTimelineStarted = true;
            }
        }
    }
    if (failure) return fail(std::move(*failure));
    return ::media::Status::success();
}

::media::Result<MediaMuxSessionPollResult>
ProjectMpegTsMuxSessionAdapter::poll(MediaGraphExecutionContext& context)
{
    if (!m_plannedGroup) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    auto runtime = context.findAvSyncGroup(*m_plannedGroup);
    if (!runtime) {
        return ::media::Result<MediaMuxSessionPollResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session A/V sync group is not registered"));
    }
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    auto pollReserved = [&]()
        -> ::media::Result<MediaMuxSessionPollResult> {
        auto current =
            m_generationState->reserveCommit(*runtime, generation);
        if (!current) {
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {false, std::nullopt});
        }
        if (m_failure) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                *m_failure);
        }
        if (m_state == State::Acquiring) {
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {false, std::nullopt});
        }
        if (m_state != State::Active || !m_session || !m_group) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session cannot poll outside its active state"));
        }
        if (!m_mediaTimelineStarted) {
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {false, std::nullopt});
        }
        if (!m_plan || !m_nextTransportDeadline) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session has no transport poll deadline"));
        }
        auto now = runtime->clock()->now();
        if (!now) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                now.error());
        }
        auto safeDeadline = m_nextTransportDeadline->checkedAdd(
            m_plan->transportDecodeLead());
        if (!safeDeadline) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                safeDeadline.error());
        }
        if (now.value() < safeDeadline.value()) {
            return ::media::Result<MediaMuxSessionPollResult>::success({
                false,
                MediaNodeProcessResult::DeadlineWait{
                    *m_group, safeDeadline.value()}});
        }
        auto polled = m_session->poll(now.value());
        if (!polled) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                polled.error());
        }
        m_nextTransportDeadline = polled.value().nextDeadline;
        auto nextSafeDeadline = m_nextTransportDeadline->checkedAdd(
            m_plan->transportDecodeLead());
        if (!nextSafeDeadline) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                nextSafeDeadline.error());
        }
        return ::media::Result<MediaMuxSessionPollResult>::success({
            polled.value().packetsWritten != 0,
            MediaNodeProcessResult::DeadlineWait{
                *m_group, nextSafeDeadline.value()}});
    };
    auto result = pollReserved();
    if (result) return result;
    auto status = fail(result.error());
    return ::media::Result<MediaMuxSessionPollResult>::failure(
        status.error());
}

bool ProjectMpegTsMuxSessionAdapter::bindingsReady() const noexcept
{
    auto mutation = m_generationState->reserveSessionMutation();
    return m_state == State::Active && m_session && !m_failure;
}

::media::Status ProjectMpegTsMuxSessionAdapter::flush(
    MediaGraphExecutionContext& context)
{
    bool failed = false;
    bool active = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        failed = m_failure.has_value();
        active = m_state == State::Active;
    }
    if (failed) return terminalStatus();
    if (!active) {
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
    if (!m_plannedGroup) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot finish before complete binding"));
    }
    auto group = context.findAvSyncGroup(*m_plannedGroup);
    if (!group) return fail(::media::ErrorInfo::notInitialized(
        "project MPEG-TS mux session A/V sync group is not registered"));
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session has no explicit generation"));
    }
    std::optional<::media::ErrorInfo> failure;
    {
        auto current = m_generationState->reserveCommit(*group, generation);
        if (!current) {
            failure = current.error();
        } else if (m_failure) {
            failure = *m_failure;
        } else if (m_state == State::Acquiring || !m_session || !m_epoch ||
                   m_epoch->generation != generation) {
            failure = ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session cannot finish before complete binding");
        } else if (auto status = m_session->finish(); !status) {
            failure = status.error();
        } else if (auto sinkStatus = m_sink->close(); !sinkStatus) {
            failure = sinkStatus.error();
        } else {
            m_state = State::Finished;
            m_resourcesClosed = true;
        }
    }
    if (failure) return fail(std::move(*failure));
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::fail(::media::ErrorInfo error)
{
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (!m_failure) m_failure = std::move(error);
        m_state = State::Poisoned;
    }
    closeOwnedResources();
    return terminalStatus();
}

::media::Status ProjectMpegTsMuxSessionAdapter::terminalStatus() const
{
    auto mutation = m_generationState->reserveSessionMutation();
    if (!m_failure) {
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "project MPEG-TS terminal status requires a recorded failure"));
    }
    return ::media::Status::failure(*m_failure);
}

void ProjectMpegTsMuxSessionAdapter::closeOwnedResources() noexcept
{
    std::unique_ptr<MediaTsMuxSession> session;
    std::unique_ptr<MediaOutputByteSink> sink;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_resourcesClosed) return;
        m_resourcesClosed = true;
        session = std::move(m_session);
        sink = std::move(m_sink);
    }
    if (session) session->abort();
    if (sink) (void)sink->close();
}

void ProjectMpegTsMuxSessionAdapter::abort() noexcept
{
    closeOwnedResources();
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_state != State::Finished && !m_failure) {
            m_failure = ::media::ErrorInfo::cancelled(
                "project MPEG-TS mux session aborted");
            m_state = State::Poisoned;
        }
    }
}

} // namespace media::ffmpeg::graph
