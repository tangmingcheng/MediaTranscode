#include "internal/FFmpegAudioStrategyPlanner.h"

#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {

    FFmpegAudioStrategyPlanner::Plan FFmpegAudioStrategyPlanner::plan(
        const TranscodeConfig& config,
        const AVStream* inputAudioStream,
        const AVFormatContext* outputFmtCtx)
    {
        Plan result;

        if (config.audioMode == AudioMode::None) {
            result.mode = AudioMode::None;
            result.diagnostic = "audio disabled by config";
            return result;
        }

        if (!inputAudioStream) {
            result.mode = AudioMode::None;
            result.diagnostic = "no input audio stream";
            return result;
        }

        if (config.audioMode == AudioMode::CopySelected) {
            result.mode = AudioMode::CopySelected;
            result.diagnostic = "audio copy explicitly selected";
            return result;
        }

        if (config.audioMode == AudioMode::EncodeSelected) {
            result.mode = AudioMode::EncodeSelected;
            result.diagnostic = "audio encode explicitly selected";
            return result;
        }

        std::string reason;
        if (canSmartCopy(config, inputAudioStream, outputFmtCtx, &reason)) {
            result.mode = AudioMode::CopySelected;
            result.smartCopy = true;
            result.diagnostic = "smart copy selected: " + reason;
            return result;
        }

        result.mode = AudioMode::EncodeSelected;
        result.smartCopy = false;
        result.diagnostic = "smart copy unavailable: " + reason + "; using audio encode";
        return result;
    }

    const char* FFmpegAudioStrategyPlanner::audioModeName(AudioMode mode)
    {
        switch (mode) {
        case AudioMode::None:
            return "none";
        case AudioMode::Auto:
            return "auto";
        case AudioMode::CopySelected:
            return "copy-selected";
        case AudioMode::EncodeSelected:
            return "encode-selected";
        default:
            return "unknown";
        }
    }

    const char* FFmpegAudioStrategyPlanner::audioCodecName(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::AAC:
            return "aac";
        case AudioCodec::OPUS:
            return "opus";
        case AudioCodec::MP3:
            return "mp3";
        default:
            return "unknown";
        }
    }

    AVCodecID FFmpegAudioStrategyPlanner::targetCodecId(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::AAC:
            return AV_CODEC_ID_AAC;
        case AudioCodec::OPUS:
            return AV_CODEC_ID_OPUS;
        case AudioCodec::MP3:
            return AV_CODEC_ID_MP3;
        default:
            return AV_CODEC_ID_NONE;
        }
    }

    const char* FFmpegAudioStrategyPlanner::codecName(AVCodecID codecId)
    {
        const char* name = avcodec_get_name(codecId);
        return name ? name : "unknown";
    }

    bool FFmpegAudioStrategyPlanner::outputContainerSupportsCodec(
        const AVFormatContext* outputFmtCtx,
        AVCodecID codecId)
    {
        if (!outputFmtCtx || !outputFmtCtx->oformat || codecId == AV_CODEC_ID_NONE) {
            return false;
        }

        const int queryResult = avformat_query_codec(
            outputFmtCtx->oformat,
            codecId,
            FF_COMPLIANCE_NORMAL
        );

        return queryResult > 0;
    }

    bool FFmpegAudioStrategyPlanner::codecRequiresGlobalHeaderForSafeCopy(
        const AVFormatContext* outputFmtCtx,
        AVCodecID codecId)
    {
        if (!outputFmtCtx || !outputFmtCtx->oformat) {
            return false;
        }

        if (!(outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)) {
            return false;
        }

        switch (codecId) {
        case AV_CODEC_ID_AAC:
        case AV_CODEC_ID_OPUS:
            return true;
        default:
            return false;
        }
    }

    bool FFmpegAudioStrategyPlanner::inputHasGlobalHeaderCompatibleExtradata(
        const AVStream* inputAudioStream)
    {
        return inputAudioStream &&
            inputAudioStream->codecpar &&
            inputAudioStream->codecpar->extradata &&
            inputAudioStream->codecpar->extradata_size > 0;
    }

    bool FFmpegAudioStrategyPlanner::canSmartCopy(
        const TranscodeConfig& config,
        const AVStream* inputAudioStream,
        const AVFormatContext* outputFmtCtx,
        std::string* reason)
    {
        auto setReason = [&](const std::string& value) {
            if (reason) {
                *reason = value;
            }
        };

        if (!inputAudioStream || !inputAudioStream->codecpar) {
            setReason("input audio stream is missing codec parameters");
            return false;
        }

        const AVCodecID inputCodecId = inputAudioStream->codecpar->codec_id;
        const AVCodecID requestedCodecId = targetCodecId(config.audioCodec);

        if (requestedCodecId == AV_CODEC_ID_NONE) {
            setReason("requested audio codec is unsupported by planner");
            return false;
        }

        if (inputCodecId != requestedCodecId) {
            std::ostringstream oss;
            oss << "input codec " << codecName(inputCodecId)
                << " does not match requested codec " << codecName(requestedCodecId);
            setReason(oss.str());
            return false;
        }

        if (!outputContainerSupportsCodec(outputFmtCtx, inputCodecId)) {
            std::ostringstream oss;
            oss << "output container "
                << (outputFmtCtx && outputFmtCtx->oformat && outputFmtCtx->oformat->name
                    ? outputFmtCtx->oformat->name
                    : "unknown")
                << " does not report support for codec " << codecName(inputCodecId);
            setReason(oss.str());
            return false;
        }

        if (codecRequiresGlobalHeaderForSafeCopy(outputFmtCtx, inputCodecId) &&
            !inputHasGlobalHeaderCompatibleExtradata(inputAudioStream)) {
            std::ostringstream oss;
            oss << "codec " << codecName(inputCodecId)
                << " requires global header in output container but input extradata is missing";
            setReason(oss.str());
            return false;
        }

        std::ostringstream oss;
        oss << "input codec " << codecName(inputCodecId)
            << " matches requested codec " << codecName(requestedCodecId)
            << " and output container supports copy";
        setReason(oss.str());
        return true;
    }

} // namespace media::ffmpeg
