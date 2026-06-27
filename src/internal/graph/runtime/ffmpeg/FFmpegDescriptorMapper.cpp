#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg::graph {

MediaStreamKind FFmpegDescriptorMapper::toStreamKind(AVMediaType type) noexcept
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return MediaStreamKind::Video;
    case AVMEDIA_TYPE_AUDIO: return MediaStreamKind::Audio;
    case AVMEDIA_TYPE_SUBTITLE: return MediaStreamKind::Subtitle;
    case AVMEDIA_TYPE_DATA: return MediaStreamKind::Data;
    case AVMEDIA_TYPE_ATTACHMENT: return MediaStreamKind::Attachment;
    default: return MediaStreamKind::Unknown;
    }
}

MediaCodecDomain FFmpegDescriptorMapper::toCodecDomain(AVMediaType type) noexcept
{
    switch (type) {
    case AVMEDIA_TYPE_VIDEO: return MediaCodecDomain::Video;
    case AVMEDIA_TYPE_AUDIO: return MediaCodecDomain::Audio;
    case AVMEDIA_TYPE_SUBTITLE: return MediaCodecDomain::Subtitle;
    case AVMEDIA_TYPE_DATA: return MediaCodecDomain::Data;
    default: return MediaCodecDomain::Unknown;
    }
}

MediaRational FFmpegDescriptorMapper::toRational(AVRational rational) noexcept
{
    return MediaRational{ rational.num, rational.den };
}

MediaFormatDescriptor FFmpegDescriptorMapper::fromStream(const AVStream* stream)
{
    MediaFormatDescriptor descriptor;
    if (!stream) {
        return descriptor;
    }

    descriptor.streamIndex = stream->index;
    descriptor.time.timeBase = toRational(stream->time_base);
    descriptor.time.frameRate = toRational(stream->avg_frame_rate);
    descriptor.time.startTime = stream->start_time;
    descriptor.time.duration = stream->duration;

    MediaFormatDescriptor params = fromCodecParameters(stream->codecpar);
    params.streamIndex = descriptor.streamIndex;
    params.time = descriptor.time;
    return params;
}

MediaFormatDescriptor FFmpegDescriptorMapper::fromCodecParameters(const AVCodecParameters* params)
{
    MediaFormatDescriptor descriptor;
    if (!params) {
        return descriptor;
    }

    descriptor.streamKind = toStreamKind(params->codec_type);
    descriptor.codec.domain = toCodecDomain(params->codec_type);
    descriptor.codec.operation = MediaCodecOperation::Unknown;

    if (const AVCodecDescriptor* codecDesc = avcodec_descriptor_get(params->codec_id)) {
        descriptor.codec.codecName = codecDesc->name ? codecDesc->name : "";
        descriptor.codec.codecLongName = codecDesc->long_name ? codecDesc->long_name : "";
    }

    descriptor.codec.bitrate = params->bit_rate;
    descriptor.codec.profile = av_get_profile_name(nullptr, params->profile) ? av_get_profile_name(nullptr, params->profile) : "";
    descriptor.codec.level = params->level;

    if (params->codec_type == AVMEDIA_TYPE_VIDEO) {
        descriptor.video.size = MediaSize{ params->width, params->height };
        descriptor.video.pixelFormat = params->format >= 0 ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(params->format)) : "";
        descriptor.video.sampleAspectRatio = toRational(params->sample_aspect_ratio);
    } else if (params->codec_type == AVMEDIA_TYPE_AUDIO) {
        descriptor.audio.sampleRate = params->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        descriptor.audio.channels = params->ch_layout.nb_channels;
        char layout[128] = {};
        if (av_channel_layout_describe(&params->ch_layout, layout, sizeof(layout)) >= 0) {
            descriptor.audio.channelLayout = layout;
        }
#else
        descriptor.audio.channels = params->channels;
        descriptor.audio.channelLayout = std::to_string(params->channel_layout);
#endif
        descriptor.audio.sampleFormat = params->format >= 0 ? av_get_sample_fmt_name(static_cast<AVSampleFormat>(params->format)) : "";
    }

    return descriptor;
}

MediaFormatDescriptor FFmpegDescriptorMapper::fromCodecContext(const AVCodecContext* context,
                                                               MediaCodecOperation operation)
{
    MediaFormatDescriptor descriptor;
    if (!context) {
        return descriptor;
    }

    descriptor.streamKind = toStreamKind(context->codec_type);
    descriptor.codec.domain = toCodecDomain(context->codec_type);
    descriptor.codec.operation = operation;
    descriptor.codec.codecName = context->codec ? context->codec->name : "";
    descriptor.codec.codecLongName = context->codec ? context->codec->long_name : "";
    descriptor.codec.bitrate = context->bit_rate;
    descriptor.codec.level = context->level;
    descriptor.time.timeBase = toRational(context->time_base);
    descriptor.time.frameRate = toRational(context->framerate);

    if (context->codec_type == AVMEDIA_TYPE_VIDEO) {
        descriptor.video.size = MediaSize{ context->width, context->height };
        descriptor.video.pixelFormat = av_get_pix_fmt_name(context->pix_fmt) ? av_get_pix_fmt_name(context->pix_fmt) : "";
        descriptor.video.sampleAspectRatio = toRational(context->sample_aspect_ratio);
    } else if (context->codec_type == AVMEDIA_TYPE_AUDIO) {
        descriptor.audio.sampleRate = context->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        descriptor.audio.channels = context->ch_layout.nb_channels;
        char layout[128] = {};
        if (av_channel_layout_describe(&context->ch_layout, layout, sizeof(layout)) >= 0) {
            descriptor.audio.channelLayout = layout;
        }
#else
        descriptor.audio.channels = context->channels;
        descriptor.audio.channelLayout = std::to_string(context->channel_layout);
#endif
        descriptor.audio.sampleFormat = av_get_sample_fmt_name(context->sample_fmt) ? av_get_sample_fmt_name(context->sample_fmt) : "";
    }

    return descriptor;
}

MediaFormatDescriptor FFmpegDescriptorMapper::fromFrame(const AVFrame* frame, MediaStreamKind streamKind)
{
    MediaFormatDescriptor descriptor;
    if (!frame) {
        return descriptor;
    }

    descriptor.streamKind = streamKind;

    if (streamKind == MediaStreamKind::Video) {
        descriptor.video.size = MediaSize{ frame->width, frame->height };
        descriptor.video.pixelFormat = frame->format >= 0 ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)) : "";
        descriptor.hardware.frameKind = toHardwareFrameKind(frame);
    } else if (streamKind == MediaStreamKind::Audio) {
        descriptor.audio.sampleRate = frame->sample_rate;
#if LIBAVUTIL_VERSION_MAJOR >= 57
        descriptor.audio.channels = frame->ch_layout.nb_channels;
        char layout[128] = {};
        if (av_channel_layout_describe(&frame->ch_layout, layout, sizeof(layout)) >= 0) {
            descriptor.audio.channelLayout = layout;
        }
#else
        descriptor.audio.channels = frame->channels;
        descriptor.audio.channelLayout = std::to_string(frame->channel_layout);
#endif
        descriptor.audio.sampleFormat = frame->format >= 0 ? av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame->format)) : "";
    }

    descriptor.time.timeBase = MediaRational{ 0, 1 };
    descriptor.time.startTime = frame->pts;
    return descriptor;
}

MediaHardwareDeviceKind FFmpegDescriptorMapper::toHardwareDeviceKind(AVHWDeviceType type) noexcept
{
    switch (type) {
    case AV_HWDEVICE_TYPE_D3D11VA: return MediaHardwareDeviceKind::D3D11VA;
    case AV_HWDEVICE_TYPE_CUDA: return MediaHardwareDeviceKind::CUDA;
    case AV_HWDEVICE_TYPE_VAAPI: return MediaHardwareDeviceKind::VAAPI;
    case AV_HWDEVICE_TYPE_DRM: return MediaHardwareDeviceKind::DRMPrime;
#if defined(AV_HWDEVICE_TYPE_VIDEOTOOLBOX)
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return MediaHardwareDeviceKind::VideoToolbox;
#endif
#if defined(AV_HWDEVICE_TYPE_MEDIACODEC)
    case AV_HWDEVICE_TYPE_MEDIACODEC: return MediaHardwareDeviceKind::MediaCodec;
#endif
    default: return MediaHardwareDeviceKind::Unknown;
    }
}

MediaHardwareFrameKind FFmpegDescriptorMapper::toHardwareFrameKind(const AVFrame* frame) noexcept
{
    if (!frame) {
        return MediaHardwareFrameKind::Unknown;
    }

    if (frame->hw_frames_ctx) {
        return MediaHardwareFrameKind::Hardware;
    }

    return MediaHardwareFrameKind::Software;
}

} // namespace media::ffmpeg::graph
