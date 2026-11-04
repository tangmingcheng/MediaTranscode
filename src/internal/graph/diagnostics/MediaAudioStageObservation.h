#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/sync/MediaCanonicalAudioSampleInterval.h"
#include "internal/graph/time/MediaSteadyClock.h"

#include <limits>
#include <optional>
#include <sstream>

namespace media::ffmpeg::graph {

// Owner-thread diagnostics only. Retains the latest observation and the two
// endpoints of the largest observed inter-event gap, never media ownership.
// A gap includes normal media cadence, backpressure and thread scheduling;
// it is not a measurement of codec execution time or queue residence.
class MediaAudioStageObservation final {
public:
    void observe(std::uint64_t generation,
                 MediaCanonicalAudioSampleInterval interval) noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        const Record current{generation, interval, mediaSteadyClockNowNs()};
        if (m_last && m_last->generation == generation &&
            current.steadyNs >= m_last->steadyNs && m_last->steadyNs >= 0) {
            const auto gap = current.steadyNs - m_last->steadyNs;
            if (!m_gap || gap > *m_gap) {
                m_gap = gap;
                m_gapBegin = m_last;
                m_gapEnd = current;
            }
        }
        m_last = current;
        if (m_count != std::numeric_limits<std::uint64_t>::max()) ++m_count;
    }

    void log(std::uint32_t node, const char* stage) const noexcept
    {
        if (!m_last) return;
        try {
            std::ostringstream message;
            message << "audio_stage_observation node=" << node
                    << " stage=" << stage << " count=" << m_count;
            const auto append = [&](const char* prefix, const Record& record) {
                message << ' ' << prefix << "_generation=" << record.generation
                        << ' ' << prefix << "_begin=" << record.interval.begin
                        << ' ' << prefix << "_end=" << record.interval.end
                        << ' ' << prefix << "_rate=" << record.interval.sampleRate
                        << ' ' << prefix << "_steady_ns=" << record.steadyNs;
            };
            append("last", *m_last);
            if (m_gap) {
                message << " maximum_inter_event_gap_ns=" << *m_gap;
                append("gap_begin", *m_gapBegin);
                append("gap_end", *m_gapEnd);
            } else {
                message << " maximum_inter_event_gap_ns=unavailable";
            }
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, message.str());
        } catch (...) {
            // Diagnostic formatting must not change media failure semantics.
        }
    }

private:
    struct Record {
        std::uint64_t generation;
        MediaCanonicalAudioSampleInterval interval;
        std::int64_t steadyNs;
    };
    std::optional<Record> m_last;
    std::optional<Record> m_gapBegin;
    std::optional<Record> m_gapEnd;
    std::optional<std::int64_t> m_gap;
    std::uint64_t m_count = 0;
};

} // namespace media::ffmpeg::graph
