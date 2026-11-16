#include "internal/graph/utils/MediaCleanupError.h"
#include "internal/graph/nodes/mux/ProjectMpegTsMuxSessionAdapter.h"

#include "internal/graph/nodes/mux/CloseOnceOutputByteSink.h"
#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/protocol/mpegts/MediaTsMuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportEmissionOrigin.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/io/MediaOutputByteSink.h"
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <algorithm>
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

} // namespace

ProjectMpegTsGenerationSessionState::
    ProjectMpegTsGenerationSessionState() = default;

ProjectMpegTsGenerationSessionState::
    ~ProjectMpegTsGenerationSessionState() = default;

void ProjectMpegTsGenerationSessionState::
    resetForGenerationPurge() noexcept
{
    // Requested-stop cancellation is a terminal owner state, not a new epoch.
    if (state == State::CancelledBeforeActivation) return;
    if (session) session->abort();
    session.reset();
    outputPlan.reset();
    transport.reset();
    prefixService.reset();
    activation.reset();
    plannedSession.reset();
    streamSet.reset();
    nextTransportDeadline.reset();
    latestAcceptedEmission.reset();
    timelineState = TimelineState::Dormant;
    generation.store(0, std::memory_order_release);
    state = State::Acquiring;
}

ProjectMpegTsGenerationAuthority::ProjectMpegTsGenerationAuthority(
    std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
    std::shared_ptr<ProjectMpegTsGenerationSessionState> generationSession,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> outputAuthority)
    : m_generationState(std::move(generationState))
    , m_generationSession(std::move(generationSession))
    , m_outputAuthority(std::move(outputAuthority))
{
}

::media::Result<ProjectMpegTsGenerationAuthority>
ProjectMpegTsGenerationAuthority::create(
    std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
    std::shared_ptr<ProjectMpegTsGenerationSessionState> generationSession,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> outputAuthority)
{
    if (!generationState || !generationSession || !outputAuthority ||
        generationState->sessionState().get() != generationSession.get()) {
        return ::media::Result<ProjectMpegTsGenerationAuthority>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS generation authority requires one identical state/session binding"));
    }
    return ::media::Result<ProjectMpegTsGenerationAuthority>::success(
        ProjectMpegTsGenerationAuthority(
            std::move(generationState), std::move(generationSession),
            std::move(outputAuthority)));
}

const std::shared_ptr<MediaProtocolOutputGenerationState>&
ProjectMpegTsGenerationAuthority::generationState() const noexcept
{
    return m_generationState;
}

const std::shared_ptr<ProjectMpegTsGenerationSessionState>&
ProjectMpegTsGenerationAuthority::generationSession() const noexcept
{
    return m_generationSession;
}

const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>&
ProjectMpegTsGenerationAuthority::outputAuthority() const noexcept
{
    return m_outputAuthority;
}

ProjectMpegTsMuxSessionAdapter::ProjectMpegTsMuxSessionAdapter(
    ProjectMpegTsGenerationAuthority authority,
    std::shared_ptr<MediaNodeWakeup> producerWakeup)
    : m_generationState(authority.generationState())
    , m_generationSession(authority.generationSession())
    , m_outputAuthority(authority.outputAuthority())
    , m_producerWakeup(std::move(producerWakeup))
    , m_plannedSession(m_generationSession->plannedSession)
    , m_streamSet(m_generationSession->streamSet)
    , m_state(m_generationSession->state)
    , m_outputPlan(m_generationSession->outputPlan)
    , m_activation(m_generationSession->activation)
    , m_session(m_generationSession->session)
    , m_nextTransportDeadline(
          m_generationSession->nextTransportDeadline)
    , m_latestAcceptedEmission(
          m_generationSession->latestAcceptedEmission)
    , m_timelineState(m_generationSession->timelineState)
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
    if (dynamic_cast<MediaProjectMpegTsRuntimePlanBuffer*>(
            buffer.get())) {
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
    const auto* runtimePlan =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            buffer.get());
    if (!runtimePlan) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed runtime plan"));
    }
    if (!m_outputAuthority ||
        runtimePlan->sessionKey() != m_outputAuthority->sessionKey() ||
        runtimePlan->streamSet() != m_outputAuthority->streamSet()) {
        return fail(invalid(
            "project MPEG-TS mux runtime plan authority is not registered"));
    }
    std::optional<::media::ErrorInfo> activationFailure;
    {
        auto permitted = m_generationState->permitActivatedGeneration(
            *m_outputAuthority, runtimePlan->activation().generation,
            runtimePlan->activation().completedTransitionSequence);
        if (!permitted) return fail(permitted.error());
        if (m_activation || m_plannedSession || m_streamSet ||
            m_outputPlan || m_session) {
            activationFailure = invalid(
                "project MPEG-TS mux session rejects an uncleared runtime generation");
        } else {
            m_plannedSession = runtimePlan->sessionKey();
            m_streamSet = runtimePlan->streamSet();
            m_outputPlan = runtimePlan->sharedOutputPlan();
            m_generationSession->transport = runtimePlan->transport();
            m_generationSession->prefixService = runtimePlan->prefixService();
            m_activation = runtimePlan->activation();
            m_generation.store(
                runtimePlan->activation().generation,
                std::memory_order_release);
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
    MediaGraphExecutionContext&)
{
    std::optional<MediaProtocolOutputSessionKey> plannedSession;
    std::optional<MediaTranscodeStreamSet> streamSet;
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan;
    bool ready = false;
    bool shapeMismatch = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        plannedSession = m_plannedSession;
        streamSet = m_streamSet;
        outputPlan = m_outputPlan;
        if (plannedSession && streamSet && outputPlan) {
            const bool videoOnlyProgram =
                outputPlan->protocol.muxPlan().videoOnlyProgram() != nullptr;
            shapeMismatch =
                (*streamSet == MediaTranscodeStreamSet::VideoOnly) !=
                    videoOnlyProgram ||
                (*streamSet == MediaTranscodeStreamSet::VideoOnly &&
                 static_cast<bool>(m_audioConfig));
            ready = m_videoConfig &&
                (*streamSet == MediaTranscodeStreamSet::VideoOnly
                     ? !m_audioConfig
                     : static_cast<bool>(m_audioConfig));
        }
    }
    if (shapeMismatch) {
        return fail(invalid(
            "project MPEG-TS mux stream set conflicts with its typed program/configuration"));
    }
    if (!ready) {
        return ::media::Status::success();
    }
    if (!m_outputAuthority || !plannedSession ||
        m_outputAuthority->sessionKey() != *plannedSession ||
        !streamSet || m_outputAuthority->streamSet() != *streamSet) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session output authority is not registered"));
    }
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) return ::media::Status::success();
    auto disposition = m_generationState->classifyGeneration(generation);
    if (!disposition) return fail(disposition.error());
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Old) {
        return ::media::Status::success();
    }
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Future) {
        return fail(invalid(
            "Project MPEG-TS activation rejects a future generation"));
    }
    std::optional<::media::ErrorInfo> failure;
    {
        auto current = m_generationState->reserveCommit(
            *m_outputAuthority, generation);
        if (!current) {
            return current.error().code == ::media::ErrorCode::Cancelled
                ? ::media::Status::success()
                : fail(current.error());
        }
        if (m_state != State::Acquiring || !m_outputPlan || !m_activation ||
            !m_streamSet || !m_plannedSession || !m_videoConfig ||
            (*m_streamSet == MediaTranscodeStreamSet::AudioVideo &&
             !m_audioConfig)) {
            return ::media::Status::success();
        }
        if (!m_generationSession->prefixService || m_activation->generation != generation ||
            *m_plannedSession != m_outputAuthority->sessionKey() ||
            *m_streamSet != m_outputAuthority->streamSet()) {
            failure = invalid(
                "project MPEG-TS mux session differs from its current authority");
        } else {
            const auto& muxPlan = m_outputPlan->protocol.muxPlan();
            auto emissionOrigin =
                mediaTsTransportEmissionOrigin(muxPlan, *m_activation);
            const auto* videoBuffer =
                dynamic_cast<const FFmpegCodecParametersBuffer*>(
                    m_videoConfig.get());
            const auto* audioBuffer = m_audioConfig
                ? dynamic_cast<const FFmpegCodecParametersBuffer*>(
                      m_audioConfig.get())
                : nullptr;
            if (!emissionOrigin) {
                failure = emissionOrigin.error();
            } else {
                auto video = MediaTsFfmpegStreamConfigMaterializer::video(
                    muxPlan, *videoBuffer->parameters());
                if (!video) {
                    failure = video.error();
                } else {
                    std::optional<MediaTsMaterializedAudioConfig> audio;
                    if (*m_streamSet ==
                        MediaTranscodeStreamSet::AudioVideo) {
                        auto materialized =
                            MediaTsFfmpegStreamConfigMaterializer::audio(
                                muxPlan, *audioBuffer->parameters());
                        if (!materialized) {
                            failure = materialized.error();
                        } else {
                            audio.emplace(std::move(materialized).value());
                        }
                    }
                    if (!failure) {
                        {
                            MediaTsMuxSession::MaterializedStreams streams =
                                MediaTsMuxSession::VideoOnlyStreams{
                                    std::move(video).value()};
                            if (audio) {
                                streams = MediaTsMuxSession::AudioVideoStreams{
                                    std::get<MediaTsMuxSession::VideoOnlyStreams>(
                                        std::move(streams)).video,
                                    std::move(*audio)};
                            }
                            auto session = MediaTsMuxSession::create(
                                MediaTsMuxSession::Binding{
                                    muxPlan, m_outputPlan->emission,
                                    *m_activation,
                                    m_outputAuthority,
                                    std::move(streams),
                                    current.value().
                                        startsAfterGenerationTransition(),
                                    m_generationSession->transport,
                                    m_producerWakeup,
                                    *m_generationSession->prefixService});
                            if (!session) {
                                failure = session.error();
                            } else if (auto started =
                                           session.value()->start(
                                               emissionOrigin.value());
                                       !started) {
                                failure = started.error();
                            } else {
                                m_session = std::move(session).value();
                                m_nextTransportDeadline =
                                    emissionOrigin.value();
                                m_timelineState =
                                    TimelineState::StartupMaintenancePending;
                                m_state = State::Active;
                            }
                        }
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
    if (!m_activation ||
        view.value().generation != m_activation->generation) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS access unit generation does not match its activation"));
    }
    auto lead = view.value().dispatchOnMaster.checkedSubtract(
        view.value().emitOnMaster);
    if (!lead || !m_outputPlan ||
        lead.value() !=
            m_outputPlan->protocol.muxPlan().transportDecodeLead()) {
        return ::media::Status::failure(invalid(
            "project MPEG-TS access unit transport decode lead does not match its plan"));
    }
    return ::media::Status::success();
}

::media::Status ProjectMpegTsMuxSessionAdapter::write(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    const auto writeEntered = mediaAccessUnitObservationNow();
    std::optional<std::int64_t> sessionReady;
    std::optional<MediaRunningTime> lateReady;
    const auto* accessUnit = dynamic_cast<const MediaTsAccessUnitBuffer*>(
        buffer.get());
    if (!accessUnit) {
        return fail(invalid(
            "project MPEG-TS mux session requires a typed access unit"));
    }
    auto view = accessUnit->view();
    if (!view) return fail(view.error());
    auto disposition =
        m_generationState->classifyGeneration(view.value().generation);
    if (!disposition) return fail(disposition.error());
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Old) {
        return ::media::Status::success();
    }
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Future) {
        return fail(invalid(
            "Project MPEG-TS mux session rejects a future access-unit generation"));
    }
    std::optional<MediaProtocolOutputSessionKey> plannedSession;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        plannedSession = m_plannedSession;
    }
    if (!plannedSession || !m_outputAuthority ||
        *plannedSession != m_outputAuthority->sessionKey()) {
        return fail(::media::ErrorInfo::notInitialized(
            "Project MPEG-TS write requires active planned generation authority"));
    }
    std::optional<::media::ErrorInfo> failure;
    {
        auto reservation =
            m_generationState->reserveCommit(
                *m_outputAuthority, view.value().generation);
        if (!reservation) {
            return reservation.error().code ==
                    ::media::ErrorCode::Cancelled
                ? ::media::Status::success()
                : fail(reservation.error());
        }
        if (m_failure) {
            failure = *m_failure;
        } else if (m_state != State::Active || !m_session) {
            failure = ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session cannot write before activation");
        } else if (m_timelineState ==
                   TimelineState::StartupMaintenancePending) {
            failure = invalid(
                "project MPEG-TS mux session requires startup maintenance before its first access unit");
        } else if (m_timelineState == TimelineState::Dormant) {
            failure = invalid(
                "project MPEG-TS mux session has no active transport timeline");
        } else if (auto unit = validateAccessUnitLocked(buffer); !unit) {
            failure = unit.error();
        } else if (m_session->hasPendingEmission()) {
            failure = invalid(
                "project MPEG-TS mux session already owns an access unit");
        } else {
            auto now = m_outputAuthority->now();
            if (!now) {
                failure = now.error();
            } else {
                sessionReady = mediaAccessUnitObservationNow();
                if (now.value() > view.value().dispatchOnMaster) lateReady = now.value();
                auto written = m_session->writeAccessUnit(
                    view.value(), now.value(), MediaAccessUnitMuxObservation{
                        accessUnit->processingTrace(), view.value().generation,
                        view.value().stream == MediaScheduledStream::Video ? "video" : "audio",
                        view.value().emitOnMaster.nanoseconds(), view.value().dispatchOnMaster.nanoseconds(),
                        now.value().nanoseconds(), writeEntered, sessionReady});
                if (!written) {
                    failure = written.error();
                } else {
                    m_nextTransportDeadline = written.value().nextDeadline;
                    m_latestAcceptedEmission = view.value().emitOnMaster;
                    m_timelineState = TimelineState::MediaTimelineActive;
                }
            }
        }
    }
    if (lateReady) {
        accessUnit->processingTrace().logMux("late_mux_input", view.value().generation,
            view.value().stream == MediaScheduledStream::Video ? "video" : "audio",
            view.value().emitOnMaster.nanoseconds(), view.value().dispatchOnMaster.nanoseconds(), lateReady->nanoseconds(),
            writeEntered, sessionReady);
    }
    if (failure && failure->code == ::media::ErrorCode::WouldBlock)
        return ::media::Status::failure(std::move(*failure));
    if (failure) return fail(std::move(*failure));
    return ::media::Status::success();
}

::media::Result<MediaMuxSessionPollResult>
ProjectMpegTsMuxSessionAdapter::poll(MediaGraphExecutionContext& context)
{
    return pollInternal(context, true);
}

::media::Result<MediaMuxSessionPollResult>
ProjectMpegTsMuxSessionAdapter::pollInternal(
    MediaGraphExecutionContext& context, bool consumeOutput)
{
    std::optional<MediaProtocolOutputSessionKey> plannedSession;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        plannedSession = m_plannedSession;
    }
    if (!plannedSession) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    if (!m_outputAuthority ||
        *plannedSession != m_outputAuthority->sessionKey()) {
        return ::media::Result<MediaMuxSessionPollResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session output authority is not registered"));
    }
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    auto disposition = m_generationState->classifyGeneration(generation);
    if (!disposition) {
        auto status = fail(disposition.error());
        return ::media::Result<MediaMuxSessionPollResult>::failure(
            status.error());
    }
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Old) {
        return ::media::Result<MediaMuxSessionPollResult>::success(
            {false, std::nullopt});
    }
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Future) {
        auto status = fail(invalid(
            "Project MPEG-TS poll rejects a future generation"));
        return ::media::Result<MediaMuxSessionPollResult>::failure(
            status.error());
    }
    auto pollReserved = [&]()
        -> ::media::Result<MediaMuxSessionPollResult> {
        auto current =
            m_generationState->reserveCommit(
                *m_outputAuthority, generation);
        if (!current) {
            return current.error().code == ::media::ErrorCode::Cancelled
                ? ::media::Result<MediaMuxSessionPollResult>::success(
                      {false, std::nullopt})
                : ::media::Result<MediaMuxSessionPollResult>::failure(
                      current.error());
        }
        if (m_failure) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                *m_failure);
        }
        if (m_state == State::Acquiring) {
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {false, std::nullopt});
        }
        if (m_state != State::Active || !m_session || !m_activation) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session cannot poll outside its active state"));
        }
        auto takeScheduledBatch = [&]()
            -> ::media::Result<MediaMuxSessionPollResult> {
            if (!consumeOutput)
                return ::media::Result<MediaMuxSessionPollResult>::success(
                    {false, std::nullopt});
            auto batch = m_session->takeScheduledBatch();
            if (!batch) {
                return ::media::Result<MediaMuxSessionPollResult>::failure(
                    batch.error());
            }
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {true, std::nullopt, std::move(batch).value()});
        };
        auto pollFailure = [&](const ::media::ErrorInfo& error)
            -> ::media::Result<MediaMuxSessionPollResult> {
            // Completed groups must drain even when the next group cannot reserve.
            if (consumeOutput && error.code == ::media::ErrorCode::WouldBlock &&
                m_session->hasScheduledBatch()) return takeScheduledBatch();
            return ::media::Result<MediaMuxSessionPollResult>::failure(error);
        };
        if (m_session->hasScheduledBatch()) return takeScheduledBatch();
        if (m_session->hasPendingEmission()) {
            auto now = m_outputAuthority->now();
            if (!now) {
                return ::media::Result<MediaMuxSessionPollResult>::failure(
                    now.error());
            }
            auto polled = m_session->poll(now.value());
            if (!polled) {
                return pollFailure(polled.error());
            }
            m_nextTransportDeadline = polled.value().nextDeadline;
            if (m_session->hasScheduledBatch()) return takeScheduledBatch();
            if (!m_session->hasPendingEmission())
                return ::media::Result<MediaMuxSessionPollResult>::success(
                    {true, std::nullopt});
            return ::media::Result<MediaMuxSessionPollResult>::success({
                polled.value().packetsWritten != 0,
                m_outputAuthority->deadlineWait(
                    polled.value().nextDeadline,
                    MediaNodeDeadlineWakePolicy::DeadlineOrCancellation)});
        }
        if (m_timelineState ==
            TimelineState::StartupMaintenancePending) {
            if (!m_nextTransportDeadline || m_latestAcceptedEmission) {
                return ::media::Result<MediaMuxSessionPollResult>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "project MPEG-TS startup maintenance has inconsistent timeline facts"));
            }
            auto now = m_outputAuthority->now();
            if (!now) {
                return ::media::Result<MediaMuxSessionPollResult>::failure(
                    now.error());
            }
            if (now.value() < *m_nextTransportDeadline) {
                return ::media::Result<MediaMuxSessionPollResult>::success({
                    false,
                    m_outputAuthority->deadlineWait(
                        *m_nextTransportDeadline,
                        MediaNodeDeadlineWakePolicy::DeadlineOrCancellation)});
            }
            auto polled = m_session->poll(now.value());
            if (!polled) {
                return pollFailure(polled.error());
            }
            m_nextTransportDeadline = polled.value().nextDeadline;
            m_timelineState = TimelineState::AwaitingFirstAccessUnit;
            return ::media::Result<MediaMuxSessionPollResult>::success({
                polled.value().packetsWritten != 0,
                std::nullopt});
        }
        if (m_timelineState == TimelineState::AwaitingFirstAccessUnit) {
            return ::media::Result<MediaMuxSessionPollResult>::success(
                {false, std::nullopt});
        }
        if (m_timelineState != TimelineState::MediaTimelineActive) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session has no pollable transport timeline"));
        }
        if (!m_outputPlan || !m_nextTransportDeadline ||
            !m_latestAcceptedEmission) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                ::media::ErrorInfo::notInitialized(
                    "project MPEG-TS mux session has no transport emission watermark"));
        }
        // PCR/PSI maintenance follows its planned transport deadline even
        // while media is unavailable; an AU watermark cannot stop that clock.
        auto now = m_outputAuthority->now();
        if (!now) {
            return ::media::Result<MediaMuxSessionPollResult>::failure(
                now.error());
        }
        // Maintenance must be produced at its planned release. Adding the
        // media transport lead here can consume its entire protocol window.
        if (now.value() < *m_nextTransportDeadline) {
            return ::media::Result<MediaMuxSessionPollResult>::success({
                false,
                m_outputAuthority->deadlineWait(
                    *m_nextTransportDeadline,
                    MediaNodeDeadlineWakePolicy::InputOrDeadline)});
        }
        auto polled = m_session->poll(now.value());
        if (!polled) {
            return pollFailure(polled.error());
        }
        m_nextTransportDeadline = polled.value().nextDeadline;

        return ::media::Result<MediaMuxSessionPollResult>::success({
            polled.value().packetsWritten != 0,
            m_outputAuthority->deadlineWait(
                *m_nextTransportDeadline,
                MediaNodeDeadlineWakePolicy::InputOrDeadline)});
    };
    auto result = pollReserved();
    if (result || result.error().code == ::media::ErrorCode::WouldBlock) return result;
    auto status = fail(result.error());
    return ::media::Result<MediaMuxSessionPollResult>::failure(
        status.error());
}

bool ProjectMpegTsMuxSessionAdapter::hasPendingOutput() const noexcept
{
    auto mutation = m_generationState->reserveSessionMutation();
    return m_state == State::Active && m_session &&
        (m_timelineState == TimelineState::StartupMaintenancePending ||
         m_session->hasPendingEmission() || m_session->hasScheduledBatch());
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
    auto polled = pollInternal(context, false);
    return polled ? ::media::Status::success()
                  : ::media::Status::failure(polled.error());
}

::media::Status ProjectMpegTsMuxSessionAdapter::finish(
    MediaGraphExecutionContext& context, MediaMuxCompletionIntent intent)
{
    std::optional<MediaProtocolOutputSessionKey> plannedSession;
    std::optional<::media::ErrorInfo> preparationFailure;
    bool cancelPreparation = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_failure) return ::media::Status::failure(*m_failure);
        if (m_state == State::Poisoned) {
            preparationFailure = invalid("Poisoned project MPEG-TS session has no recorded failure");
        } else if (m_state == State::CancelledBeforeActivation) {
            if (intent == MediaMuxCompletionIntent::RequestedStop) return ::media::Status::success();
            preparationFailure = invalid("Project MPEG-TS EOF cannot finish cancelled preparation");
        } else if (intent == MediaMuxCompletionIntent::RequestedStop && m_state == State::Acquiring) {
            // A formal generation purge aborts the retired mux and clears all
            // emission state before returning to this dormant preparation state.
            if (m_session || m_timelineState != TimelineState::Dormant ||
                m_nextTransportDeadline || m_latestAcceptedEmission) {
                preparationFailure = invalid("Project MPEG-TS preparation retains active emission state");
            } else {
                m_state = State::CancelledBeforeActivation;
                cancelPreparation = true;
            }
        }
        plannedSession = m_plannedSession;
    }
    if (preparationFailure) return fail(std::move(*preparationFailure));
    if (cancelPreparation) {
        auto closed = closeOwnedResources();
        return closed ? closed : fail(closed.error());
    }
    if (!plannedSession) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session cannot finish before complete binding"));
    }
    if (!m_outputAuthority ||
        *plannedSession != m_outputAuthority->sessionKey()) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session output authority is not registered"));
    }
    const auto generation = m_generation.load(std::memory_order_acquire);
    if (generation == 0) {
        return fail(::media::ErrorInfo::notInitialized(
            "project MPEG-TS mux session has no explicit generation"));
    }
    auto disposition = m_generationState->classifyGeneration(generation);
    if (!disposition) return fail(disposition.error());
    if (disposition.value() !=
        MediaProtocolOutputGenerationState::GenerationDisposition::Current) {
        return disposition.value() ==
                MediaProtocolOutputGenerationState::GenerationDisposition::Old
            ? ::media::Status::failure(::media::ErrorInfo::cancelled(
                  "Project MPEG-TS finish drops a closed generation"))
            : fail(invalid(
                  "Project MPEG-TS finish rejects a future generation"));
    }
    std::optional<::media::ErrorInfo> failure;
    {
        auto current = m_generationState->reserveCommit(
            *m_outputAuthority, generation);
        if (!current) {
            if (current.error().code == ::media::ErrorCode::Cancelled) {
                return ::media::Status::failure(::media::ErrorInfo::wouldBlock(
                    "Project MPEG-TS finish awaits revoked generation purge"));
            }
            failure = current.error();
        } else if (m_failure) {
            failure = *m_failure;
        } else if (m_state == State::Acquiring || !m_session ||
                   !m_activation ||
                   m_activation->generation != generation) {
            failure = ::media::ErrorInfo::notInitialized(
                "project MPEG-TS mux session cannot finish before complete binding");
        } else if (auto status = m_session->finish(); !status) {
            failure = status.error();
        } else {
            if (m_sink) {
                auto sinkStatus = m_sink->close();
                if (!sinkStatus) failure = sinkStatus.error();
            }
            if (!failure) {
                m_state = State::Finished;
                m_resourcesClosed = true;
            }
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
    (void)closeOwnedResources();
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

::media::Status ProjectMpegTsMuxSessionAdapter::closeOwnedResources() noexcept
{
    std::unique_ptr<MediaTsMuxSession> session;
    std::unique_ptr<MediaOutputByteSink> sink;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_resourcesClosed) return ::media::Status::success();
        m_resourcesClosed = true;
        session = std::move(m_session);
        sink = std::move(m_sink);
    }
    if (session) session->abort();
    return sink ? sink->close() : ::media::Status::success();
}

void ProjectMpegTsMuxSessionAdapter::abort() noexcept
{
    (void)closeOwnedResources();
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_state != State::Finished && m_state != State::CancelledBeforeActivation && !m_failure) {
            m_failure = mediaCleanupError(::media::ErrorCode::Cancelled,
                "project MPEG-TS mux session aborted", 0);
            m_state = State::Poisoned;
        }
    }
}

} // namespace media::ffmpeg::graph
