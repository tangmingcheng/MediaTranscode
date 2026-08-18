#include "media_transcode_beta/MediaRealtimeBetaSession.h"

#include "media_transcode_beta/MediaRealtimeBetaFixedProfile.h"
#include "media_transcode_beta/MediaRealtimeBetaRequestMapper.h"
#include "media_transcode_beta/MediaRealtimeBetaSnapshotProjector.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <utility>

namespace media::beta {
namespace {

mt_beta_error_code betaErrorCode(::media::ErrorCode code) noexcept
{
    switch (code) {
    case ::media::ErrorCode::None: return MT_BETA_ERROR_NONE;
    case ::media::ErrorCode::InvalidArgument:
        return MT_BETA_ERROR_INVALID_ARGUMENT;
    case ::media::ErrorCode::AllocationFailed:
        return MT_BETA_ERROR_ALLOCATION_FAILED;
    case ::media::ErrorCode::Unsupported:
        return MT_BETA_ERROR_UNSUPPORTED;
    case ::media::ErrorCode::HardwareUnavailable:
        return MT_BETA_ERROR_HARDWARE_UNAVAILABLE;
    case ::media::ErrorCode::IoFailure:
        return MT_BETA_ERROR_IO_FAILURE;
    case ::media::ErrorCode::FFmpegFailure:
        return MT_BETA_ERROR_FFMPEG_FAILURE;
    case ::media::ErrorCode::Cancelled:
        return MT_BETA_ERROR_CANCELLED;
    case ::media::ErrorCode::NotInitialized:
    case ::media::ErrorCode::InternalError:
    case ::media::ErrorCode::WouldBlock:
        return MT_BETA_ERROR_INTERNAL;
    }
    return MT_BETA_ERROR_INTERNAL;
}

std::int32_t betaNativeCode(int nativeCode) noexcept
{
    if constexpr (sizeof(int) <= sizeof(std::int32_t)) {
        return static_cast<std::int32_t>(nativeCode);
    }
    if (nativeCode < std::numeric_limits<std::int32_t>::min() ||
        nativeCode > std::numeric_limits<std::int32_t>::max()) {
        return 0;
    }
    return static_cast<std::int32_t>(nativeCode);
}

mt_beta_failure_stage controllerFailureStage(
    ::media::ErrorCode errorCode,
    ffmpeg::graph::MediaRealtimeVideoRunStage stage) noexcept
{
    using Stage = ffmpeg::graph::MediaRealtimeVideoRunStage;
    if (errorCode == ::media::ErrorCode::Unsupported ||
        errorCode == ::media::ErrorCode::HardwareUnavailable) {
        return MT_BETA_FAILURE_CAPABILITY;
    }
    switch (stage) {
    case Stage::PolicyValidation:
        return MT_BETA_FAILURE_SESSION_CREATION;
    case Stage::StopRequested:
        return MT_BETA_FAILURE_STOP;
    case Stage::Preflight:
        return MT_BETA_FAILURE_PREFLIGHT;
    case Stage::ExecutableGraphBuild:
        return MT_BETA_FAILURE_GRAPH_BUILD;
    case Stage::PreparedNotification:
    case Stage::RuntimeCompile:
    case Stage::RuntimeNodeRegistration:
    case Stage::RuntimeStart:
        return MT_BETA_FAILURE_RUNTIME_START;
    case Stage::RuntimeProgress:
    case Stage::RuntimeCompletion:
    case Stage::Completed:
        return MT_BETA_FAILURE_RUNTIME_EXECUTION;
    }
    return MT_BETA_FAILURE_RUNTIME_EXECUTION;
}

mt_beta_failure_stage controllerFailureStage(
    const ffmpeg::graph::MediaRealtimeVideoRunOutcome& outcome) noexcept
{
    return controllerFailureStage(
        outcome.status.error().code, outcome.stage);
}

bool isRuntimeFailureStage(
    ffmpeg::graph::MediaRealtimeVideoRunStage stage) noexcept
{
    using Stage = ffmpeg::graph::MediaRealtimeVideoRunStage;
    return stage == Stage::RuntimeProgress ||
        stage == Stage::RuntimeCompletion || stage == Stage::Completed;
}

mt_beta_completion_reason controllerFailureReason(
    ::media::ErrorCode errorCode,
    ffmpeg::graph::MediaRealtimeVideoRunStage stage,
    ffmpeg::graph::MediaRealtimeVideoRunEndReason endReason) noexcept
{
    if (errorCode == ::media::ErrorCode::Cancelled ||
        endReason ==
            ffmpeg::graph::MediaRealtimeVideoRunEndReason::CallerStop) {
        return MT_BETA_COMPLETION_REQUESTED_STOP;
    }
    if (errorCode == ::media::ErrorCode::IoFailure &&
        isRuntimeFailureStage(stage)) {
        return MT_BETA_COMPLETION_SOURCE_LOSS;
    }
    return isRuntimeFailureStage(stage)
        ? MT_BETA_COMPLETION_RUNTIME_FAILURE
        : MT_BETA_COMPLETION_STARTUP_FAILURE;
}

mt_beta_completion_reason controllerFailureReason(
    const ffmpeg::graph::MediaRealtimeVideoRunOutcome& outcome) noexcept
{
    return controllerFailureReason(
        outcome.status.error().code, outcome.stage, outcome.endReason);
}

::media::Result<mt_beta_completion_reason> successCompletionReason(
    ffmpeg::graph::MediaRealtimeVideoRunEndReason reason)
{
    using Reason = ffmpeg::graph::MediaRealtimeVideoRunEndReason;
    switch (reason) {
    case Reason::CallerStop:
        return ::media::Result<mt_beta_completion_reason>::success(
            MT_BETA_COMPLETION_REQUESTED_STOP);
    case Reason::SourceCompleted:
    case Reason::MaximumDuration:
        return ::media::Result<mt_beta_completion_reason>::success(
            MT_BETA_COMPLETION_SOURCE_COMPLETED);
    case Reason::NotStarted:
    case Reason::FirstOutputTimeout:
    case Reason::ProgressTimeout:
    case Reason::WorkerFailure:
    case Reason::RuntimeStopped:
    case Reason::Failure:
        return ::media::Result<mt_beta_completion_reason>::failure(
            ::media::ErrorInfo::internalError(
                "successful realtime controller outcome has a failure reason"));
    }
    return ::media::Result<mt_beta_completion_reason>::failure(
        ::media::ErrorInfo::internalError(
            "successful realtime controller outcome has an unknown reason"));
}

} // namespace

MediaRealtimeBetaSession::MediaRealtimeBetaSession(
    MediaRealtimeBetaOwnedConfig config) noexcept
    : m_config(std::move(config))
    , m_snapshot(MediaRealtimeBetaSnapshotProjector::initial(m_config))
{
}

MediaRealtimeBetaSession::~MediaRealtimeBetaSession() noexcept
{
    requestStop();
    if (m_eventThread.joinable()) {
        if (isCurrentThreadEventThread()) {
            std::terminate();
        }
        try {
            m_eventThread.join();
        } catch (...) {
            std::terminate();
        }
    }
    m_description.reset();
}

::media::Status MediaRealtimeBetaSession::start()
{
    bool expected = false;
    if (!m_startInvoked.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Beta realtime session has already been started"));
    }

    m_callback = m_config.eventCallback();
    m_callbackUserData = m_config.eventUserData();
    try {
        m_eventThread = std::jthread([this] { eventThreadMain(); });
        return ::media::Status::success();
    } catch (const std::bad_alloc&) {
        m_startInvoked.store(false, std::memory_order_release);
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "Beta realtime event thread allocation failed"));
    } catch (const std::exception& error) {
        m_startInvoked.store(false, std::memory_order_release);
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            std::string("Beta realtime event thread creation failed: ") +
            error.what()));
    } catch (...) {
        m_startInvoked.store(false, std::memory_order_release);
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "Beta realtime event thread creation failed"));
    }
}

void MediaRealtimeBetaSession::requestStop() noexcept
{
    bool expected = false;
    if (m_stopRequested.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        m_runControl.requestStop();
    }
}

mt_beta_realtime_snapshot MediaRealtimeBetaSession::snapshot() const noexcept
{
    std::lock_guard lock(m_snapshotMutex);
    return m_snapshot;
}

bool MediaRealtimeBetaSession::isCurrentThreadEventThread() const noexcept
{
    std::lock_guard lock(m_eventThreadIdentityMutex);
    return m_eventThreadId != std::thread::id{} &&
        m_eventThreadId == std::this_thread::get_id();
}

void MediaRealtimeBetaSession::eventThreadMain() noexcept
{
    {
        std::lock_guard lock(m_eventThreadIdentityMutex);
        m_eventThreadId = std::this_thread::get_id();
    }
    m_startedAt = std::chrono::steady_clock::now();

    try {
        runOnEventThread();
    } catch (const std::bad_alloc&) {
        if (!recordControllerFailureSignal()) {
            const auto failure = controllerEmergencyClassification(
                ::media::ErrorCode::AllocationFailed);
            recordEmergencyFailure(
                MT_BETA_ERROR_ALLOCATION_FAILED, failure.stage,
                failure.completionReason, 0,
                "Beta realtime session allocation failed");
        }
        finishFailure();
    } catch (const std::exception& error) {
        if (!recordControllerFailureSignal()) {
            const auto failure = controllerEmergencyClassification(
                ::media::ErrorCode::InternalError);
            recordEmergencyFailure(
                MT_BETA_ERROR_INTERNAL, failure.stage,
                failure.completionReason, 0, error.what());
        }
        finishFailure();
    } catch (...) {
        if (!recordControllerFailureSignal()) {
            const auto failure = controllerEmergencyClassification(
                ::media::ErrorCode::InternalError);
            recordEmergencyFailure(
                MT_BETA_ERROR_INTERNAL, failure.stage,
                failure.completionReason, 0,
                "Beta realtime session failed with an unknown exception");
        }
        finishFailure();
    }

    {
        std::lock_guard lock(m_eventThreadIdentityMutex);
        m_eventThreadId = std::thread::id{};
    }
}

void MediaRealtimeBetaSession::runOnEventThread()
{
    transitionState(MT_BETA_REALTIME_STARTING);

    auto description = MediaRealtimeBetaTemporaryDescription::create();
    if (!description) {
        recordFirstFailure({
            description.error(), MT_BETA_FAILURE_SESSION_CREATION,
            MT_BETA_COMPLETION_STARTUP_FAILURE });
        finishFailure();
        return;
    }
    m_description.emplace(std::move(description).value());

    auto request = MediaRealtimeBetaRequestMapper::map(
        m_config, *m_description);
    if (!request) {
        recordFirstFailure({
            request.error(), MT_BETA_FAILURE_SESSION_CREATION,
            MT_BETA_COMPLETION_STARTUP_FAILURE });
        finishFailure();
        return;
    }

    const auto& profile = MediaRealtimeBetaFixedProfile::current();
    auto policy = ffmpeg::graph::MediaRealtimeVideoRunPolicy::create(
        std::chrono::milliseconds(profile.progressTimeoutMs),
        std::chrono::milliseconds(profile.firstOutputTimeoutMs),
        std::chrono::milliseconds(profile.pollIntervalMs),
        std::nullopt);
    if (!policy) {
        recordFirstFailure({
            policy.error(), MT_BETA_FAILURE_SESSION_CREATION,
            MT_BETA_COMPLETION_STARTUP_FAILURE });
        finishFailure();
        return;
    }

    ffmpeg::graph::MediaRealtimeVideoRunObserver observer;
    observer.prepared = [this](const auto& report) {
        handlePrepared(report);
    };
    observer.progress = [this](const auto& report) {
        handleProgress(report);
    };
    m_phase = SessionPhase::Preflight;
    m_controllerActive = true;
    const auto outcome = ffmpeg::graph::MediaRealtimeVideoRunController::run(
        request.value(), policy.value(), m_runControl, observer);
    m_controllerActive = false;
    handleOutcome(outcome);
}

void MediaRealtimeBetaSession::handlePrepared(
    const ffmpeg::graph::MediaRealtimeVideoPreparedReport& report)
{
    m_phase = SessionPhase::RuntimeStart;
    if (!m_description ||
        report.outputDescription.kind !=
            ffmpeg::graph::MediaRealtimeVideoOutputDescriptionKind::
                SessionDescriptionProtocol ||
        report.outputDescription.path != m_description->path()) {
        recordFirstFailure({
            ::media::ErrorInfo::invalidArgument(
                "selected output description does not match the session-owned path"),
            MT_BETA_FAILURE_RUNTIME_START,
            MT_BETA_COMPLETION_STARTUP_FAILURE });
        requestStop();
        return;
    }
    m_outputDescriptionPath = report.outputDescription.path;

    auto projected = snapshot();
    auto status = MediaRealtimeBetaSnapshotProjector::projectPrepared(
        projected, report);
    if (!status) {
        recordFirstFailure({
            status.error(), MT_BETA_FAILURE_CAPABILITY,
            MT_BETA_COMPLETION_STARTUP_FAILURE });
        requestStop();
        return;
    }
    {
        std::lock_guard lock(m_snapshotMutex);
        m_snapshot = projected;
    }
}

void MediaRealtimeBetaSession::handleProgress(
    const ffmpeg::graph::MediaGraphRuntimeReport& report)
{
    m_phase = SessionPhase::RuntimeExecution;
    if (hasRecordedFailure()) {
        requestStop();
        return;
    }

    auto projected = snapshot();
    auto status = MediaRealtimeBetaSnapshotProjector::projectRuntime(
        projected, report, runningTime());
    if (!status) {
        recordFirstFailure({
            status.error(), MT_BETA_FAILURE_RUNTIME_EXECUTION,
            MT_BETA_COMPLETION_RUNTIME_FAILURE });
        requestStop();
        return;
    }
    {
        std::lock_guard lock(m_snapshotMutex);
        m_snapshot = projected;
    }

    if (!m_runningStateEmitted) {
        m_runningStateEmitted = true;
        transitionState(MT_BETA_REALTIME_RUNNING);
    }
    auto output = publishOutputDescription(false);
    if (!output) {
        recordFirstFailure({
            output.error(), MT_BETA_FAILURE_RUNTIME_EXECUTION,
            MT_BETA_COMPLETION_RUNTIME_FAILURE });
        requestStop();
    }
}

::media::Status MediaRealtimeBetaSession::publishOutputDescription(
    bool finalAttempt)
{
    if (m_outputReady) {
        return ::media::Status::success();
    }
    if (!m_description || m_outputDescriptionPath.empty()) {
        return finalAttempt
            ? ::media::Status::failure(::media::ErrorInfo::ioFailure(
                  "controller did not provide a session-owned output description"))
            : ::media::Status::success();
    }

    auto text = m_description->readCompletedText();
    if (!text) {
        if (text.error().code == ::media::ErrorCode::WouldBlock &&
            !finalAttempt) {
            return ::media::Status::success();
        }
        if (text.error().code == ::media::ErrorCode::WouldBlock) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "Beta output description was not completed"));
        }
        return ::media::Status::failure(text.error());
    }
    m_outputDescription = std::move(text).value();
    m_outputReady = true;
    emitOutputReady();
    return ::media::Status::success();
}

void MediaRealtimeBetaSession::handleOutcome(
    const ffmpeg::graph::MediaRealtimeVideoRunOutcome& outcome)
{
    if (!outcome.status) {
        recordFirstFailure({
            outcome.status.error(), controllerFailureStage(outcome),
            controllerFailureReason(outcome) });
    }

    const auto* report = outcome.finalReport
        ? &*outcome.finalReport
        : outcome.failureReport ? &*outcome.failureReport : nullptr;
    if (report) {
        auto projected = snapshot();
        auto status = MediaRealtimeBetaSnapshotProjector::projectRuntime(
            projected, *report, runningTime());
        if (status) {
            std::lock_guard lock(m_snapshotMutex);
            m_snapshot = projected;
        } else {
            recordFirstFailure({
                status.error(), MT_BETA_FAILURE_RUNTIME_EXECUTION,
                MT_BETA_COMPLETION_RUNTIME_FAILURE });
        }
    }

    if (!hasRecordedFailure()) {
        auto output = publishOutputDescription(true);
        if (!output) {
            recordFirstFailure({
                output.error(), MT_BETA_FAILURE_RUNTIME_EXECUTION,
                MT_BETA_COMPLETION_RUNTIME_FAILURE });
        }
    }
    if (hasRecordedFailure()) {
        finishFailure();
        return;
    }

    auto completion = successCompletionReason(outcome.endReason);
    if (!completion) {
        recordFirstFailure({
            completion.error(), MT_BETA_FAILURE_RUNTIME_EXECUTION,
            MT_BETA_COMPLETION_RUNTIME_FAILURE });
        finishFailure();
        return;
    }
    finishSuccess(completion.value());
}

void MediaRealtimeBetaSession::recordFirstFailure(SessionFailure failure)
{
    if (!m_firstFailure && !m_hasEmergencyFailure) {
        m_firstFailure.emplace(std::move(failure));
    }
}

void MediaRealtimeBetaSession::recordEmergencyFailure(
    mt_beta_error_code errorCode,
    mt_beta_failure_stage stage,
    mt_beta_completion_reason completionReason,
    std::int32_t nativeCode,
    const char* detail) noexcept
{
    if (m_firstFailure || m_hasEmergencyFailure || m_terminalStateEmitted) {
        return;
    }
    m_emergencyFailure = {
        errorCode, stage, completionReason, nativeCode };
    const char* source = detail != nullptr
        ? detail
        : "Beta realtime session failed without diagnostic detail";
    std::size_t index = 0U;
    while (index + 1U < m_emergencyDetail.size() && source[index] != '\0') {
        m_emergencyDetail[index] = source[index];
        ++index;
    }
    m_emergencyDetail[index] = '\0';
    m_hasEmergencyFailure = true;
}

bool MediaRealtimeBetaSession::recordControllerFailureSignal() noexcept
{
    const auto signal = m_runControl.firstFailureSignal();
    if (!signal) {
        return false;
    }
    recordEmergencyFailure(
        betaErrorCode(signal->errorCode),
        controllerFailureStage(signal->errorCode, signal->stage),
        controllerFailureReason(
            signal->errorCode, signal->stage, signal->endReason),
        betaNativeCode(signal->nativeCode),
        "Beta realtime controller preserved the first operational failure");
    return true;
}

MediaRealtimeBetaSession::PhaseFailureClassification
MediaRealtimeBetaSession::controllerEmergencyClassification(
    ::media::ErrorCode errorCode) const noexcept
{
    if (!m_controllerActive) {
        return currentFailureClassification();
    }
    const auto stage = m_runControl.activeStage();
    return {
        controllerFailureStage(errorCode, stage),
        controllerFailureReason(
            errorCode, stage,
            ffmpeg::graph::MediaRealtimeVideoRunEndReason::Failure) };
}

bool MediaRealtimeBetaSession::hasRecordedFailure() const noexcept
{
    return m_firstFailure.has_value() || m_hasEmergencyFailure;
}

void MediaRealtimeBetaSession::finishFailure() noexcept
{
    if (!hasRecordedFailure() || m_terminalStateEmitted) {
        return;
    }

    const mt_beta_completion_reason completionReason = m_firstFailure
        ? m_firstFailure->completionReason
        : m_emergencyFailure.completionReason;
    {
        std::lock_guard lock(m_snapshotMutex);
        m_snapshot.completion_reason = completionReason;
        m_snapshot.state = MT_BETA_REALTIME_FAILED;
    }
    m_phase = SessionPhase::Terminal;
    m_terminalStateEmitted = true;

    if (m_firstFailure) {
        emitError(*m_firstFailure);
    } else {
        emitEmergencyError(m_emergencyFailure);
    }
    emitState(MT_BETA_REALTIME_FAILED);
    emitCompleted(completionReason);
}

void MediaRealtimeBetaSession::finishSuccess(
    mt_beta_completion_reason completionReason)
{
    if (m_terminalStateEmitted) {
        return;
    }
    m_phase = SessionPhase::Stopping;
    transitionState(MT_BETA_REALTIME_STOPPING);
    if (hasRecordedFailure()) {
        finishFailure();
        return;
    }
    setCompletionReason(completionReason);
    transitionState(MT_BETA_REALTIME_COMPLETED);
    m_phase = SessionPhase::Terminal;
    emitCompleted(completionReason);
}

void MediaRealtimeBetaSession::transitionState(mt_beta_realtime_state state)
{
    if (m_terminalStateEmitted) {
        return;
    }
    {
        std::lock_guard lock(m_snapshotMutex);
        m_snapshot.state = state;
    }
    if (state == MT_BETA_REALTIME_COMPLETED ||
        state == MT_BETA_REALTIME_FAILED) {
        m_terminalStateEmitted = true;
    }
    emitState(state);
}

void MediaRealtimeBetaSession::setCompletionReason(
    mt_beta_completion_reason completionReason)
{
    std::lock_guard lock(m_snapshotMutex);
    m_snapshot.completion_reason = completionReason;
}

void MediaRealtimeBetaSession::invokeCallback(
    const mt_beta_realtime_event& event) noexcept
{
    if (m_callback == nullptr) {
        return;
    }
    try {
        m_callback(m_callbackUserData, &event);
    } catch (...) {
        m_callback = nullptr;
        const auto failure = currentFailureClassification();
        recordEmergencyFailure(
            MT_BETA_ERROR_INTERNAL, failure.stage,
            failure.completionReason, 0,
            "Beta event callback raised an exception");
        requestStop();
    }
}

void MediaRealtimeBetaSession::emitState(mt_beta_realtime_state state) noexcept
{
    mt_beta_realtime_event event{};
    event.type = MT_BETA_EVENT_STATE_CHANGED;
    event.state = state;
    invokeCallback(event);
}

void MediaRealtimeBetaSession::emitOutputReady() noexcept
{
    mt_beta_realtime_event event{};
    event.type = MT_BETA_EVENT_OUTPUT_READY;
    event.output_description = m_outputDescription.c_str();
    invokeCallback(event);
}

void MediaRealtimeBetaSession::emitError(
    const SessionFailure& failure) noexcept
{
    mt_beta_realtime_event event{};
    event.type = MT_BETA_EVENT_ERROR;
    event.error_code = betaErrorCode(failure.error.code);
    event.failure_stage = failure.stage;
    event.native_code = betaNativeCode(failure.error.nativeCode);
    event.detail = failure.error.message.c_str();
    invokeCallback(event);
}

void MediaRealtimeBetaSession::emitEmergencyError(
    const EmergencyFailure& failure) noexcept
{
    mt_beta_realtime_event event{};
    event.type = MT_BETA_EVENT_ERROR;
    event.error_code = failure.errorCode;
    event.failure_stage = failure.stage;
    event.native_code = failure.nativeCode;
    event.detail = m_emergencyDetail.data();
    invokeCallback(event);
}

void MediaRealtimeBetaSession::emitCompleted(
    mt_beta_completion_reason completionReason) noexcept
{
    mt_beta_realtime_event event{};
    event.type = MT_BETA_EVENT_COMPLETED;
    event.completion_reason = completionReason;
    invokeCallback(event);
}

MediaRealtimeBetaSession::PhaseFailureClassification
MediaRealtimeBetaSession::currentFailureClassification() const noexcept
{
    switch (m_phase) {
    case SessionPhase::SessionCreation:
        return {
            MT_BETA_FAILURE_SESSION_CREATION,
            MT_BETA_COMPLETION_STARTUP_FAILURE };
    case SessionPhase::Preflight:
        return {
            MT_BETA_FAILURE_PREFLIGHT,
            MT_BETA_COMPLETION_STARTUP_FAILURE };
    case SessionPhase::RuntimeStart:
        return {
            MT_BETA_FAILURE_RUNTIME_START,
            MT_BETA_COMPLETION_STARTUP_FAILURE };
    case SessionPhase::RuntimeExecution:
        return {
            MT_BETA_FAILURE_RUNTIME_EXECUTION,
            MT_BETA_COMPLETION_RUNTIME_FAILURE };
    case SessionPhase::Stopping:
        return {
            MT_BETA_FAILURE_STOP,
            MT_BETA_COMPLETION_RUNTIME_FAILURE };
    case SessionPhase::Terminal:
        return {
            MT_BETA_FAILURE_RUNTIME_EXECUTION,
            MT_BETA_COMPLETION_RUNTIME_FAILURE };
    }
    return {
        MT_BETA_FAILURE_RUNTIME_EXECUTION,
        MT_BETA_COMPLETION_RUNTIME_FAILURE };
}

std::chrono::milliseconds MediaRealtimeBetaSession::runningTime() const noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_startedAt);
}

} // namespace media::beta
