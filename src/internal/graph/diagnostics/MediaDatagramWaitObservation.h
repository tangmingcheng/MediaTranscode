#pragma once
#include "internal/graph/diagnostics/MediaNodeWaitObservation.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"
#include <limits>
#include <array>
#include <ostream>

namespace media::ffmpeg::graph {
struct MediaDatagramWaitAttempt final {
    std::uint64_t sequence;
    std::int64_t targetMasterNs;
    std::int64_t sampledMasterNs;
    std::int64_t sampledSteadyNs;
    std::int64_t timeoutNs;
    MediaNodeWaitObservation wait;
    std::optional<MediaNodeWakeup::WaitOutcome> outcome;
    std::optional<std::int64_t> nextMasterNs;
    std::optional<std::int64_t> nextSteadyNs;
};

struct MediaDatagramWaitMetric final {
    std::uint64_t count = 0;
    std::int64_t total = 0;
    std::int64_t maximum = 0;
    void add(std::int64_t value, bool& incomplete) noexcept
    {
        if (value < 0 || count == (std::numeric_limits<std::uint64_t>::max)() ||
            value > (std::numeric_limits<std::int64_t>::max)() - total) {
            incomplete = true; return;
        }
        ++count; total += value; if (value > maximum) maximum = value;
    }
    void append(std::ostream& out, const char* name) const
    {
        out << ' ' << name << "_count=" << count << ' ' << name << "_total_ns=" << total
            << ' ' << name << "_max_ns=" << maximum;
    }
};

// Fixed per-reservation/per-batch aggregates. The API wall interval includes
// timer/event waiting AND thread rescheduling; it is not pure timer latency.
struct MediaDatagramWaitObservation final {
    std::uint64_t attempts = 0;
    std::uint64_t notified = 0;
    std::uint64_t deadlines = 0;
    std::uint64_t interrupted = 0;
    std::uint64_t unavailable = 0;
    bool incomplete = false;
    MediaDatagramWaitMetric call, beforeCall, arm, api, afterCall, targetLateness;
    std::optional<MediaDatagramWaitAttempt> worst;

    void observe(const MediaDatagramWaitAttempt& sample) noexcept
    {
        const auto increment = [&](std::uint64_t& count) {
            if (count == (std::numeric_limits<std::uint64_t>::max)()) incomplete = true;
            else ++count;
        };
        increment(attempts);
        if (!sample.outcome) increment(unavailable);
        else switch (*sample.outcome) {
            case MediaNodeWakeup::WaitOutcome::Notified: increment(notified); break;
            case MediaNodeWakeup::WaitOutcome::Deadline: increment(deadlines); break;
            case MediaNodeWakeup::WaitOutcome::Interrupted: increment(interrupted); break;
        }
        const auto interval = [&](MediaDatagramWaitMetric& metric,
            std::optional<std::int64_t> begin, std::optional<std::int64_t> end, bool required) {
            if (!begin || !end) { if (required || begin || end) incomplete = true; return; }
            if (*begin < 0 || *end < *begin) { incomplete = true; return; }
            metric.add(*end - *begin, incomplete);
        };
        std::optional<std::int64_t> previous;
        for (const auto& time : std::array{sample.wait.entered, sample.wait.armBefore,
                 sample.wait.armAfter, sample.wait.apiBefore, sample.wait.apiReturned, sample.wait.returned}) {
            if (!time) continue;
            if (*time < 0 || (previous && *time < *previous)) incomplete = true;
            previous = time;
        }
        const auto previousMaximum = call.maximum;
        interval(call, sample.wait.entered, sample.wait.returned, true);
        interval(beforeCall, sample.sampledSteadyNs, sample.wait.entered, true);
        interval(arm, sample.wait.armBefore, sample.wait.armAfter, false);
        interval(api, sample.wait.apiBefore, sample.wait.apiReturned, false);
        interval(afterCall, sample.wait.returned, sample.nextSteadyNs, true);
        if (!sample.outcome || !sample.nextMasterNs || sample.targetMasterNs < 0 ||
            sample.sampledMasterNs < 0 || sample.timeoutNs <= 0 ||
            (sample.nextMasterNs && *sample.nextMasterNs < sample.sampledMasterNs)) incomplete = true;
        else targetLateness.add(*sample.nextMasterNs > sample.targetMasterNs
            ? *sample.nextMasterNs - sample.targetMasterNs : 0, incomplete);
        if (!worst || call.maximum > previousMaximum) worst = sample;
    }

    void append(std::ostream& out) const
    {
        out << " wait_attempts=" << attempts << " wait_incomplete=" << incomplete
            << " wait_notified_count=" << notified << " wait_deadline_count=" << deadlines
            << " wait_interrupted_count=" << interrupted << " wait_unavailable_count=" << unavailable;
        call.append(out, "wait_call"); beforeCall.append(out, "wait_before_call");
        arm.append(out, "wait_arm"); api.append(out, "wait_api_wall");
        afterCall.append(out, "wait_return_to_next_clock"); targetLateness.append(out, "wait_target_lateness");
        if (!worst) return;
        const auto& s = *worst;
        const auto field = [&](const char* name, const auto& value) {
            out << ' ' << name << '=';
            if (value) out << *value; else out << "unavailable";
        };
        out << " worst_wait_metric=call_wall worst_wait_sequence=" << s.sequence
            << " worst_wait_target_master_ns=" << s.targetMasterNs
            << " worst_wait_sampled_master_ns=" << s.sampledMasterNs
            << " worst_wait_sampled_steady_ns=" << s.sampledSteadyNs
            << " worst_wait_timeout_ns=" << s.timeoutNs;
        field("worst_wait_entered_steady_ns", s.wait.entered);
        field("worst_wait_returned_steady_ns", s.wait.returned);
        field("worst_wait_arm_before_steady_ns", s.wait.armBefore);
        field("worst_wait_arm_after_steady_ns", s.wait.armAfter);
        field("worst_wait_api_before_steady_ns", s.wait.apiBefore);
        field("worst_wait_api_returned_steady_ns", s.wait.apiReturned);
        field("worst_wait_windows_due_100ns", s.wait.windowsDue100ns);
        field("worst_wait_windows_result", s.wait.windowsWaitResult);
        field("worst_wait_windows_error", s.wait.windowsError);
        field("worst_wait_windows_pid", s.wait.windowsProcessId);
        field("worst_wait_windows_tid", s.wait.windowsThreadId);
        field("worst_wait_windows_qpc_frequency_hz", s.wait.windowsQpcFrequency);
        field("worst_wait_linux_tid", s.wait.linuxThreadId);
        field("worst_wait_linux_scheduler_policy", s.wait.linuxSchedulerPolicy);
        field("worst_wait_linux_timer_slack_ns", s.wait.linuxTimerSlackNs);
        field("worst_wait_linux_timer_slack_error", s.wait.linuxTimerSlackError);
        field("worst_wait_next_master_ns", s.nextMasterNs);
        field("worst_wait_next_steady_ns", s.nextSteadyNs);
        out << " worst_wait_outcome=";
        if (!s.outcome) out << "unavailable";
        else switch (*s.outcome) {
            case MediaNodeWakeup::WaitOutcome::Notified: out << "notified"; break;
            case MediaNodeWakeup::WaitOutcome::Deadline: out << "deadline"; break;
            case MediaNodeWakeup::WaitOutcome::Interrupted: out << "interrupted"; break;
        }
    }
};
} // namespace media::ffmpeg::graph
