#include "internal/FFmpegVideoInputMetadata.h"

namespace media::ffmpeg {
namespace {

bool isValidRatio(AVRational ratio)
{
    return ratio.num > 0 && ratio.den > 0;
}

AVRational validOrDefaultRatio(AVRational ratio, AVRational fallback)
{
    return isValidRatio(ratio) ? ratio : fallback;
}

int validOrFallbackDimension(int primary, int fallback)
{
    return primary > 0 ? primary : fallback;
}

AVRational streamTimeBase(const AVStream* inputStream)
{
    if (inputStream && isValidRatio(inputStream->time_base)) {
        return inputStream->time_base;
    }

    return AVRational{ 0, 1 };
}

AVRational decoderTimeBase(const AVCodecContext* decoderCtx)
{
    if (decoderCtx && isValidRatio(decoderCtx->time_base)) {
        return decoderCtx->time_base;
    }

    return AVRational{ 0, 1 };
}

AVRational chooseTimeBase(const AVCodecContext* decoderCtx, const AVStream* inputStream)
{
    const AVRational fromStream = streamTimeBase(inputStream);
    if (isValidRatio(fromStream)) {
        return fromStream;
    }

    return decoderTimeBase(decoderCtx);
}

AVRational chooseFrameRate(const AVCodecContext* decoderCtx, const AVStream* inputStream)
{
    if (inputStream && isValidRatio(inputStream->avg_frame_rate)) {
        return inputStream->avg_frame_rate;
    }

    if (inputStream && isValidRatio(inputStream->r_frame_rate)) {
        return inputStream->r_frame_rate;
    }

    if (decoderCtx && isValidRatio(decoderCtx->framerate)) {
        return decoderCtx->framerate;
    }

    return AVRational{ 0, 1 };
}

AVRational streamSampleAspectRatio(const AVStream* inputStream)
{
    if (inputStream && inputStream->codecpar && isValidRatio(inputStream->codecpar->sample_aspect_ratio)) {
        return inputStream->codecpar->sample_aspect_ratio;
    }

    return AVRational{ 1, 1 };
}

AVRational chooseSampleAspectRatio(const AVCodecContext* decoderCtx, const AVStream* inputStream)
{
    if (decoderCtx && isValidRatio(decoderCtx->sample_aspect_ratio)) {
        return decoderCtx->sample_aspect_ratio;
    }

    return validOrDefaultRatio(streamSampleAspectRatio(inputStream), AVRational{ 1, 1 });
}

} // namespace

bool FFmpegVideoInputMetadata::hasValidSize() const
{
    return width > 0 && height > 0;
}

FFmpegVideoInputMetadata FFmpegVideoInputMetadata::fromDecoderContextAndStream(
    const AVCodecContext* decoderCtx,
    const AVStream* inputStream)
{
    FFmpegVideoInputMetadata metadata;

    const int streamWidth = inputStream && inputStream->codecpar
        ? inputStream->codecpar->width
        : 0;
    const int streamHeight = inputStream && inputStream->codecpar
        ? inputStream->codecpar->height
        : 0;

    metadata.width = validOrFallbackDimension(decoderCtx ? decoderCtx->width : 0, streamWidth);
    metadata.height = validOrFallbackDimension(decoderCtx ? decoderCtx->height : 0, streamHeight);
    metadata.sampleAspectRatio = chooseSampleAspectRatio(decoderCtx, inputStream);
    metadata.timeBase = chooseTimeBase(decoderCtx, inputStream);
    metadata.frameRate = chooseFrameRate(decoderCtx, inputStream);

    return metadata;
}

} // namespace media::ffmpeg
