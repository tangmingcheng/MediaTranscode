#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace media::ffmpeg {

    class FFmpegPhaseDiagnostics {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        struct Snapshot {
            TimePoint wallTime = Clock::now();
            int64_t processCpuMs = -1;
            int64_t threadCpuMs = -1;
        };

        struct Counters {
            int64_t videoPackets = 0;
            int64_t audioPackets = 0;
            int64_t progressCallbacks = 0;
        };

        class Session {
        public:
            explicit Session(const char* name);

            Snapshot mark() const;
            void logStep(const char* stepName,
                         const Snapshot& before,
                         const Counters& countersBefore,
                         const Counters& countersAfter,
                         const std::string& details = {}) const;
            void logFailure(const char* stepName,
                            const Snapshot& before,
                            const Counters& countersBefore,
                            const Counters& countersAfter,
                            const std::string& details = {}) const;
            void finish(bool success,
                        const Counters& countersBefore,
                        const Counters& countersAfter,
                        const std::string& details = {}) const;

        private:
            const char* m_name = "unknown";
            Snapshot m_started;
        };

        static Snapshot snapshot();
        static int64_t wallElapsedMs(const Snapshot& before, const Snapshot& after);
        static int64_t processCpuElapsedMs(const Snapshot& before, const Snapshot& after);
        static int64_t threadCpuElapsedMs(const Snapshot& before, const Snapshot& after);

    private:
        static int64_t currentProcessCpuMs();
        static int64_t currentThreadCpuMs();
    };

} // namespace media::ffmpeg
