#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>

namespace media::ffmpeg::graph {

// One bounded record owned by one head, not a cross-AU maximum.
// Times describe scheduler observations, not the instant a producer finished.
struct MediaAvHeadWaitObservation final {
    std::optional<std::int64_t> begin;
    std::optional<std::int64_t> end;
    std::optional<std::uint64_t> resumedGeneration;
    std::optional<std::uint64_t> resumedSequence;
    std::uint64_t intervals = 0;
    std::int64_t totalNanoseconds = 0;
    bool active = false;
    bool valid = true;

    void observe(std::int64_t now, bool missing,
        std::optional<std::uint64_t> generation,
        std::optional<std::uint64_t> sequence) noexcept
    {
        if (!valid) return;
        if (now < 0) { valid = false; return; }
        if (missing) {
            if (!active) {
                if (intervals == (std::numeric_limits<std::uint64_t>::max)()) {
                    valid = false;
                    return;
                }
                begin = now;
                end.reset();
                resumedGeneration.reset();
                resumedSequence.reset();
                ++intervals;
                active = true;
            }
        } else if (active) {
            end = now;
            if (now < *begin || now - *begin >
                (std::numeric_limits<std::int64_t>::max)() - totalNanoseconds) {
                valid = false;
                return;
            }
            totalNanoseconds += now - *begin;
            resumedGeneration = generation;
            resumedSequence = sequence;
            active = false;
        }
    }

    void append(std::ostream& out) const
    {
        out << " head_missing_valid=" << valid
            << " head_missing_intervals=" << intervals
            << " head_missing_completed_ns=" << totalNanoseconds
            << " head_missing_active=" << active;
        const auto field = [&](const char* name, auto value) {
            out << ' ' << name << '=';
            if (value) out << *value; else out << "unavailable";
        };
        field("head_missing_last_begin_ns", begin);
        field("head_missing_last_end_ns", end);
        field("head_resumed_generation", resumedGeneration);
        field("head_resumed_sequence", resumedSequence);
    }
};

} // namespace media::ffmpeg::graph
