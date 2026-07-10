#include "internal/graph/nodes/input/RawRtpInputNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputOptions.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <chrono>
#include <fstream>
#include <string>

namespace media::ffmpeg::graph {

RawRtpInputNode::RawRtpInputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RawRtpInputNode")
{
}

MediaNodeKind RawRtpInputNode::staticKind() noexcept
{
    return MediaNodeKind::RawRtpInput;
}

::media::Result<MediaNodeProcessResult> RawRtpInputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted) {
        return processFinished();
    }

    auto status = openInput(context);
    if (!status) {
        return processProgress(status);
    }

    auto buffer = FFmpegBufferFactory::wrapInputFormatContext(std::move(m_context));
    if (!buffer) {
        return ::media::Result<MediaNodeProcessResult>::failure(buffer.error());
    }

    auto pushed = emitOutput(context, "format", buffer.value());
    if (!pushed) {
        return processProgress(pushed);
    }

    m_formatEmitted = true;
    return processProgress();
}

::media::Status RawRtpInputNode::stop(MediaGraphExecutionContext& context)
{
    m_context.reset();
    m_formatEmitted = false;
    cleanupSdpFile();
    return FFmpegNodeRuntime::stop(context);
}

void RawRtpInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_context.reset();
    m_formatEmitted = false;
    cleanupSdpFile();
    FFmpegNodeRuntime::abort(context);
}

::media::Status RawRtpInputNode::openInput(MediaGraphExecutionContext& context)
{
    if (m_context) {
        return ::media::Status::success();
    }

    const MediaNodeOptions* options = nodeOptions(context);
    auto sdpText = requiredNodeOption(options, "RawRtpInputNode", "input.sdp");
    if (!sdpText) {
        return ::media::Status::failure(sdpText.error());
    }
    auto openTimeoutMs = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "input.open_timeout_ms");
    if (!openTimeoutMs) {
        return ::media::Status::failure(openTimeoutMs.error());
    }
    auto readTimeoutMs = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "input.read_timeout_ms");
    if (!readTimeoutMs) {
        return ::media::Status::failure(readTimeoutMs.error());
    }
    auto analyzeDurationUs = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "input.analyze_duration_us");
    if (!analyzeDurationUs) {
        return ::media::Status::failure(analyzeDurationUs.error());
    }
    auto probeSizeBytes = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "input.probe_size_bytes");
    if (!probeSizeBytes) {
        return ::media::Status::failure(probeSizeBytes.error());
    }
    auto lowLatency = requiredBoolNodeOption(options, "RawRtpInputNode", "input.low_latency");
    if (!lowLatency) {
        return ::media::Status::failure(lowLatency.error());
    }

    auto sdpPath = writeSdpFile(sdpText.value());
    if (!sdpPath) {
        return ::media::Status::failure(sdpPath.error());
    }
    m_sdpPath = sdpPath.value();

    FFmpegRealtimeInputOptions realtimeOptions;
    realtimeOptions.openTimeoutMs = openTimeoutMs.value();
    realtimeOptions.readTimeoutMs = readTimeoutMs.value();
    realtimeOptions.analyzeDurationUs = analyzeDurationUs.value();
    realtimeOptions.probeSizeBytes = probeSizeBytes.value();
    realtimeOptions.lowLatency = lowLatency.value();
    realtimeOptions.allowFileUdpRtpProtocols = true;

    AVDictionary* inputOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&inputOptions, realtimeOptions);

    AVFormatContext* raw = nullptr;
    const AVInputFormat* sdpFormat = av_find_input_format("sdp");
    const int openRet = avformat_open_input(&raw, m_sdpPath.string().c_str(), sdpFormat, &inputOptions);
    if (inputOptions) {
        av_dict_free(&inputOptions);
    }
    if (openRet < 0) {
        cleanupSdpFile();
        return FFmpegGraphError::statusFromCode(openRet, "avformat_open_input(raw rtp sdp)");
    }

    m_context.reset(raw);
    const int infoRet = avformat_find_stream_info(m_context.get(), nullptr);
    if (infoRet < 0) {
        m_context.reset();
        cleanupSdpFile();
        return FFmpegGraphError::statusFromCode(infoRet, "avformat_find_stream_info(raw rtp)");
    }

    return ::media::Status::success();
}

::media::Result<std::filesystem::path> RawRtpInputNode::writeSdpFile(const std::string& sdpText) const
{
    if (sdpText.empty()) {
        return ::media::Result<std::filesystem::path>::failure(
            ::media::ErrorInfo::invalidArgument("RawRtpInputNode requires non-empty SDP text"));
    }

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("media_transcode_raw_rtp_" + std::to_string(nodeId().value) + "_" + std::to_string(stamp) + ".sdp");

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return ::media::Result<std::filesystem::path>::failure(
            ::media::ErrorInfo::ioFailure("RawRtpInputNode failed to create temporary SDP file"));
    }
    output.write(sdpText.data(), static_cast<std::streamsize>(sdpText.size()));
    if (!output) {
        return ::media::Result<std::filesystem::path>::failure(
            ::media::ErrorInfo::ioFailure("RawRtpInputNode failed to write temporary SDP file"));
    }
    return ::media::Result<std::filesystem::path>::success(path);
}

void RawRtpInputNode::cleanupSdpFile() noexcept
{
    if (m_sdpPath.empty()) {
        return;
    }
    std::error_code ignored;
    std::filesystem::remove(m_sdpPath, ignored);
    m_sdpPath.clear();
}

} // namespace media::ffmpeg::graph
