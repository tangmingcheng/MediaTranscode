#include "media_transcode_beta/MediaRealtimeBetaSnapshotProjector.h"

#include "media_transcode_beta/MediaRealtimeBetaOwnedConfig.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace media::beta {
namespace {

mt_beta_selected_backend selectedBackend(
    ffmpeg::graph::MediaHardwareDeviceKind kind) noexcept
{
    using Kind = ffmpeg::graph::MediaHardwareDeviceKind;
    switch (kind) {
    case Kind::None: return MT_BETA_BACKEND_NONE;
    case Kind::D3D11VA: return MT_BETA_BACKEND_D3D11VA;
    case Kind::QSV: return MT_BETA_BACKEND_QSV;
    case Kind::CUDA: return MT_BETA_BACKEND_CUDA;
    case Kind::VAAPI: return MT_BETA_BACKEND_VAAPI;
    case Kind::RKMPP: return MT_BETA_BACKEND_RKMPP;
    case Kind::VideoToolbox: return MT_BETA_BACKEND_VIDEOTOOLBOX;
    case Kind::MediaCodec: return MT_BETA_BACKEND_MEDIACODEC;
    case Kind::Unknown:
    case Kind::DRMPrime:
        return MT_BETA_BACKEND_UNKNOWN;
    }
    return MT_BETA_BACKEND_UNKNOWN;
}

mt_beta_selected_filter selectedFilter(
    const ffmpeg::graph::MediaRealtimeVideoPreparedReport& report) noexcept
{
    if (!report.filterActive) {
        return MT_BETA_FILTER_NONE;
    }
    const std::string_view name(report.filterName);
    if (name.starts_with("scale_rkrga=")) {
        return MT_BETA_FILTER_RGA;
    }
    if (name.starts_with("scale_d3d11=") ||
        name.starts_with("scale_qsv=") ||
        name.starts_with("scale_cuda=") ||
        name.starts_with("scale_vaapi=") ||
        name.starts_with("scale_videotoolbox=") ||
        name == "passthrough_d3d11va" || name == "passthrough_qsv" ||
        name == "passthrough_cuda" || name == "passthrough_vaapi" ||
        name == "passthrough_videotoolbox") {
        return MT_BETA_FILTER_HARDWARE;
    }
    return MT_BETA_FILTER_UNKNOWN;
}

::media::Status validateOutputCodec(
    mt_beta_realtime_snapshot& snapshot,
    std::string_view codecName)
{
    if (codecName == "h264") {
        snapshot.output_codec = MT_BETA_VIDEO_CODEC_H264;
        return ::media::Status::success();
    }
    if (codecName == "hevc") {
        snapshot.output_codec = MT_BETA_VIDEO_CODEC_HEVC;
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::unsupported(
        "selected output codec has no Beta representation"));
}

template <typename Destination, typename Source>
::media::Result<Destination> checkedUnsigned(
    Source value,
    const char* field)
{
    if (static_cast<std::uintmax_t>(value) >
        static_cast<std::uintmax_t>(
            std::numeric_limits<Destination>::max())) {
        return ::media::Result<Destination>::failure(
            ::media::ErrorInfo::internalError(
                std::string(field) + " exceeds the Beta snapshot range"));
    }
    return ::media::Result<Destination>::success(
        static_cast<Destination>(value));
}

} // namespace

mt_beta_realtime_snapshot MediaRealtimeBetaSnapshotProjector::initial(
    const MediaRealtimeBetaOwnedConfig& config) noexcept
{
    mt_beta_realtime_snapshot snapshot{};
    snapshot.state = MT_BETA_REALTIME_STARTING;
    snapshot.completion_reason = MT_BETA_COMPLETION_NONE;
    snapshot.selected_backend = MT_BETA_BACKEND_UNKNOWN;
    snapshot.input_codec = config.inputCodec();
    snapshot.output_codec = config.outputCodec();
    snapshot.selected_filter = MT_BETA_FILTER_UNKNOWN;
    return snapshot;
}

::media::Status MediaRealtimeBetaSnapshotProjector::projectPrepared(
    mt_beta_realtime_snapshot& snapshot,
    const ffmpeg::graph::MediaRealtimeVideoPreparedReport& report)
{
    auto codec = validateOutputCodec(snapshot, report.outputCodecName);
    if (!codec) {
        return codec;
    }
    snapshot.selected_backend = selectedBackend(report.hardwareDeviceKind);
    snapshot.selected_filter = selectedFilter(report);
    snapshot.zero_copy_planned = report.zeroCopyPlanned ? 1U : 0U;
    return ::media::Status::success();
}

::media::Status MediaRealtimeBetaSnapshotProjector::projectRuntime(
    mt_beta_realtime_snapshot& snapshot,
    const ffmpeg::graph::MediaGraphRuntimeReport& report,
    std::chrono::milliseconds runningTime)
{
    if (runningTime.count() < 0) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "Beta running time cannot be negative"));
    }
    auto runningTimeMs = checkedUnsigned<std::uint64_t>(
        static_cast<std::uintmax_t>(runningTime.count()), "running_time_ms");
    auto queuedBuffers = checkedUnsigned<std::uint64_t>(
        report.metrics.queuedBuffers, "queued_buffers");
    auto peakQueuedBuffers = checkedUnsigned<std::uint64_t>(
        report.metrics.peakQueuedBuffers, "peak_queued_buffers");
    auto logicalProcessors = checkedUnsigned<std::uint32_t>(
        report.metrics.logicalProcessorCount, "logical_processor_count");
    if (!runningTimeMs) return ::media::Status::failure(runningTimeMs.error());
    if (!queuedBuffers) return ::media::Status::failure(queuedBuffers.error());
    if (!peakQueuedBuffers) {
        return ::media::Status::failure(peakQueuedBuffers.error());
    }
    if (!logicalProcessors) {
        return ::media::Status::failure(logicalProcessors.error());
    }

    snapshot.running_time_ms = runningTimeMs.value();
    snapshot.queued_buffers = queuedBuffers.value();
    snapshot.peak_queued_buffers = peakQueuedBuffers.value();
    snapshot.worker_progress = report.metrics.workerProgress;
    snapshot.worker_process_calls = report.metrics.workerProcessCalls;
    snapshot.worker_waits = report.metrics.workerWaits;
    snapshot.worker_wakeups = report.metrics.workerWakeups;
    snapshot.worker_errors = report.metrics.workerErrors;
    snapshot.stalled_intervals = report.metrics.stalledIntervals;
    snapshot.total_pushed = report.metrics.totalPushed;
    snapshot.total_popped = report.metrics.totalPopped;
    snapshot.dropped_buffers = report.metrics.droppedBuffers;
    snapshot.encoded_packets_pushed = report.metrics.encodedPacketsPushed;
    snapshot.encoded_packets_popped = report.metrics.encodedPacketsPopped;
    snapshot.working_set_bytes = report.metrics.workingSetBytes;
    snapshot.peak_working_set_bytes = report.metrics.peakWorkingSetBytes;
    snapshot.logical_processor_count = logicalProcessors.value();
    snapshot.average_process_machine_cpu_percent =
        report.metrics.averageProcessMachineCpuPercent;
    snapshot.peak_process_machine_cpu_percent =
        report.metrics.peakProcessMachineCpuPercent;
    snapshot.average_process_single_core_cpu_percent =
        report.metrics.averageProcessSingleCoreCpuPercent;
    snapshot.peak_process_single_core_cpu_percent =
        report.metrics.peakProcessSingleCoreCpuPercent;
    return ::media::Status::success();
}

} // namespace media::beta
