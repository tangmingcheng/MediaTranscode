#include "internal/FFmpegAudioStrategyPlanner.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace media::ffmpeg {
namespace {

    int64_t inputBitRate(const AVStream* stream)
    {
        if (!stream || !stream->codecpar) {
            return 0;
        }

        return stream->codecpar->bit_rate > 0 ? stream->codecpar->bit_rate : 0;
    }

    int64_t bitRateTolerance(int64_t reference)
    {
        if (reference <= 0) {
            return 0;
        }

        return std::max<int64_t>(1000, reference / 100); // 1% or 1kbps, whichever is larger.
    }

} // namespace

    FFmpegAudioStrategyPlanner::Plan FFmpegAudioStrategyPlanner::plan(
        const TranscodeConfig& config,
        const AVStream* inputAudioStream,
        const AVFormatContext* outputFmtCtx)
    {
        Plan result;

        if (!config.audioEnabled) {
            result.mode = FFmpegAudioPipelineMode::None;
            result.diagnostic = "audio disabled by config";
            return result;
        }

        if (!inputAudioStream) {
            result.mode = FFmpegAudioPipelineMode::None;
            result.diagnostic = "no input audio stream";
            return result;
        }

        const TargetParameters target = resolveTargetParameters(config, inputAudioStream);
        result.codec = target.encodeCodec;
        result.audioBitrateKbps = plannedBitrateKbps(target);

        std::string reason;
        if (canSmartCopy(config, inputAudioStream, outputFmtCtx, target, &reason)) {
            result.mode = FFmpegAudioPipelineMode::Copy;
            result.smartCopy = true;
            result.diagnostic = "smart copy selected: " + reason;
            return result;
        }

        result.mode = FFmpegAudioPipelineMode::Encode;
        result.smartCopy = false;
        result.diagnostic = "smart copy unavailable: " + reason + "; using audio encode";
        return result;
    }

    const char* FFmpegAudioStrategyPlanner::audioCodecName(AudioCodec codec)
    {
        switch (codec) {
        case AudioCodec::Auto:
            return "auto";
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

    FFmpegAudioStrategyPlanner::TargetParameters
    FFmpegAudioStrategyPlanner::resolveTargetParameters(
        const TranscodeConfig& config,
        const AVStream* inputAudioStream)
    {
        TargetParameters target;

        const AVCodecID inputCodecId = inputAudioStream && inputAudioStream->codecpar
            ? inputAudioStream->codecpar->codec_id
            : AV_CODEC_ID_NONE;

        if (config.audioCodec == AudioCodec::Auto) {
            target.codecId = inputCodecId;
        }
        else {
            target.codecId = targetCodecId(config.audioCodec);
        }

        target.encodeCodec = fallbackEncodeCodec(config, inputAudioStream);

        if (config.audioBitrateKbps > 0) {
            target.bitRateSpecified = true;
            target.bitRate = static_cast<int64_t>(config.audioBitrateKbps) * 1000;
        }
        else {
            target.bitRateSpecified = false;
            target.bitRate = inputBitRate(inputAudioStream);
        }

        return target;
    }

    int FFmpegAudioStrategyPlanner::plannedBitrateKbps(const TargetParameters& target)
    {
        if (target.bitRate > 0) {
            return static_cast<int>(std::max<int64_t>(32, target.bitRate / 1000));
        }

        return 128;
    }

    AudioCodec FFmpegAudioStrategyPlanner::audioCodecFromCodecId(AVCodecID codecId)
    {
        switch (codecId) {
        case AV_CODEC_ID_AAC:
            return AudioCodec::AAC;
        case AV_CODEC_ID_OPUS:
            return AudioCodec::OPUS;
        case AV_CODEC_ID_MP3:
            return AudioCodec::MP3;
        default:
            return AudioCodec::Auto;
        }
    }

    AudioCodec FFmpegAudioStrategyPlanner::fallbackEncodeCodec(
        const TranscodeConfig& config,
        const AVStream* inputAudioStream)
    {
        if (config.audioCodec != AudioCodec::Auto) {
            return config.audioCodec;
        }

        const AVCodecID inputCodecId = inputAudioStream && inputAudioStream->codecpar
            ? inputAudioStream->codecpar->codec_id
            : AV_CODEC_ID_NONE;

        const AudioCodec matchingInputCodec = audioCodecFromCodecId(inputCodecId);
        if (matchingInputCodec != AudioCodec::Auto) {
            return matchingInputCodec;
        }

        return AudioCodec::AAC;
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
        case AudioCodec::Auto:
        default:
            return AV_CODEC_ID_NONE;
        }
    }

    const char* FFmpegAudioStrategyPlanner::codecName(AVCodecID codecId)
    {
        const char* name = avcodec_get_name(codecId);
        return name ? name : "unknown";
    }

    bool FFmpegAudioStrategyPlanner::bitRateMatches(
        const AVStream* inputAudioStream,
        const TargetParameters& target,
        std::string* reason)
    {
        auto setReason = [&](const std::string& value) {
            if (reason) {
                *reason = value;
            }
        };

        if (!target.bitRateSpecified) {
            setReason("target bitrate is auto, keeping input bitrate");
            return true;
        }

        const int64_t inputRate = inputBitRate(inputAudioStream);
        if (inputRate <= 0) {
            setReason("target bitrate is specified but input bitrate is unknown");
            return false;
        }

        const int64_t diff = std::llabs(target.bitRate - inputRate);
        const int64_t tolerance = bitRateTolerance(inputRate);
        if (diff <= tolerance) {
            std::ostringstream oss;
            oss << "target bitrate " << target.bitRate
                << " is effectively equal to input bitrate " << inputRate;
            setReason(oss.str());
            return true;
        }

        std::ostringstream oss;
        oss << "target bitrate " << target.bitRate
            << " differs from input bitrate " << inputRate;
        setReason(oss.str());
        return false;
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
        const TranscodeConfig& /*config*/,
        const AVStream* inputAudioStream,
        const AVFormatContext* outputFmtCtx,
        const TargetParameters& target,
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

        if (target.codecId == AV_CODEC_ID_NONE) {
            setReason("target audio codec is unknown");
            return false;
        }

        if (inputCodecId != target.codecId) {
            std::ostringstream oss;
            oss << "target codec " << codecName(target.codecId)
                << " differs from input codec " << codecName(inputCodecId);
            setReason(oss.str());
            return false;
        }

        std::string bitrateReason;
        if (!bitRateMatches(inputAudioStream, target, &bitrateReason)) {
            setReason(bitrateReason);
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
        oss << "target audio parameters match input: codec=" << codecName(inputCodecId)
            << ", " << bitrateReason
            << ", output container supports copy";
        setReason(oss.str());
        return true;
    }

} // namespace media::ffmpeg
