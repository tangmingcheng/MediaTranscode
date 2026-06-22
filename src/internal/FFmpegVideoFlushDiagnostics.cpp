#include "internal/FFmpegVideoFlushDiagnostics.h"

#include "spdlog/spdlog.h"

namespace media::ffmpeg {

    FFmpegVideoFlushDiagnostics::Session::Session(Context context)
        : m_context(context)
        , m_startedAt(Clock::now())
    {
        spdlog::info(
            "[FLUSH][VIDEO][{}] begin mode={}, backend={}, zero_copy={}, packets_before={}",
            m_context.phase ? m_context.phase : "unknown",
            FFmpegVideoFlushDiagnostics::executionModeName(m_context.executionMode),
            m_context.backendName ? m_context.backendName : "none",
            m_context.zeroCopy,
            m_context.packetsBefore
        );
    }

    FFmpegVideoFlushDiagnostics::TimePoint FFmpegVideoFlushDiagnostics::Session::mark() const
    {
        return Clock::now();
    }

    void FFmpegVideoFlushDiagnostics::Session::logStep(const char* stepName,
                                                       TimePoint startedAt) const
    {
        spdlog::info(
            "[FLUSH][VIDEO][{}] {} cost={}ms",
            m_context.phase ? m_context.phase : "unknown",
            stepName ? stepName : "step",
            FFmpegVideoFlushDiagnostics::elapsedMs(startedAt)
        );
    }

    void FFmpegVideoFlushDiagnostics::Session::logStepPackets(const char* stepName,
                                                              TimePoint startedAt,
                                                              int64_t packetsWritten) const
    {
        spdlog::info(
            "[FLUSH][VIDEO][{}] {} cost={}ms, packets_written={}",
            m_context.phase ? m_context.phase : "unknown",
            stepName ? stepName : "step",
            FFmpegVideoFlushDiagnostics::elapsedMs(startedAt),
            packetsWritten
        );
    }

    void FFmpegVideoFlushDiagnostics::Session::logFailure(const char* stepName,
                                                          TimePoint startedAt) const
    {
        spdlog::warn(
            "[FLUSH][VIDEO][{}] {} failed cost={}ms",
            m_context.phase ? m_context.phase : "unknown",
            stepName ? stepName : "step",
            FFmpegVideoFlushDiagnostics::elapsedMs(startedAt)
        );
    }

    void FFmpegVideoFlushDiagnostics::Session::finish(int64_t packetsAfter, bool success) const
    {
        spdlog::info(
            "[FLUSH][VIDEO][{}] end cost={}ms, packets_written={}, packets_total={}, status={}",
            m_context.phase ? m_context.phase : "unknown",
            FFmpegVideoFlushDiagnostics::elapsedMs(m_startedAt),
            packetsAfter - m_context.packetsBefore,
            packetsAfter,
            success ? "ok" : "failed"
        );
    }

    FFmpegVideoFlushDiagnostics::Context FFmpegVideoFlushDiagnostics::makeContext(
        const char* phase,
        bool hasHardwarePlan,
        const HardwarePipelinePlan& hardwarePlan,
        bool zeroCopy,
        int64_t packetsBefore)
    {
        Context context;
        context.phase = phase ? phase : "unknown";
        context.executionMode = hasHardwarePlan
            ? hardwarePlan.executionMode
            : VideoExecutionMode::Cpu;
        context.backendName = hasHardwarePlan && hardwarePlan.backend.name
            ? hardwarePlan.backend.name
            : "none";
        context.zeroCopy = zeroCopy;
        context.packetsBefore = packetsBefore;
        return context;
    }

    const char* FFmpegVideoFlushDiagnostics::executionModeName(VideoExecutionMode mode)
    {
        switch (mode) {
        case VideoExecutionMode::Cpu:
            return "cpu";
        case VideoExecutionMode::HardwareZeroCopy:
            return "hardware-zero-copy";
        case VideoExecutionMode::HardwareDecodeSoftwareFilterHardwareEncode:
            return "hardware-decode-software-filter-hardware-encode";
        case VideoExecutionMode::HardwareDecodeSoftwareFilterGenericEncode:
            return "hardware-decode-software-filter-generic-encode";
        default:
            return "unknown";
        }
    }

    int64_t FFmpegVideoFlushDiagnostics::elapsedMs(TimePoint startedAt)
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - startedAt
        ).count();
    }

} // namespace media::ffmpeg
