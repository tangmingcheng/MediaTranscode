#include "application/realtime/MediaRealtimeVideoRunController.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlan.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/lifecycle/MediaRealtimeProgressTracker.h"
#include "internal/graph/runtime/lifecycle/MediaRealtimeRuntimeCompletion.h"

#include <algorithm>
#include <exception>
#include <type_traits>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

class MediaGraphRuntimeReset final {
public:
    explicit MediaGraphRuntimeReset(MediaGraphRuntime& runtime) noexcept
        : m_runtime(runtime)
    {
    }

    MediaGraphRuntimeReset(const MediaGraphRuntimeReset&) = delete;
    MediaGraphRuntimeReset& operator=(const MediaGraphRuntimeReset&) = delete;

    ~MediaGraphRuntimeReset()
    {
        m_runtime.reset();
    }

private:
    MediaGraphRuntime& m_runtime;
};

struct MediaRealtimeVideoWaitOutcome final {
    ::media::Status status;
    MediaRealtimeVideoRunEndReason endReason;
};

MediaRealtimeVideoRunOutcome failureOutcome(
    ::media::ErrorInfo error,
    MediaRealtimeVideoRunStage stage,
    MediaRealtimeVideoRunEndReason endReason =
        MediaRealtimeVideoRunEndReason::Failure)
{
    return {
        ::media::Status::failure(std::move(error)),
        stage,
        endReason,
        std::nullopt
    };
}

MediaRealtimeVideoOutputDescription outputDescription(
    const MediaProjectMpegTsRuntimeOutputPlan& output)
{
    if (const auto* rtp = std::get_if<MediaMpegTsRtpOutputPlan>(
            &output.transport)) {
        return {
            MediaRealtimeVideoOutputDescriptionKind::SessionDescriptionProtocol,
            rtp->sdp().path
        };
    }
    return {};
}

template <typename Output>
MediaRealtimeVideoOutputDescription outputDescriptionFromPlan(
    const Output& output)
{
    if constexpr (std::is_same_v<
                      Output,
                      MediaVideoOnlySeparateRtpOutputRuntimePlan> ||
                  std::is_same_v<Output, MediaSeparateRtpOutputRuntimePlan>) {
        return {
            MediaRealtimeVideoOutputDescriptionKind::
                SessionDescriptionProtocol,
            output.sdp.path
        };
    } else {
        return outputDescription(output);
    }
}

MediaRealtimeVideoOutputDescription outputDescription(
    const MediaRealtimeVideoRuntimePlan& runtimePlan)
{
    return std::visit(
        [](const auto& output) {
            return outputDescriptionFromPlan(output);
        },
        runtimePlan.outputAdapter);
}

MediaRealtimeVideoOutputDescription outputDescription(
    const MediaRealtimeAvSyncRuntimePlan& runtimePlan)
{
    return std::visit(
        [](const auto& output) {
            return outputDescriptionFromPlan(output);
        },
        runtimePlan.protocolOutput);
}

MediaRealtimeVideoOutputDescription outputDescription(
    const MediaRealtimeRuntimePlan& runtimePlan)
{
    return std::visit(
        [](const auto& selectedRuntime) {
            return outputDescription(selectedRuntime);
        },
        runtimePlan);
}

MediaRealtimeVideoPreparedReport preparedReport(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeRtpTranscodePlan& plan)
{
    const auto& selected = plan.videoPlan.selected;
    MediaRealtimeVideoPreparedReport report;
    report.inputType = plan.inputType;
    report.inputLayout = plan.inputLayout;
    report.outputLayout = plan.outputLayout;
    report.outputTransport = plan.outputTransport;
    report.streamSet = *request.parameters.execution.streamSet;
    report.selectedChain = selected.label;
    report.selectedScore = selected.score;
    report.decoderName = selected.decoder.ffmpegName;
    report.filterActive = selected.filterActive;
    report.filterName = selected.filter.filterName;
    report.encoderName = selected.encoder.ffmpegName;
    report.outputCodecName = plan.videoPlan.outputCodecName;
    report.hardwareDeviceKind = selected.encoder.deviceKind();
    report.zeroCopyPlanned = selected.zeroCopy;
    report.outputDescription = outputDescription(plan.runtime);

    if (const auto* avRuntime = std::get_if<MediaRealtimeAvSyncRuntimePlan>(
            &plan.runtime)) {
        MediaRealtimeVideoPreparedAudioReport audio;
        audio.branchMode = avRuntime->audioPipeline.branchMode;
        audio.reason = avRuntime->audioPipeline.reason;
        if (avRuntime->audioPipeline.resolvedOutput) {
            const auto& resolved = *avRuntime->audioPipeline.resolvedOutput;
            audio.resolvedOutput = MediaRealtimeVideoPreparedAudioOutput {
                resolved.codecName(),
                resolved.sampleRate(),
                resolved.channels(),
                resolved.codecFrameSamples(),
                resolved.encoderName(),
                resolved.bitrateKbps()
            };
        }
        report.audio = std::move(audio);
    }
    return report;
}

::media::Status notifyPrepared(
    const MediaRealtimeVideoRunObserver& observer,
    const MediaRealtimeVideoPreparedReport& report)
{
    if (!observer.prepared) {
        return ::media::Status::success();
    }
    try {
        observer.prepared(report);
        return ::media::Status::success();
    } catch (const std::exception& error) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            std::string("realtime prepared observer failed: ") + error.what()));
    } catch (...) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "realtime prepared observer failed with an unknown exception"));
    }
}

::media::Status notifyProgress(
    const MediaRealtimeVideoRunObserver& observer,
    const MediaGraphRuntimeReport& report)
{
    if (!observer.progress) {
        return ::media::Status::success();
    }
    try {
        observer.progress(report);
        return ::media::Status::success();
    } catch (const std::exception& error) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            std::string("realtime progress observer failed: ") + error.what()));
    } catch (...) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "realtime progress observer failed with an unknown exception"));
    }
}

MediaRealtimeVideoWaitOutcome waitForRealtimeProgress(
    MediaGraphRuntime& runtime,
    const MediaRealtimeVideoRunPolicy& policy,
    MediaRealtimeVideoRunControl& control,
    const MediaRealtimeVideoRunObserver& observer)
{
    using Clock = std::chrono::steady_clock;
    const auto startedAt = Clock::now();
    auto lastProgressAt = startedAt;
    MediaRealtimeProgressTracker progressTracker;
    const auto workerStartupGrace = std::chrono::milliseconds(
        std::min(
            policy.progressTimeout().count(),
            std::max(policy.pollInterval().count() * 2, 1000LL)));

    while (true) {
        auto lifecycleStatus = runtime.synchronizeThreadedState();
        if (!lifecycleStatus) {
            return {
                ::media::Status::failure(lifecycleStatus.error()),
                MediaRealtimeVideoRunEndReason::WorkerFailure
            };
        }
        if (control.stopRequested()) {
            return {
                ::media::Status::success(),
                MediaRealtimeVideoRunEndReason::CallerStop
            };
        }
        if (!runtime.threadedRunning()) {
            break;
        }
        if (runtime.threadedCompleted()) {
            return {
                ::media::Status::success(),
                MediaRealtimeVideoRunEndReason::SourceCompleted
            };
        }

        const MediaGraphRuntimeReport progressReport =
            MediaGraphRuntimeReporter::capture(runtime);
        auto sampleStatus = runtime.acceptanceCollector().sample(
            progressReport.metrics.encodedPacketsPushed);
        if (!sampleStatus) {
            return {
                ::media::Status::failure(sampleStatus.error()),
                MediaRealtimeVideoRunEndReason::Failure
            };
        }
        const MediaGraphRuntimeReport report =
            MediaGraphRuntimeReporter::capture(runtime);
        auto observerStatus = notifyProgress(observer, report);
        if (!observerStatus) {
            return {
                ::media::Status::failure(observerStatus.error()),
                MediaRealtimeVideoRunEndReason::Failure
            };
        }

        if (report.metrics.workerErrors > 0) {
            auto workerFailure = runtime.synchronizeThreadedState();
            if (!workerFailure) {
                return {
                    ::media::Status::failure(workerFailure.error()),
                    MediaRealtimeVideoRunEndReason::WorkerFailure
                };
            }
            return {
                ::media::Status::failure(::media::ErrorInfo::internalError(
                    "realtime runtime reported worker errors without a preserved primary failure")),
                MediaRealtimeVideoRunEndReason::WorkerFailure
            };
        }

        const auto now = Clock::now();
        if (report.metrics.activeWorkers == 0 &&
            now - startedAt >= workerStartupGrace) {
            auto terminalStatus = runtime.synchronizeThreadedState();
            if (!terminalStatus) {
                return {
                    ::media::Status::failure(terminalStatus.error()),
                    MediaRealtimeVideoRunEndReason::WorkerFailure
                };
            }
            if (runtime.threadedCompleted()) {
                return {
                    ::media::Status::success(),
                    MediaRealtimeVideoRunEndReason::SourceCompleted
                };
            }
            return {
                ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "realtime runtime has no active workers")),
                MediaRealtimeVideoRunEndReason::RuntimeStopped
            };
        }

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - startedAt);
        auto progress = progressTracker.observe(
            report.metrics.workerProgress,
            report.metrics.encodedPacketsPushed,
            elapsed);
        if (!progress) {
            return {
                ::media::Status::failure(progress.error()),
                MediaRealtimeVideoRunEndReason::Failure
            };
        }
        if (progress.value()) {
            lastProgressAt = Clock::now();
        }

        if (progressTracker.firstOutputDeadlineExpired(
                elapsed, policy.firstOutputTimeout())) {
            return {
                ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "realtime runtime produced no encoded output before startup deadline")),
                MediaRealtimeVideoRunEndReason::FirstOutputTimeout
            };
        }
        if (policy.maximumDuration() &&
            progressTracker.maximumOutputDurationExpired(
                elapsed, *policy.maximumDuration())) {
            return {
                ::media::Status::success(),
                MediaRealtimeVideoRunEndReason::MaximumDuration
            };
        }
        if (now - lastProgressAt >= policy.progressTimeout()) {
            return {
                ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "realtime runtime made no progress before timeout")),
                MediaRealtimeVideoRunEndReason::ProgressTimeout
            };
        }

        if (control.waitForStop(policy.pollInterval())) {
            return {
                ::media::Status::success(),
                MediaRealtimeVideoRunEndReason::CallerStop
            };
        }
    }

    return {
        ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "realtime runtime stopped before progress condition completed")),
        MediaRealtimeVideoRunEndReason::RuntimeStopped
    };
}

} // namespace

MediaRealtimeVideoRunPolicy::MediaRealtimeVideoRunPolicy(
    std::chrono::milliseconds progressTimeout,
    std::chrono::milliseconds firstOutputTimeout,
    std::chrono::milliseconds pollInterval,
    std::optional<std::chrono::milliseconds> maximumDuration) noexcept
    : m_progressTimeout(progressTimeout),
      m_firstOutputTimeout(firstOutputTimeout),
      m_pollInterval(pollInterval),
      m_maximumDuration(maximumDuration)
{
}

::media::Result<MediaRealtimeVideoRunPolicy>
MediaRealtimeVideoRunPolicy::create(
    std::chrono::milliseconds progressTimeout,
    std::chrono::milliseconds firstOutputTimeout,
    std::chrono::milliseconds pollInterval,
    std::optional<std::chrono::milliseconds> maximumDuration)
{
    MediaRealtimeVideoRunPolicy policy(
        progressTimeout,
        firstOutputTimeout,
        pollInterval,
        maximumDuration);
    auto status = policy.validate();
    if (!status) {
        return ::media::Result<MediaRealtimeVideoRunPolicy>::failure(
            status.error());
    }
    return ::media::Result<MediaRealtimeVideoRunPolicy>::success(
        std::move(policy));
}

::media::Status MediaRealtimeVideoRunPolicy::validate() const
{
    if (m_progressTimeout.count() <= 0 ||
        m_firstOutputTimeout.count() <= 0 ||
        m_pollInterval.count() <= 0 ||
        (m_maximumDuration && m_maximumDuration->count() <= 0)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "configured runtime duration, progress timeout, first-output timeout, and poll interval must be positive"));
    }
    return ::media::Status::success();
}

std::chrono::milliseconds
MediaRealtimeVideoRunPolicy::progressTimeout() const noexcept
{
    return m_progressTimeout;
}

std::chrono::milliseconds
MediaRealtimeVideoRunPolicy::firstOutputTimeout() const noexcept
{
    return m_firstOutputTimeout;
}

std::chrono::milliseconds
MediaRealtimeVideoRunPolicy::pollInterval() const noexcept
{
    return m_pollInterval;
}

std::optional<std::chrono::milliseconds>
MediaRealtimeVideoRunPolicy::maximumDuration() const noexcept
{
    return m_maximumDuration;
}

void MediaRealtimeVideoRunControl::requestStop() noexcept
{
    {
        std::lock_guard lock(m_waitMutex);
        m_stopRequested.store(true, std::memory_order_release);
    }
    m_waitCondition.notify_all();
}

bool MediaRealtimeVideoRunControl::stopRequested() const noexcept
{
    return m_stopRequested.load(std::memory_order_acquire);
}

bool MediaRealtimeVideoRunControl::waitForStop(
    std::chrono::milliseconds timeout)
{
    std::unique_lock lock(m_waitMutex);
    return m_waitCondition.wait_for(
        lock,
        timeout,
        [this] { return stopRequested(); });
}

MediaRealtimeVideoRunOutcome MediaRealtimeVideoRunController::run(
    const MediaRealtimeRtpTranscodeRequest& request,
    const MediaRealtimeVideoRunPolicy& policy,
    MediaRealtimeVideoRunControl& control,
    const MediaRealtimeVideoRunObserver& observer)
{
    MediaGraphRuntime runtime;
    MediaGraphRuntimeReset reset(runtime);

    auto policyStatus = policy.validate();
    if (!policyStatus) {
        return failureOutcome(
            policyStatus.error(),
            MediaRealtimeVideoRunStage::PolicyValidation);
    }
    if (control.stopRequested()) {
        return failureOutcome(
            ::media::ErrorInfo::cancelled(
                "realtime video run stop was requested before preflight"),
            MediaRealtimeVideoRunStage::StopRequested,
            MediaRealtimeVideoRunEndReason::CallerStop);
    }

    auto preflightResult = MediaRealtimeRtpTranscodePlanner::preflight(request);
    if (!preflightResult) {
        return failureOutcome(
            preflightResult.error(),
            MediaRealtimeVideoRunStage::Preflight);
    }
    MediaRealtimeTranscodePreflight preflight =
        std::move(preflightResult).value();
    const MediaRealtimeVideoPreparedReport prepared =
        preparedReport(request, preflight.plan);
    const MediaThreadingPolicy threadingPolicy = std::visit(
        [](const auto& runtimePlan) {
            return runtimePlan.threadingPolicy;
        },
        preflight.plan.runtime);
    if (control.stopRequested()) {
        return failureOutcome(
            ::media::ErrorInfo::cancelled(
                "realtime video run stop was requested after preflight"),
            MediaRealtimeVideoRunStage::StopRequested,
            MediaRealtimeVideoRunEndReason::CallerStop);
    }

    auto executableResult =
        MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
            std::move(preflight));
    if (!executableResult) {
        return failureOutcome(
            executableResult.error(),
            MediaRealtimeVideoRunStage::ExecutableGraphBuild);
    }
    MediaRealtimeExecutableGraph executable =
        std::move(executableResult).value();

    auto preparedStatus = notifyPrepared(observer, prepared);
    if (!preparedStatus) {
        return failureOutcome(
            preparedStatus.error(),
            MediaRealtimeVideoRunStage::PreparedNotification);
    }
    if (control.stopRequested()) {
        return failureOutcome(
            ::media::ErrorInfo::cancelled(
                "realtime video run stop was requested before runtime compile"),
            MediaRealtimeVideoRunStage::StopRequested,
            MediaRealtimeVideoRunEndReason::CallerStop);
    }

    runtime.setDiagnosticsEnabled(
        request.parameters.execution.diagnosticLogEnabled);
    runtime.setThreadingPolicy(threadingPolicy);
    auto compileStatus = runtime.compile(std::move(executable));
    if (!compileStatus) {
        return failureOutcome(
            compileStatus.error(),
            MediaRealtimeVideoRunStage::RuntimeCompile);
    }
    if (control.stopRequested()) {
        return failureOutcome(
            ::media::ErrorInfo::cancelled(
                "realtime video run stop was requested before runtime node registration"),
            MediaRealtimeVideoRunStage::StopRequested,
            MediaRealtimeVideoRunEndReason::CallerStop);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failureOutcome(
            registerStatus.error(),
            MediaRealtimeVideoRunStage::RuntimeNodeRegistration);
    }
    if (control.stopRequested()) {
        return failureOutcome(
            ::media::ErrorInfo::cancelled(
                "realtime video run stop was requested before runtime start"),
            MediaRealtimeVideoRunStage::StopRequested,
            MediaRealtimeVideoRunEndReason::CallerStop);
    }

    auto startStatus = runtime.startThreaded();
    if (!startStatus) {
        return failureOutcome(
            startStatus.error(),
            MediaRealtimeVideoRunStage::RuntimeStart);
    }

    const MediaRealtimeVideoWaitOutcome waitOutcome =
        waitForRealtimeProgress(runtime, policy, control, observer);
    const auto completion = MediaRealtimeRuntimeCompletion::complete(
        runtime, waitOutcome.status);
    const MediaGraphRuntimeReport finalReport =
        MediaGraphRuntimeReporter::capture(runtime);
    if (!completion.status) {
        return {
            ::media::Status::failure(completion.status.error()),
            waitOutcome.status
                ? MediaRealtimeVideoRunStage::RuntimeCompletion
                : MediaRealtimeVideoRunStage::RuntimeProgress,
            waitOutcome.endReason,
            finalReport
        };
    }
    return {
        ::media::Status::success(),
        MediaRealtimeVideoRunStage::Completed,
        waitOutcome.endReason,
        finalReport
    };
}

} // namespace media::ffmpeg::graph
