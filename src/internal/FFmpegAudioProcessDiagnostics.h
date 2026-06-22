#pragma once

#include "internal/FFmpegPhaseDiagnostics.h"

#include <array>
#include <cstdint>

namespace media::ffmpeg {

    class FFmpegAudioProcessDiagnostics {
    public:
        using Snapshot = FFmpegPhaseDiagnostics::Snapshot;

        enum class Step {
            DecoderSendPacket = 0,
            DecoderReceiveFrame,
            ResampleConvert,
            FifoWrite,
            FifoRead,
            EncoderSendFrame,
            EncoderReceivePacket,
            PacketWrite,
            FlushResamplerConvert,
            FrameAlloc,
            Count
        };

        explicit FFmpegAudioProcessDiagnostics(int intervalMs = 1000);
        ~FFmpegAudioProcessDiagnostics();

        Snapshot mark() const;
        void record(Step step, const Snapshot& before);
        void maybeLog(int64_t packetCount, int fifoSize, const char* reason = "interval");
        void flush(int64_t packetCount, int fifoSize, const char* reason = "flush");
        void reset();

    private:
        struct Accumulator {
            int64_t count = 0;
            int64_t wallMs = 0;
            int64_t processCpuMs = 0;
            int64_t threadCpuMs = 0;
        };

        struct Window {
            Snapshot started;
            int64_t packetCountAtStart = 0;
            std::array<Accumulator, static_cast<size_t>(Step::Count)> steps{};
        };

        void ensureWindowStarted(int64_t packetCount);
        void resetWindow(int64_t packetCount);
        void logWindow(int64_t packetCount, int fifoSize, const char* reason);
        static const char* stepName(Step step);
        static size_t stepIndex(Step step);

    private:
        int m_intervalMs = 1000;
        Window m_window;
        bool m_windowStarted = false;
    };

} // namespace media::ffmpeg
