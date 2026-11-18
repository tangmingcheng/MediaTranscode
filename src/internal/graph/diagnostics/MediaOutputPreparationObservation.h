#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <chrono>
#include <cstdint>
#include <sstream>

namespace media::ffmpeg::graph {

// One bounded record per generation preparation scope, including failed exits.
// Elapsed time is observation only; it never changes release times or deadlines.
class MediaOutputPreparationObservation final {
public:
    MediaOutputPreparationObservation(const char* stage, std::uint64_t generation,
        std::uint64_t plannedBytes, std::uint64_t plannedItems) noexcept
        : m_stage(stage), m_generation(generation), m_bytes(plannedBytes), m_items(plannedItems),
          m_enabled(mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)),
          m_started(std::chrono::steady_clock::now()) {}
    MediaOutputPreparationObservation(const MediaOutputPreparationObservation&) = delete;
    MediaOutputPreparationObservation& operator=(const MediaOutputPreparationObservation&) = delete;
    ~MediaOutputPreparationObservation() noexcept
    {
        if (!m_enabled) return;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - m_started).count();
        try {
            std::ostringstream message;
            message << "output_preparation stage=" << m_stage << " generation=" << m_generation
                << " elapsed_ns=" << elapsed << " planned_bytes=" << m_bytes
                << " planned_items=" << m_items << " event=scope_exit";
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, message.str());
        } catch (...) { /* Diagnostics must not replace the preparation result. */ }
    }
private:
    const char* m_stage;
    std::uint64_t m_generation;
    std::uint64_t m_bytes;
    std::uint64_t m_items;
    bool m_enabled;
    std::chrono::steady_clock::time_point m_started;
};

} // namespace media::ffmpeg::graph
