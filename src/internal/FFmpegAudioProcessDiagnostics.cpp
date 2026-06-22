#include "internal/FFmpegAudioProcessDiagnostics.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <sstream>

namespace media::ffmpeg {
namespace {

    int64_t clampUnavailableCpuDelta(int64_t value)
    {
        return value < 0 ? 0 : value;
    }

    std::string formatAccumulator(const char* name,
                                  const FFmpegAudioProcessDiagnostics::Snapshot& windowStart,
                                  const FFmpegAudioProcessDiagnostics::Snapshot& now,
                                  const FFmpegAudioProcessDiagnostics::Snapshot& /*unused*/)
    {
        (void)name;
        (void)windowStart;
        (void)now;
        return {};
    }

    std::string accumulatorFields(const char* name,
                                  const FFmpegAudioProcessDiagnostics::Step /*step*/,
                                  int64_t count,
                                  int64_t wallMs,
                                  int64_t processCpuMs,
                                  int64_t threadCpuMs)
    {
        std::ostringstream oss;
        oss << name << "_count=" << count
            << ", " << name << "_wall=" << wallMs << "ms"
            << ", " << name << "_process_cpu=" << processCpuMs << "ms"
            << ", " << name << "_thread_cpu=" << threadCpuMs << "ms";
        return oss.str();
    }

} // namespace

    FFmpegAudioProcessDiagnostics::FFmpegAudioProcessDiagnostics(int intervalMs)
        : m_intervalMs(std::max(100, intervalMs))
    {
    }

    FFmpegAudioProcessDiagnostics::~FFmpegAudioProcessDiagnostics()
    {
        flush(0, 0, "destruct");
    }

    FFmpegAudioProcessDiagnostics::Snapshot FFmpegAudioProcessDiagnostics::mark() const
    {
        return FFmpegPhaseDiagnostics::snapshot();
    }

    void FFmpegAudioProcessDiagnostics::record(Step step, const Snapshot& before)
    {
        ensureWindowStarted(0);

        const Snapshot after = FFmpegPhaseDiagnostics::snapshot();
        Accumulator& accumulator = m_window.steps[stepIndex(step)];
        ++accumulator.count;
        accumulator.wallMs += FFmpegPhaseDiagnostics::wallElapsedMs(before, after);
        accumulator.processCpuMs += clampUnavailableCpuDelta(
            FFmpegPhaseDiagnostics::processCpuElapsedMs(before, after)
        );
        accumulator.threadCpuMs += clampUnavailableCpuDelta(
            FFmpegPhaseDiagnostics::threadCpuElapsedMs(before, after)
        );
    }

    void FFmpegAudioProcessDiagnostics::maybeLog(int64_t packetCount,
                                                 int fifoSize,
                                                 const char* reason)
    {
        ensureWindowStarted(packetCount);

        const Snapshot now = FFmpegPhaseDiagnostics::snapshot();
        if (FFmpegPhaseDiagnostics::wallElapsedMs(m_window.started, now) >= m_intervalMs) {
            logWindow(packetCount, fifoSize, reason ? reason : "interval");
            resetWindow(packetCount);
        }
    }

    void FFmpegAudioProcessDiagnostics::flush(int64_t packetCount,
                                              int fifoSize,
                                              const char* reason)
    {
        if (!m_windowStarted) {
            return;
        }

        logWindow(packetCount, fifoSize, reason ? reason : "flush");
        m_windowStarted = false;
    }

    void FFmpegAudioProcessDiagnostics::reset()
    {
        m_window = Window{};
        m_windowStarted = false;
    }

    void FFmpegAudioProcessDiagnostics::ensureWindowStarted(int64_t packetCount)
    {
        if (!m_windowStarted) {
            resetWindow(packetCount);
        }
    }

    void FFmpegAudioProcessDiagnostics::resetWindow(int64_t packetCount)
    {
        m_window = Window{};
        m_window.started = FFmpegPhaseDiagnostics::snapshot();
        m_window.packetCountAtStart = packetCount;
        m_windowStarted = true;
    }

    void FFmpegAudioProcessDiagnostics::logWindow(int64_t packetCount,
                                                  int fifoSize,
                                                  const char* reason)
    {
        const Snapshot now = FFmpegPhaseDiagnostics::snapshot();
        const int64_t wallMs = FFmpegPhaseDiagnostics::wallElapsedMs(m_window.started, now);
        const int64_t processCpuMs = FFmpegPhaseDiagnostics::processCpuElapsedMs(m_window.started, now);
        const int64_t threadCpuMs = FFmpegPhaseDiagnostics::threadCpuElapsedMs(m_window.started, now);

        std::ostringstream oss;
        oss << "[AUDIO][PROCESS] reason=" << (reason ? reason : "unknown")
            << ", wall=" << wallMs << "ms"
            << ", process_cpu=" << processCpuMs << "ms"
            << ", thread_cpu=" << threadCpuMs << "ms"
            << ", packets_delta=" << std::max<int64_t>(0, packetCount - m_window.packetCountAtStart)
            << ", packets_total=" << packetCount
            << ", fifo_size=" << fifoSize;

        for (size_t i = 0; i < static_cast<size_t>(Step::Count); ++i) {
            const Step step = static_cast<Step>(i);
            const Accumulator& accumulator = m_window.steps[i];
            oss << ", "
                << accumulatorFields(
                    stepName(step),
                    step,
                    accumulator.count,
                    accumulator.wallMs,
                    accumulator.processCpuMs,
                    accumulator.threadCpuMs
                );
        }

        spdlog::info("{}", oss.str());
    }

    const char* FFmpegAudioProcessDiagnostics::stepName(Step step)
    {
        switch (step) {
        case Step::DecoderSendPacket:
            return "decoder_send";
        case Step::DecoderReceiveFrame:
            return "decoder_receive";
        case Step::ResampleConvert:
            return "resample_convert";
        case Step::FifoWrite:
            return "fifo_write";
        case Step::FifoRead:
            return "fifo_read";
        case Step::EncoderSendFrame:
            return "encoder_send";
        case Step::EncoderReceivePacket:
            return "encoder_receive";
        case Step::PacketWrite:
            return "packet_write";
        case Step::FlushResamplerConvert:
            return "flush_resampler_convert";
        case Step::FrameAlloc:
            return "frame_alloc";
        case Step::Count:
        default:
            return "unknown";
        }
    }

    size_t FFmpegAudioProcessDiagnostics::stepIndex(Step step)
    {
        const size_t index = static_cast<size_t>(step);
        return index < static_cast<size_t>(Step::Count)
            ? index
            : static_cast<size_t>(Step::Count) - 1;
    }

} // namespace media::ffmpeg
