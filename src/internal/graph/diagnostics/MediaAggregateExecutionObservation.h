#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/MediaNodeProcessResult.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "media_transcode/Result.h"

#include <array>
#include <limits>
#include <optional>
#include <sstream>

namespace media::ffmpeg::graph {

// Owner-thread, fixed-size wall-clock observations. Durations include preemption;
// they are not device execution times. No media references or scheduling inputs.
class MediaAggregateExecutionObservation final {
public:
    enum class Stage { Process, Purge, Bind, Consume, Activate, Video, Audio, Canvas, Emit, Count };
    struct Cursor { std::optional<std::int64_t> video; std::optional<std::int64_t> audio; };

    template<class Function>
    auto measure(Stage stage, Cursor cursor, Function&& function)
    {
        if (!enabled()) return function();
        const auto begin = mediaSteadyClockNowNs();
        auto result = function();
        Record record{begin, mediaSteadyClockNowNs(), cursor, cursor};
        record.error = result ? ::media::ErrorCode::None : result.error().code;
        retain(m_stages[static_cast<std::size_t>(stage)], record);
        return result;
    }

    template<class Function>
    auto process(Cursor cursor, bool pendingAtEntry, Function&& function)
    {
        if (!enabled()) return function();
        const auto begin = mediaSteadyClockNowNs();
        if (m_previous) {
            auto gap = *m_previous;
            gap.begin = m_previous->end;
            gap.end = begin;
            gap.nextCursor = cursor;
            retain(m_betweenCalls, gap);
        }
        m_enteredOnProcess = false;
        auto result = function();
        Record record{begin, mediaSteadyClockNowNs(), cursor, cursor};
        record.pendingAtEntry = pendingAtEntry;
        record.enteredOnProcess = m_enteredOnProcess;
        record.error = result ? ::media::ErrorCode::None : result.error().code;
        if (result) {
            record.state = result.value().state;
            if (const auto& wait = result.value().deadlineWait) {
                record.wakePolicy = wait->wakePolicy;
                if (const auto* master = std::get_if<MediaNodeProcessResult::DeadlineWait::AvSyncMaster>(&wait->deadline)) {
                    record.deadlineNs = master->deadline.nanoseconds();
                    record.masterDeadline = true;
                } else {
                    record.deadlineNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::get<MediaNodeProcessResult::DeadlineWait::Steady>(wait->deadline).deadline.time_since_epoch()).count();
                }
            }
        }
        retain(m_stages[static_cast<std::size_t>(Stage::Process)], record);
        m_previous = record;
        return result;
    }

    void enteredOnProcess() noexcept { m_enteredOnProcess = true; }

    void log(std::uint32_t node, std::uint64_t generation) const noexcept
    {
        static constexpr std::array names{
            "process", "purge", "bind", "consume", "activate", "build_video", "build_audio", "canvas", "emit"};
        static_assert(names.size() == static_cast<std::size_t>(Stage::Count));
        for (std::size_t i = 0; i < names.size(); ++i) logInterval(node, generation, names[i], m_stages[i]);
        logInterval(node, generation, "between_calls_previous_return", m_betweenCalls);
    }

private:
    struct Record {
        std::int64_t begin;
        std::int64_t end;
        Cursor cursor;
        Cursor nextCursor;
        ::media::ErrorCode error = ::media::ErrorCode::None;
        std::optional<MediaNodeProcessState> state;
        std::optional<std::int64_t> deadlineNs;
        std::optional<MediaNodeDeadlineWakePolicy> wakePolicy;
        bool masterDeadline = false;
        bool pendingAtEntry = false;
        bool enteredOnProcess = false;
    };
    struct Maximum {
        std::optional<Record> record;
        std::uint64_t count = 0;
    };
    static bool enabled() noexcept
    {
        return mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State);
    }
    static void retain(Maximum& maximum, const Record& record) noexcept
    {
        if (record.begin < 0 || record.end < record.begin) return;
        if (!maximum.record || record.end - record.begin > maximum.record->end - maximum.record->begin)
            maximum.record = record;
        if (maximum.count != std::numeric_limits<std::uint64_t>::max()) ++maximum.count;
    }
    static void logInterval(std::uint32_t node, std::uint64_t generation,
                            const char* stage, const Maximum& maximum) noexcept
    {
        if (!maximum.record) return;
        try {
            const auto& record = *maximum.record;
            std::ostringstream message;
            message << "aggregate_execution node=" << node << " generation=" << generation
                    << " stage=" << stage << " count=" << maximum.count
                    << " maximum_ns=" << record.end - record.begin
                    << " begin_steady_ns=" << record.begin << " end_steady_ns=" << record.end
                    << " video_cursor=" << (record.cursor.video ? std::to_string(*record.cursor.video) : "not_owned") << " audio_cursor=" << (record.cursor.audio ? std::to_string(*record.cursor.audio) : "not_owned")
                    << " next_video_cursor=" << (record.nextCursor.video ? std::to_string(*record.nextCursor.video) : "not_owned") << " next_audio_cursor=" << (record.nextCursor.audio ? std::to_string(*record.nextCursor.audio) : "not_owned")
                    << " error=" << ::media::errorCodeName(record.error);
            if (record.state) {
                const auto state = *record.state;
                message << " return_state=" << (state == MediaNodeProcessState::Progress ? "progress" :
                    state == MediaNodeProcessState::Waiting ? "waiting" : "finished");
            } else message << " return_state=unavailable";
            if (record.deadlineNs) {
                message << " deadline_ns=" << *record.deadlineNs
                        << " deadline_clock=" << (record.masterDeadline ? "master" : "steady")
                        << " wake_policy=" << static_cast<int>(*record.wakePolicy);
            } else message << " deadline_ns=unavailable";
            message << " pending_at_entry=" << record.pendingAtEntry
                    << " entered_onprocess=" << record.enteredOnProcess;
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, message.str());
        } catch (...) {
            // Formatting must never replace a media error or affect shutdown.
        }
    }
    std::array<Maximum, static_cast<std::size_t>(Stage::Count)> m_stages;
    Maximum m_betweenCalls;
    std::optional<Record> m_previous;
    bool m_enteredOnProcess = false;
};

} // namespace media::ffmpeg::graph
