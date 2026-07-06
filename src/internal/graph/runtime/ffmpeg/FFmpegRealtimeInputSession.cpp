#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputSession.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
}

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

std::string makeTempSdpPath()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream name;
    name << "media_transcode_rtp_input_" << stamp << ".sdp";
    return (std::filesystem::temp_directory_path() / name.str()).string();
}

::media::Result<std::string> writeTempSdp(const std::string& sdpText)
{
    const std::string path = makeTempSdpPath();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return ::media::Result<std::string>::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegRealtimeInputSession failed to create temporary SDP"));
    }
    out << sdpText;
    return ::media::Result<std::string>::success(path);
}

} // namespace

FFmpegRealtimeInputSession::~FFmpegRealtimeInputSession()
{
    close();
}

::media::Status FFmpegRealtimeInputSession::open(FFmpegRealtimeInputSessionOptions options)
{
    close();
    m_interrupted = false;
    m_options = std::move(options);

    if (!m_options.sdpText.empty() && m_options.sdpPath.empty()) {
        auto temp = writeTempSdp(m_options.sdpText);
        if (!temp) {
            return ::media::Status::failure(temp.error());
        }
        m_tempSdpPath = temp.value();
        m_options.sdpPath = m_tempSdpPath;
    }

    const std::string openUrl = !m_options.sdpPath.empty() ? m_options.sdpPath : m_options.url;
    if (openUrl.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegRealtimeInputSession requires URL or SDP path"));
    }

    AVFormatContext* raw = avformat_alloc_context();
    if (!raw) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("avformat_alloc_context"));
    }

    raw->interrupt_callback.callback = &FFmpegRealtimeInputSession::interruptCallback;
    raw->interrupt_callback.opaque = this;

    AVDictionary* dictionary = nullptr;
    if (m_options.readTimeoutMs > 0) {
        av_dict_set_int(&dictionary, "timeout", static_cast<int64_t>(m_options.readTimeoutMs) * 1000, 0);
        av_dict_set_int(&dictionary, "rw_timeout", static_cast<int64_t>(m_options.readTimeoutMs) * 1000, 0);
    }
    if (!m_options.sdpPath.empty()) {
        av_dict_set(&dictionary, "protocol_whitelist", "file,udp,rtp,tcp", 0);
    }

    AVFormatContext* openContext = raw;
    const int openRet = avformat_open_input(&openContext, openUrl.c_str(), nullptr, &dictionary);
    av_dict_free(&dictionary);
    if (openRet < 0) {
        avformat_free_context(raw);
        return FFmpegGraphError::statusFromCode(openRet, "avformat_open_input(realtime)");
    }

    m_context.reset(openContext);
    const int infoRet = avformat_find_stream_info(m_context.get(), nullptr);
    if (infoRet < 0) {
        close();
        return FFmpegGraphError::statusFromCode(infoRet, "avformat_find_stream_info(realtime)");
    }

    return ::media::Status::success();
}

void FFmpegRealtimeInputSession::close() noexcept
{
    m_context.reset();
    if (!m_tempSdpPath.empty()) {
        std::error_code ignored;
        std::filesystem::remove(m_tempSdpPath, ignored);
        m_tempSdpPath.clear();
    }
}

void FFmpegRealtimeInputSession::interrupt() noexcept
{
    m_interrupted = true;
}

AVFormatContext* FFmpegRealtimeInputSession::context() noexcept
{
    return m_context.get();
}

const AVFormatContext* FFmpegRealtimeInputSession::context() const noexcept
{
    return m_context.get();
}

::media::ffmpeg::InputFormatContextPtr FFmpegRealtimeInputSession::takeContext() noexcept
{
    return std::move(m_context);
}

int FFmpegRealtimeInputSession::interruptCallback(void* opaque) noexcept
{
    auto* self = static_cast<FFmpegRealtimeInputSession*>(opaque);
    return self && self->m_interrupted ? 1 : 0;
}

} // namespace media::ffmpeg::graph
