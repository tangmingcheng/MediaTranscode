#pragma once

#include "internal/FFmpegPhaseDiagnostics.h"

#include <chrono>
#include <cstdint>

namespace media::ffmpeg {

    class FFmpegTranscodeLoopDiagnostics {
    public:
        using Snapshot = FFmpegPhaseDiagnostics::Snapshot;

        enum class StreamKind {
            Video,
            Audio,
            Other
        };

        struct OutputCounters {
            int64_t videoPackets = 0;
            int64_t audioPackets = 0;
            int64_t progressCallbacks = 0;
            int64_t outTimeMs = 0;
        };

        explicit FFmpegTranscodeLoopDiagnostics(int intervalMs = 1000);

        Snapshot mark() const;
        void recordReadPacket(StreamKind streamKind, const Snapshot& before);
        void recordProcessPacket(StreamKind streamKind, const Snapshot& before);
        void maybeLog(const OutputCounters& outputCounters);
        void flush(const OutputCounters& outputCounters);

    private:
        struct Accumulator {
            int64_t count = 0;
            int64_t wallMs = 0;
            int64_t processCpuMs = 0;
            int64_t threadCpuMs = 0;
        };

        struct Window {
            Snapshot started;
            OutputCounters outputAtStart;

            Accumulator read;
            Accumulator videoProcess;
            Accumulator audioProcess;
            Accumulator otherProcess;

            int64_t videoInputPackets = 0;
            int64_t audioInputPackets = 0;
            int64_t otherInputPackets = 0;
        };

        void resetWindow(const OutputCounters& outputCounters);
        void recordAccumulator(Accumulator& accumulator, const Snapshot& before);
        void logWindow(const OutputCounters& outputCounters, const char* reason);

    private:
        int m_intervalMs = 1000;
        Window m_window;
        bool m_windowStarted = false;
    };

} // namespace media::ffmpeg
