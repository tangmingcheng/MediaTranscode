#pragma once

#include "internal/FFmpegError.h"
#include "internal/output/AudioOutputStreamProvider.h"
#include "internal/output/PacketOutputNode.h"
#include "internal/output/VideoOutputStreamProvider.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

class FFmpegMuxerOutputNode final : public PacketOutputNode,
                                    public VideoOutputStreamProvider,
                                    public AudioOutputStreamProvider {
public:
    struct Config {
        AVFormatContext* outputFmtCtx = nullptr;
    };

    FFmpegMuxerOutputNode() = default;
    ~FFmpegMuxerOutputNode() override = default;

    FFmpegMuxerOutputNode(const FFmpegMuxerOutputNode&) = delete;
    FFmpegMuxerOutputNode& operator=(const FFmpegMuxerOutputNode&) = delete;

    void reset()
    {
        m_outputFmtCtx = nullptr;
    }

    Status initialize(const Config& config)
    {
        reset();

        if (!config.outputFmtCtx) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode initialize failed: outputFmtCtx is null"));
        }

        if (!config.outputFmtCtx->oformat) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode initialize failed: output format is null"));
        }

        m_outputFmtCtx = config.outputFmtCtx;
        return Status::success();
    }

    bool requiresGlobalHeader() const override
    {
        return m_outputFmtCtx &&
            m_outputFmtCtx->oformat &&
            (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER);
    }

    Result<AVStream*> createVideoStream(AVCodecContext* encoderCtx) override
    {
        if (!m_outputFmtCtx) {
            return Result<AVStream*>::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerOutputNode createVideoStream failed: not initialized"));
        }

        if (!encoderCtx) {
            return Result<AVStream*>::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode createVideoStream failed: encoderCtx is null"));
        }

        AVStream* stream = avformat_new_stream(m_outputFmtCtx, nullptr);
        if (!stream) {
            return Result<AVStream*>::failure(makeAllocationError(
                "avformat_new_stream video failed"));
        }

        stream->time_base = encoderCtx->time_base;

        const int ret = avcodec_parameters_from_context(stream->codecpar, encoderCtx);
        if (ret < 0) {
            return Result<AVStream*>::failure(makeFFmpegError(
                "avcodec_parameters_from_context video failed", ret));
        }

        stream->codecpar->codec_tag = 0;
        return Result<AVStream*>::success(stream);
    }

    Result<AVStream*> createAudioCopyStream(AVStream* inputAudioStream) override
    {
        if (!m_outputFmtCtx) {
            return Result<AVStream*>::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerOutputNode createAudioCopyStream failed: not initialized"));
        }

        if (!inputAudioStream) {
            return Result<AVStream*>::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode createAudioCopyStream failed: inputAudioStream is null"));
        }

        AVStream* stream = avformat_new_stream(m_outputFmtCtx, nullptr);
        if (!stream) {
            return Result<AVStream*>::failure(makeAllocationError(
                "avformat_new_stream audio failed"));
        }

        const int ret = avcodec_parameters_copy(stream->codecpar, inputAudioStream->codecpar);
        if (ret < 0) {
            return Result<AVStream*>::failure(makeFFmpegError(
                "avcodec_parameters_copy audio failed", ret));
        }

        stream->codecpar->codec_tag = 0;
        stream->time_base = inputAudioStream->time_base;
        return Result<AVStream*>::success(stream);
    }

    Result<AVStream*> createEncodedAudioStream(AVCodecContext* encoderCtx) override
    {
        if (!m_outputFmtCtx) {
            return Result<AVStream*>::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerOutputNode createEncodedAudioStream failed: not initialized"));
        }

        if (!encoderCtx) {
            return Result<AVStream*>::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode createEncodedAudioStream failed: encoderCtx is null"));
        }

        AVStream* stream = avformat_new_stream(m_outputFmtCtx, nullptr);
        if (!stream) {
            return Result<AVStream*>::failure(makeAllocationError(
                "avformat_new_stream encoded audio failed"));
        }

        stream->time_base = encoderCtx->time_base;

        const int ret = avcodec_parameters_from_context(stream->codecpar, encoderCtx);
        if (ret < 0) {
            return Result<AVStream*>::failure(makeFFmpegError(
                "avcodec_parameters_from_context audio failed", ret));
        }

        stream->codecpar->codec_tag = 0;
        return Result<AVStream*>::success(stream);
    }

    Status pushPacket(AVPacket* packet) override
    {
        if (!m_outputFmtCtx) {
            return Status::failure(ErrorInfo::notInitialized(
                "FFmpegMuxerOutputNode pushPacket failed: not initialized"));
        }

        if (!packet) {
            return Status::failure(ErrorInfo::invalidArgument(
                "FFmpegMuxerOutputNode pushPacket failed: packet is null"));
        }

        const int ret = av_interleaved_write_frame(m_outputFmtCtx, packet);
        if (ret < 0) {
            return Status::failure(makeFFmpegError("packet write failed", ret));
        }

        return Status::success();
    }

private:
    AVFormatContext* m_outputFmtCtx = nullptr;
};

} // namespace media::ffmpeg
