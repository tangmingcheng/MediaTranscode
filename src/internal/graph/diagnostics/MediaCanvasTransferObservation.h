#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/diagnostics/MediaCurrentThreadCpuClock.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include <array>
#include <limits>
#include <optional>
#include <sstream>
#include <type_traits>
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

// Single adapter owner; bounded observations, never a scheduling input.
class MediaCanvasTransferObservation final {
public:
    enum class Stage { ContextPush, SourceWait, PlaneCopy, DestinationWait, ContextPop, BatchCompletion, Count };
    enum class Operation { Upload, Download, Copy };

    template<class Function>
    auto measure(Stage stage, Operation operation, Function&& function)
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return function();
        const auto begin = mediaSteadyClockNowNs();
        const auto cpuBegin = MediaCurrentThreadCpuClock::nowNanoseconds();
        auto result = function();
        const auto cpuEnd = MediaCurrentThreadCpuClock::nowNanoseconds();
        const auto end = mediaSteadyClockNowNs();
        if (begin >= 0 && end >= begin) {
            auto& maximum = m_stages[static_cast<std::size_t>(stage)];
            if (maximum.count != std::numeric_limits<std::uint64_t>::max()) ++maximum.count;
            if (!maximum.record || end - begin > maximum.record->end - maximum.record->begin) {
                Record record{begin, end, operation, 0, std::nullopt, std::nullopt};
                if constexpr (std::is_same_v<decltype(result), ::media::Status>) {
                    if (!result) {
                        record.result = result.error().nativeCode;
                        record.statusError = static_cast<int>(result.error().code);
                    }
                } else {
                    record.result = static_cast<int>(result);
                }
                if (cpuBegin && cpuEnd && cpuEnd.value() >= cpuBegin.value())
                    record.cpu = cpuEnd.value() - cpuBegin.value();
                maximum.record = record;
            }
        }
        return result;
    }

    void log() const noexcept
    {
        static constexpr std::array names{
            "context_push", "source_wait", "plane_copy", "destination_wait", "context_pop", "batch_completion"};
        static_assert(names.size() == static_cast<std::size_t>(Stage::Count));
        try {
            for (std::size_t i = 0; i < names.size(); ++i) {
                const auto& maximum = m_stages[i];
                if (!maximum.record) continue;
                const auto& record = *maximum.record;
                std::ostringstream message;
                message << "cuda_canvas_call stage=" << names[i] << " count=" << maximum.count
                        << " operation=" << (record.operation == Operation::Upload ? "upload" :
                            record.operation == Operation::Download ? "download" : "copy")
                        << " maximum_ns=" << record.end - record.begin
                        << " begin_steady_ns=" << record.begin << " end_steady_ns=" << record.end
                        << " native_result=" << record.result;
                if (record.statusError) message << " status_error=" << *record.statusError;
                message << " thread_cpu_ns=";
                if (record.cpu) message << *record.cpu;
                else message << "unavailable";
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::RuntimeNode, message.str());
            }
        } catch (...) {
            // Diagnostic formatting must not replace media failure or shutdown.
        }
    }

private:
    struct Record {
        std::int64_t begin;
        std::int64_t end;
        Operation operation;
        int result;
        std::optional<std::uint64_t> cpu;
        std::optional<int> statusError;
    };
    struct Maximum {
        std::optional<Record> record;
        std::uint64_t count = 0;
    };
    std::array<Maximum, static_cast<std::size_t>(Stage::Count)> m_stages;
};

} // namespace media::ffmpeg::graph
