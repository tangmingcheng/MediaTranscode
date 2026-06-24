#include "media_transcode/MediaTranscode.h"

#include "media_transcode/FFmpegTranscoder.h"
#include "media_transcode/MediaTranscodeTypes.h"

#include <utility>

namespace media {
namespace {

    VideoCodec toInternalVideoCodec(OutputVideoCodec codec)
    {
        switch (codec) {
        case OutputVideoCodec::H264:
            return VideoCodec::H264;
        case OutputVideoCodec::H265:
            return VideoCodec::H265;
        case OutputVideoCodec::MPEG4:
            return VideoCodec::MPEG4;
        case OutputVideoCodec::VP8:
            return VideoCodec::VP8;
        case OutputVideoCodec::VP9:
            return VideoCodec::VP9;
        case OutputVideoCodec::AV1:
            return VideoCodec::AV1;
        default:
            return VideoCodec::H264;
        }
    }

    VideoRateControlMode toInternalRcMode(VideoRcMode mode)
    {
        switch (mode) {
        case VideoRcMode::CBR:
            return VideoRateControlMode::CBR;
        case VideoRcMode::VBR:
            return VideoRateControlMode::VBR;
        case VideoRcMode::CRF:
            return VideoRateControlMode::CRF;
        case VideoRcMode::CappedVBR:
            return VideoRateControlMode::CappedVBR;
        case VideoRcMode::Auto:
        default:
            return VideoRateControlMode::Auto;
        }
    }

    VideoEncodeSpeedPreset toInternalSpeed(VideoSpeedPreset speed)
    {
        switch (speed) {
        case VideoSpeedPreset::Ultrafast:
            return VideoEncodeSpeedPreset::Ultrafast;
        case VideoSpeedPreset::Superfast:
            return VideoEncodeSpeedPreset::Superfast;
        case VideoSpeedPreset::Veryfast:
            return VideoEncodeSpeedPreset::Veryfast;
        case VideoSpeedPreset::Faster:
            return VideoEncodeSpeedPreset::Faster;
        case VideoSpeedPreset::Fast:
            return VideoEncodeSpeedPreset::Fast;
        case VideoSpeedPreset::Slow:
            return VideoEncodeSpeedPreset::Slow;
        case VideoSpeedPreset::Slower:
            return VideoEncodeSpeedPreset::Slower;
        case VideoSpeedPreset::Veryslow:
            return VideoEncodeSpeedPreset::Veryslow;
        case VideoSpeedPreset::Placebo:
            return VideoEncodeSpeedPreset::Placebo;
        case VideoSpeedPreset::Medium:
        default:
            return VideoEncodeSpeedPreset::Medium;
        }
    }

    AudioCodec toInternalAudioCodec(OutputAudioCodec codec)
    {
        switch (codec) {
        case OutputAudioCodec::AAC:
            return AudioCodec::AAC;
        case OutputAudioCodec::OPUS:
            return AudioCodec::OPUS;
        case OutputAudioCodec::MP3:
            return AudioCodec::MP3;
        case OutputAudioCodec::Auto:
        default:
            return AudioCodec::Auto;
        }
    }

    TranscodeProgress toPublicProgress(const ProgressInfo& info)
    {
        TranscodeProgress progress;
        progress.frame = info.frame;
        progress.outTimeMs = info.outTimeMs;
        progress.speed = info.speed;
        progress.stage = info.raw;
        return progress;
    }

    TranscodeConfig toInternalConfig(const LocalTranscodeConfig& config)
    {
        TranscodeConfig internal;
        internal.inputUrl = config.inputUrl;
        internal.outputUrl = config.outputUrl;
        internal.width = config.width;
        internal.height = config.height;
        internal.fps = config.fps;

        internal.videoCodec = toInternalVideoCodec(config.videoCodec);
        internal.videoBitrate.rateControl = toInternalRcMode(config.rcMode);
        internal.videoBitrate.targetKbps = config.videoBitrateKbps;
        internal.videoBitrate.minKbps = config.minVideoBitrateKbps;
        internal.videoBitrate.maxKbps = config.maxVideoBitrateKbps;
        internal.videoBitrate.bufferSizeKbits = config.videoBufferSizeKbits;
        internal.videoBitrate.quality = config.quality;

        internal.videoEncode.speedPreset = toInternalSpeed(config.speed);
        internal.videoEncode.gopSize = config.gopSize;
        internal.videoEncode.maxBFrames = config.maxBFrames;
        internal.videoEncode.tune = config.tune;
        internal.videoEncode.profile = config.profile;
        internal.videoEncode.level = config.level;

        internal.hardware.enabled = !config.disableHardware;
        internal.hardware.allowZeroCopyFallback = true;

        internal.audioEnabled = !config.noAudio;
        internal.audioCodec = toInternalAudioCodec(config.audioCodec);
        internal.audioBitrateKbps = config.audioBitrateKbps;

        return internal;
    }

    ErrorInfo validateConfig(const LocalTranscodeConfig& config)
    {
        if (config.inputUrl.empty()) {
            return ErrorInfo::invalidArgument("inputUrl is empty");
        }

        if (config.outputUrl.empty()) {
            return ErrorInfo::invalidArgument("outputUrl is empty");
        }

        if ((config.width < 0) || (config.height < 0) || (config.fps < 0)) {
            return ErrorInfo::invalidArgument("width, height and fps must be greater than or equal to 0");
        }

        if ((config.videoBitrateKbps < 0) ||
            (config.minVideoBitrateKbps < 0) ||
            (config.maxVideoBitrateKbps < 0) ||
            (config.videoBufferSizeKbits < 0) ||
            (config.audioBitrateKbps < 0)) {
            return ErrorInfo::invalidArgument("bitrate values must be greater than or equal to 0");
        }

        if (config.quality < 0 || config.gopSize < 0 || config.maxBFrames < 0) {
            return ErrorInfo::invalidArgument("quality, gopSize and maxBFrames must be greater than or equal to 0");
        }

        if (config.minVideoBitrateKbps > 0 &&
            config.maxVideoBitrateKbps > 0 &&
            config.minVideoBitrateKbps > config.maxVideoBitrateKbps) {
            return ErrorInfo::invalidArgument("minVideoBitrateKbps must be less than or equal to maxVideoBitrateKbps");
        }

        return ErrorInfo::success();
    }

} // namespace

Result<LocalTranscodeReport> transcodeLocalFile(const LocalTranscodeConfig& config,
                                                TranscodeProgressCallback progressCallback)
{
    const ErrorInfo validation = validateConfig(config);
    if (!validation.ok()) {
        return Result<LocalTranscodeReport>::failure(validation);
    }

    FFmpegTranscoder transcoder;
    LocalTranscodeReport report;
    report.config = config;

    transcoder.setProgressCallback([&](const ProgressInfo& info) {
        report.lastProgress = toPublicProgress(info);
        if (progressCallback) {
            progressCallback(report.lastProgress);
        }
    });

    if (!transcoder.initialize(toInternalConfig(config))) {
        return Result<LocalTranscodeReport>::failure(
            ErrorInfo::invalidArgument(transcoder.lastError())
        );
    }

    if (!transcoder.start()) {
        return Result<LocalTranscodeReport>::failure(
            ErrorInfo::internalError(transcoder.lastError())
        );
    }

    if (!transcoder.wait()) {
        return Result<LocalTranscodeReport>::failure(
            ErrorInfo::internalError(transcoder.lastError().empty()
                ? "transcode wait failed"
                : transcoder.lastError())
        );
    }

    const std::string lastError = transcoder.lastError();
    if (!lastError.empty()) {
        return Result<LocalTranscodeReport>::failure(
            ErrorInfo::ffmpegFailure(lastError)
        );
    }

    return Result<LocalTranscodeReport>::success(std::move(report));
}

} // namespace media
