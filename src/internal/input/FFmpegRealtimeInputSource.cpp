#include "internal/input/FFmpegRealtimeInputSource.h"

#include "internal/FFmpegError.h"
#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavutil/dict.h>
#include <libavutil/error.h>
}

namespace media::ffmpeg {
namespace {

class DictionaryGuard {
public:
    ~DictionaryGuard()
    {
        av_dict_free(&m_dict);
    }

    DictionaryGuard(const DictionaryGuard&) = delete;
    DictionaryGuard& operator=(const DictionaryGuard&) = delete;

    DictionaryGuard() = default;

    AVDictionary** ref()
    {
        return &m_dict;
    }

private:
    AVDictionary* m_dict = nullptr;
};

void setOptionIfPositive(AVDictionary** options, const char* key, int value)
{
    if (value > 0) {
        av_dict_set_int(options, key, value, 0);
    }
}

void closeRawInputContext(AVFormatContext*& ctx)
{
    if (ctx) {
        avformat_close_input(&ctx);
    }
}

} // namespace

FFmpegRealtimeInputSource::~FFmpegRealtimeInputSource()
{
    close();
}

Status FFmpegRealtimeInputSource::open(const Config& config)
{
    close();

    const Status validation = validateConfig(config);
    if (!validation) {
        return validation;
    }

    m_config = config;
    m_interruptController.reset();

    AVFormatContext* rawContext = avformat_alloc_context();
    if (!rawContext) {
        return Status::failure(makeAllocationError(
            "avformat_alloc_context realtime input failed"));
    }

    rawContext->interrupt_callback.callback = &FFmpegRealtimeInterruptController::callback;
    rawContext->interrupt_callback.opaque = &m_interruptController;

    decltype(av_find_input_format("")) inputFormat = nullptr;
    if (!m_config.inputFormatHint.empty()) {
        inputFormat = av_find_input_format(m_config.inputFormatHint.c_str());
        if (!inputFormat) {
            closeRawInputContext(rawContext);
            return Status::failure(ErrorInfo::unsupported(
                "realtime input open failed: unknown input format hint: " + m_config.inputFormatHint));
        }
    }

    DictionaryGuard options;
    applyInputOptions(options.ref());

    spdlog::info(
        "[REALTIME][INPUT] opening: url={}, format_hint={}, open_timeout_ms={}, read_timeout_ms={}, analyzeduration_us={}, probesize={}",
        m_config.inputUrl,
        m_config.inputFormatHint.empty() ? "auto" : m_config.inputFormatHint,
        m_config.openTimeoutMs,
        m_config.readTimeoutMs,
        m_config.analyzeDurationUs,
        m_config.probeSizeBytes
    );

    m_interruptController.beginOperation(m_config.openTimeoutMs);
    const int ret = avformat_open_input(
        &rawContext,
        m_config.inputUrl.c_str(),
        inputFormat,
        options.ref()
    );
    m_interruptController.endOperation();

    if (ret < 0) {
        closeRawInputContext(rawContext);
        if (interruptedByRequest()) {
            return Status::failure(ErrorInfo::ioFailure(
                "realtime input open interrupted by stop request", ret));
        }
        return Status::failure(makeFFmpegError("avformat_open_input realtime input failed", ret));
    }

    m_formatContext.reset(rawContext);
    spdlog::info("[REALTIME][INPUT] opened");
    return Status::success();
}

Status FFmpegRealtimeInputSource::findStreamInfo()
{
    if (!m_formatContext) {
        return Status::failure(ErrorInfo::notInitialized(
            "realtime input findStreamInfo failed: input is not open"));
    }

    m_interruptController.beginOperation(m_config.openTimeoutMs);
    const int ret = avformat_find_stream_info(m_formatContext.get(), nullptr);
    m_interruptController.endOperation();

    if (ret < 0) {
        if (interruptedByRequest()) {
            return Status::failure(ErrorInfo::ioFailure(
                "realtime input findStreamInfo interrupted by stop request", ret));
        }
        return Status::failure(makeFFmpegError(
            "avformat_find_stream_info realtime input failed", ret));
    }

    return findBestStreams();
}

Result<RealtimeInputReadState> FFmpegRealtimeInputSource::readPacket(AVPacket* packet)
{
    if (!m_formatContext) {
        return Result<RealtimeInputReadState>::failure(ErrorInfo::notInitialized(
            "realtime input readPacket failed: input is not open"));
    }

    if (!packet) {
        return Result<RealtimeInputReadState>::failure(ErrorInfo::invalidArgument(
            "realtime input readPacket failed: packet is null"));
    }

    m_interruptController.beginOperation(m_config.readTimeoutMs);
    const int ret = av_read_frame(m_formatContext.get(), packet);
    m_interruptController.endOperation();

    if (ret >= 0) {
        return Result<RealtimeInputReadState>::success(RealtimeInputReadState::Packet);
    }

    if (ret == AVERROR(EAGAIN)) {
        return Result<RealtimeInputReadState>::success(RealtimeInputReadState::TryAgain);
    }

    if (ret == AVERROR_EOF) {
        return Result<RealtimeInputReadState>::success(RealtimeInputReadState::EndOfStream);
    }

    if (ret == AVERROR_EXIT || interruptedByRequest()) {
        return Result<RealtimeInputReadState>::success(RealtimeInputReadState::Interrupted);
    }

    return Result<RealtimeInputReadState>::failure(makeFFmpegError(
        "av_read_frame realtime input failed", ret));
}

void FFmpegRealtimeInputSource::requestInterrupt()
{
    m_interruptController.requestInterrupt();
}

void FFmpegRealtimeInputSource::close()
{
    m_interruptController.requestInterrupt();
    m_formatContext.reset();
    m_videoStream = nullptr;
    m_audioStream = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_interruptController.reset();
}

bool FFmpegRealtimeInputSource::isOpen() const
{
    return m_formatContext != nullptr;
}

bool FFmpegRealtimeInputSource::isVideoPacket(const AVPacket* packet) const
{
    return packet && packet->stream_index == m_videoStreamIndex;
}

bool FFmpegRealtimeInputSource::isAudioPacket(const AVPacket* packet) const
{
    return packet && packet->stream_index == m_audioStreamIndex;
}

AVFormatContext* FFmpegRealtimeInputSource::formatContext() const
{
    return m_formatContext.get();
}

AVStream* FFmpegRealtimeInputSource::videoStream() const
{
    return m_videoStream;
}

AVStream* FFmpegRealtimeInputSource::audioStream() const
{
    return m_audioStream;
}

int FFmpegRealtimeInputSource::videoStreamIndex() const
{
    return m_videoStreamIndex;
}

int FFmpegRealtimeInputSource::audioStreamIndex() const
{
    return m_audioStreamIndex;
}

Status FFmpegRealtimeInputSource::validateConfig(const Config& config) const
{
    if (config.inputUrl.empty()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime input config is invalid: inputUrl is empty"));
    }

    if (config.openTimeoutMs < 0 || config.readTimeoutMs < 0 ||
        config.analyzeDurationUs < 0 || config.probeSizeBytes < 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "realtime input config is invalid: timeout, analyzeduration and probesize must be greater than or equal to 0"));
    }

    return Status::success();
}

Status FFmpegRealtimeInputSource::findBestStreams()
{
    const int videoIndex = av_find_best_stream(
        m_formatContext.get(),
        AVMEDIA_TYPE_VIDEO,
        -1,
        -1,
        nullptr,
        0
    );

    if (videoIndex < 0) {
        return Status::failure(makeFFmpegError(
            "av_find_best_stream realtime video failed", videoIndex));
    }

    m_videoStreamIndex = videoIndex;
    m_videoStream = m_formatContext->streams[m_videoStreamIndex];

    const int audioIndex = av_find_best_stream(
        m_formatContext.get(),
        AVMEDIA_TYPE_AUDIO,
        -1,
        -1,
        nullptr,
        0
    );

    if (audioIndex >= 0) {
        m_audioStreamIndex = audioIndex;
        m_audioStream = m_formatContext->streams[m_audioStreamIndex];
    }
    else {
        m_audioStreamIndex = -1;
        m_audioStream = nullptr;
    }

    spdlog::info(
        "[REALTIME][INPUT] streams: video_index={}, audio_index={}",
        m_videoStreamIndex,
        m_audioStreamIndex
    );

    return Status::success();
}

void FFmpegRealtimeInputSource::applyInputOptions(AVDictionary** options) const
{
    setOptionIfPositive(options, "probesize", m_config.probeSizeBytes);
    setOptionIfPositive(options, "analyzeduration", m_config.analyzeDurationUs);

    if (m_config.readTimeoutMs > 0) {
        const int64_t timeoutUs = static_cast<int64_t>(m_config.readTimeoutMs) * 1000;
        av_dict_set_int(options, "timeout", timeoutUs, 0);
        av_dict_set_int(options, "stimeout", timeoutUs, 0);
        av_dict_set_int(options, "rw_timeout", timeoutUs, 0);
    }

    if (m_config.lowLatency) {
        av_dict_set(options, "fflags", "nobuffer", 0);
        av_dict_set(options, "flags", "low_delay", 0);
        av_dict_set(options, "max_delay", "0", 0);
    }
}

bool FFmpegRealtimeInputSource::interruptedByRequest() const
{
    return m_interruptController.interruptRequested();
}

} // namespace media::ffmpeg
