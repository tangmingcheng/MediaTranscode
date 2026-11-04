#pragma once
#include <cstdint>
#include <optional>
#include <ostream>

namespace media::ffmpeg::graph {
// One batch, copied into the enclosing compose record; never a separate maximum.
struct MediaCanvasBatchTiming final {
    std::uint32_t process = 0, thread = 0;
    std::uintptr_t context = 0, stream = 0;
    std::int64_t submitBegin = 0, submitEnd = 0, waitBegin = 0, waitEnd = 0;
    const char* preparation = "not_prepared";
    std::optional<int> preparationResult, startResult, endResult, completionResult, elapsedResult;
    std::optional<float> gpuMilliseconds;
    void write(std::ostream& out) const {
        out << " batch_pid=" << process << " batch_tid=" << thread
            << " batch_context=" << context << " batch_stream=" << stream
            << " batch_submit_begin_steady_ns=" << submitBegin << " batch_submit_end_steady_ns=" << submitEnd
            << " batch_wait_begin_steady_ns=" << waitBegin << " batch_wait_end_steady_ns=" << waitEnd
            << " batch_timing_preparation=" << preparation;
        const auto code = [&](const char* name, const std::optional<int>& value) {
            out << ' ' << name << '=';
            if (value) out << *value; else out << "unavailable";
        };
        code("batch_timing_preparation_native", preparationResult);
        code("batch_start_event_native", startResult);
        code("batch_end_event_native", endResult);
        code("batch_sync_native", completionResult);
        code("batch_elapsed_native", elapsedResult);
        out << " batch_gpu_elapsed_available=" << gpuMilliseconds.has_value() << " batch_gpu_elapsed_ms=";
        if (gpuMilliseconds) out << *gpuMilliseconds; else out << "unavailable";
        out << " batch_gpu_interval=event_execution_interval_excludes_pre_start_queue";
    }
};
}
