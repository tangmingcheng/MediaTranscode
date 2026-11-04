#pragma once
#include "internal/graph/diagnostics/MediaVideoReadinessObservation.h"
#include "internal/graph/diagnostics/MediaCanvasBatchTiming.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/diagnostics/MediaCurrentThreadCpuClock.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include <array>
#include <optional>
#include <sstream>
#include <type_traits>

namespace media::ffmpeg::graph {

// Owner-thread diagnostic only. Nested driver intervals overlap copy intervals.
class MediaCanvasComposeObservation final {
public:
    enum class Stage { Surface, Validate, Black, Tile, Publish,
        ContextPush, SourceWait, PlaneCopy, DestinationWait, ContextPop, BatchCompletion, Count };
    struct Sample {
        std::uint64_t count = 0;
        std::int64_t total = 0;
        std::int64_t maximum = 0;
        std::optional<std::size_t> tile;
        std::optional<int> result;
    };
    struct ReadinessSample {
        std::uint64_t count = 0;
        std::int64_t begin = 0;
        std::int64_t end = 0;
        std::optional<std::size_t> tile;
        std::optional<MediaVideoReadinessObservation> result;
    };
    struct Record {
        std::int64_t begin = 0;
        std::int64_t end = 0;
        std::optional<std::uint64_t> cpu;
        std::optional<MediaCanvasBatchTiming> batchTiming;
        std::array<Sample, static_cast<std::size_t>(Stage::Count)> stages{};
        std::array<ReadinessSample, static_cast<std::size_t>(MediaVideoReadinessObservation::State::Count)> readiness{};
    };
    class Scope final {
    public:
        explicit Scope(MediaCanvasComposeObservation& owner) noexcept
            : owner_(owner), previous_(active_), enabled_(mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State))
        {
            if (!enabled_) return;
            record_.begin = mediaSteadyClockNowNs();
            const auto cpu = MediaCurrentThreadCpuClock::nowNanoseconds();
            if (cpu) cpuBegin_ = cpu.value();
            active_ = this;
        }
        ~Scope()
        {
            if (!enabled_) return;
            record_.end = mediaSteadyClockNowNs();
            const auto cpuEnd = MediaCurrentThreadCpuClock::nowNanoseconds();
            if (cpuBegin_ && cpuEnd && cpuEnd.value() >= *cpuBegin_) record_.cpu = cpuEnd.value() - *cpuBegin_;
            active_ = previous_;
            if (!owner_.maximum_ || record_.end - record_.begin > owner_.maximum_->end - owner_.maximum_->begin)
                owner_.maximum_ = record_;
        }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        void tile(std::optional<std::size_t> value) noexcept { tile_ = value; }
    private:
        friend class MediaCanvasComposeObservation;
        MediaCanvasComposeObservation& owner_;
        Scope* previous_;
        bool enabled_;
        Record record_;
        std::optional<std::uint64_t> cpuBegin_;
        std::optional<std::size_t> tile_;
    };
    static void setTile(std::optional<std::size_t> value) noexcept { if (active_) active_->tile_ = value; }
    static bool active() noexcept { return active_ != nullptr; }
    static void recordBatchTiming(const MediaCanvasBatchTiming& value) noexcept {
        if (active_) active_->record_.batchTiming = value;
    }
    template<class Function>
    static auto measure(Stage stage, Function&& function)
    {
        if (!active_) return function();
        const auto begin = mediaSteadyClockNowNs();
        auto result = function();
        const auto end = mediaSteadyClockNowNs();
        if (begin >= 0 && end >= begin) {
            auto& sample = active_->record_.stages[static_cast<std::size_t>(stage)];
            ++sample.count;
            sample.total += end - begin;
            if (end - begin > sample.maximum) {
                sample.maximum = end - begin;
                sample.tile = active_->tile_;
                if constexpr (std::is_integral_v<decltype(result)> || std::is_enum_v<decltype(result)>)
                    sample.result = static_cast<int>(result);
                else if constexpr (requires { result.error().code; })
                    sample.result = result ? 0 : static_cast<int>(result.error().code);
            }
        }
        return result;
    }
    template<class Function>
    static void observeReadiness(Function&& function)
    {
        if (!active_ || !active_->tile_) return;
        const auto begin = mediaSteadyClockNowNs();
        const auto result = function();
        const auto end = mediaSteadyClockNowNs();
        auto& sample = active_->record_.readiness[static_cast<std::size_t>(result.state)];
        ++sample.count;
        if (!sample.result || end - begin > sample.end - sample.begin) {
            sample.begin = begin;
            sample.end = end;
            sample.tile = active_->tile_;
            sample.result = result;
        }
    }
    ~MediaCanvasComposeObservation()
    {
        if (!maximum_) return;
        try {
            static constexpr std::array names{"surface", "validate", "black", "tile", "publish",
                "context_push", "source_wait", "plane_copy", "destination_wait", "context_pop", "batch_completion"};
            static_assert(names.size() == static_cast<std::size_t>(Stage::Count));
            std::ostringstream out;
            out << "canvas_compose_correlated scope=attempt begin_steady_ns=" << maximum_->begin
                << " end_steady_ns=" << maximum_->end << " wall_ns=" << maximum_->end - maximum_->begin
                << " thread_cpu_ns=";
            if (maximum_->cpu) out << *maximum_->cpu; else out << "unavailable";
            if (maximum_->batchTiming) maximum_->batchTiming->write(out);
            for (std::size_t i = 0; i < names.size(); ++i) {
                const auto& sample = maximum_->stages[i];
                out << ' ' << names[i] << "_count=" << sample.count
                    << ' ' << names[i] << "_total_ns=" << sample.total
                    << ' ' << names[i] << "_maximum_ns=" << sample.maximum
                    << ' ' << names[i] << "_maximum_tile=";
                if (sample.tile) out << *sample.tile; else out << "none";
                out << ' ' << names[i] << "_maximum_result=";
                if (sample.result) out << *sample.result; else out << "unavailable";
            }
            static constexpr std::array readinessNames{
                "completed", "ready", "pending", "unavailable", "error"};
            static_assert(readinessNames.size() ==
                static_cast<std::size_t>(MediaVideoReadinessObservation::State::Count));
            for (std::size_t i = 0; i < readinessNames.size(); ++i) {
                const auto& sample = maximum_->readiness[i];
                out << " source_query_" << readinessNames[i] << "_count=" << sample.count;
                if (!sample.result) continue;
                out << " source_query_" << readinessNames[i] << "_maximum_ns=" << sample.end - sample.begin
                    << " source_query_" << readinessNames[i] << "_begin_steady_ns=" << sample.begin
                    << " source_query_" << readinessNames[i] << "_end_steady_ns=" << sample.end
                    << " source_query_" << readinessNames[i] << "_tile=";
                if (sample.tile) out << *sample.tile; else out << "none";
                out << " source_query_" << readinessNames[i] << "_operation=" << sample.result->operation
                    << " source_query_" << readinessNames[i] << "_native=" << sample.result->nativeCode
                    << " source_query_" << readinessNames[i] << "_restore_native=" << sample.result->restoreCode;
            }
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State, MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {}
    }
private:
    inline static thread_local Scope* active_ = nullptr;
    std::optional<Record> maximum_;
};
} // namespace media::ffmpeg::graph
