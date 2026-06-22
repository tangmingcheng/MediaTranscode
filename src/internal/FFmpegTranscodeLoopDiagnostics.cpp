#include "internal/FFmpegTranscodeLoopDiagnostics.h"

#include "spdlog/spdlog.h"

#include <algorithm>

namespace media::ffmpeg {
namespace {

    int64_t clampUnavailableCpuDelta(int64_t value)
    {
        return value < 0 ? 0 : value;
    }

    int64_t nonNegativeDelta(int64_t after, int64_t before)
    {
        return std::max<int64_t>(0, after - before);
    }

} // namespace

    FFmpegTranscodeLoopDiagnostics::FFmpegTranscodeLoopDiagnostics(int intervalMs)
        : m_intervalMs(std::max(100, intervalMs))
    {
    }

    FFmpegTranscodeLoopDiagnostics::Snapshot FFmpegTranscodeLoopDiagnostics::mark() const
    {
        return FFmpegPhaseDiagnostics::snapshot();
    }

    void FFmpegTranscodeLoopDiagnostics::recordReadPacket(StreamKind streamKind,
                                                          const Snapshot& before)
    {
        if (!m_windowStarted) {
            return;
        }

        recordAccumulator(m_window.read, before);

        switch (streamKind) {
        case StreamKind::Video:
            ++m_window.videoInputPackets;
            break;
        case StreamKind::Audio:
            ++m_window.audioInputPackets;
            break;
        case StreamKind::Other:
        default:
            ++m_window.otherInputPackets;
            break;
        }
    }

    void FFmpegTranscodeLoopDiagnostics::recordProcessPacket(StreamKind streamKind,
                                                             const Snapshot& before)
    {
        if (!m_windowStarted) {
            return;
        }

        switch (streamKind) {
        case StreamKind::Video:
            recordAccumulator(m_window.videoProcess, before);
            break;
        case StreamKind::Audio:
            recordAccumulator(m_window.audioProcess, before);
            break;
        case StreamKind::Other:
        default:
            recordAccumulator(m_window.otherProcess, before);
            break;
        }
    }

    void FFmpegTranscodeLoopDiagnostics::maybeLog(const OutputCounters& outputCounters)
    {
        if (!m_windowStarted) {
            resetWindow(outputCounters);
            return;
        }

        const Snapshot now = FFmpegPhaseDiagnostics::snapshot();
        if (FFmpegPhaseDiagnostics::wallElapsedMs(m_window.started, now) >= m_intervalMs) {
            logWindow(outputCounters, "interval");
            resetWindow(outputCounters);
        }
    }

    void FFmpegTranscodeLoopDiagnostics::flush(const OutputCounters& outputCounters)
    {
        if (!m_windowStarted) {
            return;
        }

        logWindow(outputCounters, "flush");
        m_windowStarted = false;
    }

    void FFmpegTranscodeLoopDiagnostics::resetWindow(const OutputCounters& outputCounters)
    {
        m_window = Window{};
        m_window.started = FFmpegPhaseDiagnostics::snapshot();
        m_window.outputAtStart = outputCounters;
        m_windowStarted = true;
    }

    void FFmpegTranscodeLoopDiagnostics::recordAccumulator(Accumulator& accumulator,
                                                           const Snapshot& before)
    {
        const Snapshot after = FFmpegPhaseDiagnostics::snapshot();
        ++accumulator.count;
        accumulator.wallMs += FFmpegPhaseDiagnostics::wallElapsedMs(before, after);
        accumulator.processCpuMs += clampUnavailableCpuDelta(
            FFmpegPhaseDiagnostics::processCpuElapsedMs(before, after)
        );
        accumulator.threadCpuMs += clampUnavailableCpuDelta(
            FFmpegPhaseDiagnostics::threadCpuElapsedMs(before, after)
        );
    }

    void FFmpegTranscodeLoopDiagnostics::logWindow(const OutputCounters& outputCounters,
                                                   const char* reason)
    {
        const Snapshot now = FFmpegPhaseDiagnostics::snapshot();
        const int64_t windowWallMs = FFmpegPhaseDiagnostics::wallElapsedMs(m_window.started, now);
        const int64_t processCpuMs = FFmpegPhaseDiagnostics::processCpuElapsedMs(m_window.started, now);
        const int64_t threadCpuMs = FFmpegPhaseDiagnostics::threadCpuElapsedMs(m_window.started, now);

        spdlog::info(
            "[LOOP][TRANSCODE] reason={}, wall={}ms, process_cpu={}ms, thread_cpu={}ms, "
            "read_packets={}, video_in={}, audio_in={}, other_in={}, "
            "video_out_delta={}, audio_out_delta={}, progress_callbacks_delta={}, out_time_ms={}, "
            "read_wall={}ms, read_process_cpu={}ms, read_thread_cpu={}ms, "
            "video_process_count={}, video_process_wall={}ms, video_process_cpu={}ms, video_process_thread_cpu={}ms, "
            "audio_process_count={}, audio_process_wall={}ms, audio_process_cpu={}ms, audio_process_thread_cpu={}ms, "
            "other_process_count={}, other_process_wall={}ms, other_process_cpu={}ms, other_process_thread_cpu={}ms",
            reason ? reason : "unknown",
            windowWallMs,
            processCpuMs,
            threadCpuMs,
            m_window.read.count,
            m_window.videoInputPackets,
            m_window.audioInputPackets,
            m_window.otherInputPackets,
            nonNegativeDelta(outputCounters.videoPackets, m_window.outputAtStart.videoPackets),
            nonNegativeDelta(outputCounters.audioPackets, m_window.outputAtStart.audioPackets),
            nonNegativeDelta(outputCounters.progressCallbacks, m_window.outputAtStart.progressCallbacks),
            outputCounters.outTimeMs,
            m_window.read.wallMs,
            m_window.read.processCpuMs,
            m_window.read.threadCpuMs,
            m_window.videoProcess.count,
            m_window.videoProcess.wallMs,
            m_window.videoProcess.processCpuMs,
            m_window.videoProcess.threadCpuMs,
            m_window.audioProcess.count,
            m_window.audioProcess.wallMs,
            m_window.audioProcess.processCpuMs,
            m_window.audioProcess.threadCpuMs,
            m_window.otherProcess.count,
            m_window.otherProcess.wallMs,
            m_window.otherProcess.processCpuMs,
            m_window.otherProcess.threadCpuMs
        );
    }

} // namespace media::ffmpeg
