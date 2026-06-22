#pragma once

#include "internal/FFmpegPipelinePlanner.h"

#include <chrono>
#include <cstdint>

namespace media::ffmpeg {

    class FFmpegVideoFlushDiagnostics {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        struct Context {
            const char* phase = "unknown";
            VideoExecutionMode executionMode = VideoExecutionMode::Cpu;
            const char* backendName = "none";
            bool zeroCopy = false;
            int64_t packetsBefore = 0;
        };

        class Session {
        public:
            explicit Session(Context context);

            TimePoint mark() const;
            void logStep(const char* stepName, TimePoint startedAt) const;
            void logStepPackets(const char* stepName,
                                TimePoint startedAt,
                                int64_t packetsWritten) const;
            void logFailure(const char* stepName, TimePoint startedAt) const;
            void finish(int64_t packetsAfter, bool success) const;

        private:
            Context m_context;
            TimePoint m_startedAt;
        };

        static Context makeContext(const char* phase,
                                   bool hasHardwarePlan,
                                   const HardwarePipelinePlan& hardwarePlan,
                                   bool zeroCopy,
                                   int64_t packetsBefore);

        static const char* executionModeName(VideoExecutionMode mode);

    private:
        static int64_t elapsedMs(TimePoint startedAt);
    };

} // namespace media::ffmpeg
