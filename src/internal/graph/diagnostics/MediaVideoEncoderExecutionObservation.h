#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/time/MediaSteadyClock.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

namespace media::ffmpeg::graph {

// Owner-thread, fixed-size evidence. The interval between submissions includes
// receive/drain, backpressure, input cadence and scheduling, not just idle time.
// Payload waiting spans a WouldBlock observation through the next successful
// admission, including time before the worker retries. PTS is in the encoder
// time base; observations never influence media decisions.
class MediaVideoEncoderExecutionObservation final {
public:
    template<class Function>
    int measure(std::int64_t pts, Function&& function)
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State))
            return function();
        const auto begin = mediaSteadyClockNowNs();
        const int result = function();
        const Record current{begin, mediaSteadyClockNowNs(), pts, result};
        if (current.begin < 0 || current.end < current.begin) return result;
        if (m_count != std::numeric_limits<std::uint64_t>::max()) ++m_count;
        if (!m_maximum || current.end - current.begin > m_maximum->end - m_maximum->begin)
            m_maximum = current;
        if (m_previous && current.begin >= m_previous->end) {
            const auto gap = current.begin - m_previous->end;
            if (!m_gap || gap > *m_gap) {
                m_gap = gap;
                m_gapBefore = m_previous;
                m_gapAfter = current;
            }
        }
        m_previous = current;
        return result;
    }

    void observePayloadAdmission(bool waiting) noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        const auto now = mediaSteadyClockNowNs();
        if (now < 0) return;
        if (waiting) {
            if (!m_payloadWaitBegin) m_payloadWaitBegin = now;
        } else if (m_payloadWaitBegin) {
            if (now >= *m_payloadWaitBegin &&
                (!m_payloadWaitMaximum || now - *m_payloadWaitBegin > *m_payloadWaitMaximum)) {
                m_payloadWaitMaximum = now - *m_payloadWaitBegin;
                m_payloadMaximumBegin = m_payloadWaitBegin;
                m_payloadMaximumEnd = now;
            }
            m_payloadWaitBegin.reset();
        }
    }

    void log(std::uint32_t node) const noexcept
    {
        if (!m_maximum && !m_payloadWaitBegin && !m_payloadWaitMaximum) return;
        try {
            std::ostringstream out;
            out << "video_encoder_execution_observation node=" << node
                << " count=" << m_count
                << " maximum_call_ns=" << (m_maximum ? std::to_string(m_maximum->end - m_maximum->begin) : "unavailable");
            const auto append = [&](const char* prefix, const Record& record) {
                out << ' ' << prefix << "_begin_steady_ns=" << record.begin
                    << ' ' << prefix << "_end_steady_ns=" << record.end
                    << ' ' << prefix << "_encoder_pts=" << record.pts
                    << ' ' << prefix << "_native_result=" << record.result;
            };
            if (m_maximum) append("maximum", *m_maximum);
            if (m_gap) {
                out << " maximum_between_submissions_ns=" << *m_gap;
                append("before_gap", *m_gapBefore);
                append("after_gap", *m_gapAfter);
            }
            if (m_payloadWaitMaximum)
                out << " payload_wait_maximum_ns=" << *m_payloadWaitMaximum
                    << " payload_wait_begin_steady_ns=" << *m_payloadMaximumBegin
                    << " payload_wait_end_steady_ns=" << *m_payloadMaximumEnd;
            if (m_payloadWaitBegin)
                out << " payload_wait_active_begin_steady_ns=" << *m_payloadWaitBegin;
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Preserve the original media error if diagnostic formatting fails.
        }
    }

private:
    struct Record {
        std::int64_t begin;
        std::int64_t end;
        std::int64_t pts;
        int result;
    };
    std::optional<Record> m_previous;
    std::optional<Record> m_maximum;
    std::optional<Record> m_gapBefore;
    std::optional<Record> m_gapAfter;
    std::optional<std::int64_t> m_gap;
    std::optional<std::int64_t> m_payloadWaitBegin;
    std::optional<std::int64_t> m_payloadWaitMaximum;
    std::optional<std::int64_t> m_payloadMaximumBegin;
    std::optional<std::int64_t> m_payloadMaximumEnd;
    std::uint64_t m_count = 0;
};

} // namespace media::ffmpeg::graph
