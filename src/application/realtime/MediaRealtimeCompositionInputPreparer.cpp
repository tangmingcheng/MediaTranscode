#include "application/realtime/MediaRealtimeCompositionInputPreparer.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <future>
#include <thread>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<std::vector<MediaRealtimeCompositionCapturedSource>>
MediaRealtimeCompositionInputPreparer::prepare(
    const MediaRealtimeRtpTranscodeRequest& request,
    std::span<const MediaRealtimeCompositionInputPlan> inputs,
    const std::shared_ptr<MediaMasterClock>& masterClock,
    std::stop_token cancellation,
    const MediaSharedPreparationStorage& storage)
{
    using Captured = MediaRealtimeCompositionCapturedSource;
    using SourceResult = ::media::Result<Captured>;
    using Result = ::media::Result<std::vector<Captured>>;
    if (!masterClock || !storage.root || inputs.size() < 2 || inputs.size() > 4)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Composition requires two through four prepared RTP sources"));
    for (const auto& input : inputs) {
        if (input.input.type != RealtimeInputType::RtpPort ||
            !input.input.openTimeoutMs || *input.input.openTimeoutMs <= 0 ||
            !input.input.inputLatencyMs || *input.input.inputLatencyMs <= 0 ||
            (input.members != MediaTranscodeStreamSet::VideoOnly &&
             input.members != MediaTranscodeStreamSet::AudioVideo))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "Composition source requires explicit RTP input, positive open timeout and input latency"));
    }
    std::stop_source stop;
    std::stop_callback relay(cancellation, [&] { stop.request_stop(); });
    // Declared before workers: futures and captures outlive all worker joins,
    // including partial thread-launch failure.
    std::vector<std::future<SourceResult>> pending;
    std::vector<std::jthread> workers;
    pending.reserve(inputs.size());
    workers.reserve(inputs.size());
    const auto started = std::chrono::steady_clock::now();
    try {
        for (std::size_t index = 0; index < inputs.size(); ++index) {
            const auto& input = inputs[index];
            const auto deadline = started + std::chrono::milliseconds(*input.input.openTimeoutMs);
            std::promise<SourceResult> promise;
            pending.push_back(promise.get_future());
            workers.emplace_back([&, source = &input, index, deadline,
                                  promise = std::move(promise)]() mutable {
                try {
                    auto capture = MediaRtpSourcePreflight::begin(
                        source->input, source->members, deadline, stop.get_token(), storage,
                        MediaRtpVideoPreparationIntent::CanonicalIntervals);
                    if (!capture) {
                        stop.request_stop();
                        promise.set_value(SourceResult::failure(capture.error()));
                        return;
                    }
                    auto observed = capture.value().observation();
                    if (!observed) {
                        stop.request_stop();
                        promise.set_value(SourceResult::failure(observed.error()));
                        return;
                    }
                    auto facts = MediaRealtimeCompositionSourceFactsPlanner::plan(
                        request, index, *source, observed.value());
                    if (!facts) {
                        stop.request_stop();
                        promise.set_value(SourceResult::failure(facts.error()));
                        return;
                    }
                    const MediaPreparationControl control{deadline, stop.get_token()};
                    if (auto status = control.check("composition protocol installation"); !status) {
                        stop.request_stop();
                        promise.set_value(SourceResult::failure(status.error()));
                        return;
                    }
                    // Each source transfers its raw prefix as soon as its own
                    // protocol facts are ready, before waiting for other sources.
                    auto session = capture.value().installContinuousSession(facts.value(), masterClock, stop.get_token());
                    if (!session) {
                        stop.request_stop();
                        promise.set_value(SourceResult::failure(session.error()));
                        return;
                    }
                    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                        MediaGraphDiagnosticPhase::PlannerCapability,
                        "composition_protocol_installed source=" + std::to_string(index));
                    promise.set_value(SourceResult::success(Captured{
                        std::move(capture).value(), std::move(observed).value(), deadline,
                        std::move(facts).value(), std::move(session).value(), masterClock, {}}));
                } catch (...) {
                    stop.request_stop();
                    promise.set_exception(std::current_exception());
                }
            });
        }
    } catch (...) {
        stop.request_stop();
        workers.clear();
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Composition input preparation worker allocation failed"));
    }
    std::vector<Captured> sources;
    std::optional<::media::ErrorInfo> failure;
    try {
        sources.reserve(inputs.size());
        for (auto& future : pending) {
            auto prepared = future.get();
            if (!prepared) {
                // Preserve the originating failure over cancellation generated
                // by another source's earlier error.
                if (!failure || failure->code == ::media::ErrorCode::Cancelled)
                    failure = prepared.error();
                stop.request_stop();
            } else {
                sources.push_back(std::move(prepared).value());
            }
        }
    } catch (...) {
        stop.request_stop();
        workers.clear();
        return Result::failure(::media::ErrorInfo::internalError(
            "Composition input preparation worker failed"));
    }
    workers.clear();
    if (failure) return Result::failure(std::move(*failure));
    if (cancellation.stop_requested()) return Result::failure(
        ::media::ErrorInfo::cancelled("Composition input preparation cancelled"));
    return Result::success(std::move(sources));
}

} // namespace media::ffmpeg::graph
