#include "internal/graph/runtime/lifecycle/MediaContinuousOutputDrain.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/sync/MediaAvAggregateCoordination.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <algorithm>
#include <thread>

namespace media::ffmpeg::graph {
namespace {
bool mediaMovement(MediaPayloadKind kind) noexcept
{
    switch (kind) {
    case MediaPayloadKind::Frame:
    case MediaPayloadKind::Packet:
    case MediaPayloadKind::TsAccessUnit:
    case MediaPayloadKind::ScheduledDatagramBatch:
    case MediaPayloadKind::WireDatagramBatch:
    case MediaPayloadKind::ScheduledWireDatagramBatch:
    case MediaPayloadKind::MpegTsProtocolDatagramBatch:
        return true;
    default:
        return false;
    }
}
}
::media::Status MediaContinuousOutputDrain::finish(MediaGraphRuntime& runtime,
    const MediaRuntimeBranchDrainPlan& plan, std::chrono::milliseconds pollInterval)
{
    if (!runtime.threadedRunning() || plan.maximumProgressSilence.nanoseconds() <= 0 || pollInterval.count() <= 0)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Continuous output drain requires running workers and planned positive wait bounds"));
    const MediaAvOutputDomainRuntimeState* output = nullptr;
    for (const auto& domain : runtime.m_avDomains) {
        if (const auto* candidate = std::get_if<MediaAvOutputDomainRuntimeState>(&domain.role)) {
            if (output) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "Continuous output drain requires one registered output domain"));
            output = candidate;
        }
    }
    if (!output || !output->coordination || output->registration.processingMembers.empty())
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Continuous output drain lacks its registered coordination and members"));
    const auto& registration = output->registration;
    const auto& members = registration.processingMembers;
    const auto member = [&](MediaNodeId node) {
        return std::find(members.begin(), members.end(), node) != members.end();
    };
    if (!member(registration.activationOwner) || !member(registration.audioAggregate))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Continuous output drain registration omits a producer"));
    if (auto status = output->coordination->requestDrain(); !status) return status;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeLifecycle,
        "continuous_output_drain stage=requested");
    std::optional<std::uint64_t> lastProgress;
    auto lastProgressAt = std::chrono::steady_clock::now();
    for (;;) {
        if (const auto failure = runtime.threadedExecutor().primaryFailure())
            return ::media::Status::failure(failure->error);
        const auto drain = output->coordination->drainSnapshot();
        const auto completion = output->coordination->snapshot();
        std::uint64_t progress = static_cast<std::uint64_t>(drain.videoClosed) +
            static_cast<std::uint64_t>(drain.audioClosed);
        bool complete = true;
        for (const auto id : members) {
            auto state = runtime.threadedExecutor().nodeCompletion(id);
            if (!state) return ::media::Status::failure(state.error());
            const bool finished = state.value().finished && state.value().exited;
            progress += static_cast<std::uint64_t>(finished);
            if (id == registration.activationOwner)
                complete &= drain.videoClosed || (completion.allInputsSealed && finished);
            else if (id == registration.audioAggregate)
                complete &= drain.audioClosed || (completion.allInputsSealed && finished);
            else complete &= finished;
            if (state.value().exited && !state.value().finished)
                return ::media::Status::failure(::media::ErrorInfo::internalError(
                    "Continuous output worker exited before normal drain completion"));
        }
        // Only output-domain media movement counts. Source ingress and purge
        // activity must not keep a blocked output drain alive.
        for (const auto* channel : runtime.context().channels().channels()) {
            if (!member(channel->binding().from.nodeId) ||
                !mediaMovement(channel->binding().payloadKind)) continue;
            progress += channel->metrics().pushed.load(std::memory_order_relaxed);
            progress += channel->metrics().popped.load(std::memory_order_relaxed);
        }
        if (complete) {
            if (const auto failure = runtime.threadedExecutor().primaryFailure())
                return ::media::Status::failure(failure->error);
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeLifecycle,
                "continuous_output_drain stage=completed");
            return ::media::Status::success();
        }
        const auto now = std::chrono::steady_clock::now();
        if (!lastProgress || progress != *lastProgress) { lastProgress = progress; lastProgressAt = now; }
        else if (now - lastProgressAt >= std::chrono::nanoseconds(plan.maximumProgressSilence.nanoseconds()))
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "Continuous output drain made no media progress within its planned silence bound"));
        std::this_thread::sleep_for(pollInterval);
    }
}
} // namespace media::ffmpeg::graph
