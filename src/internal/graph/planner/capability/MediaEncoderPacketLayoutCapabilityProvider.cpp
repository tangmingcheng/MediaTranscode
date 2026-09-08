#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"

#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using LayoutResult = ::media::Result<MediaEncodedPacketLayout>;

void clearFrameBuffers(AVFrame& frame) noexcept
{
    for (AVBufferRef* buffer : frame.buf) {
        if (buffer) std::memset(buffer->data, 0, buffer->size);
    }
}

bool startsWithStartCode(std::span<const std::uint8_t> bytes) noexcept
{
    return bytes.size() >= 3 && bytes[0] == 0 && bytes[1] == 0 &&
        (bytes[2] == 1 || (bytes.size() >= 4 && bytes[2] == 0 && bytes[3] == 1));
}

bool isCompleteLengthPrefixed(
    std::span<const std::uint8_t> bytes, std::uint8_t width) noexcept
{
    std::size_t offset = 0;
    std::size_t units = 0;
    while (offset < bytes.size()) {
        if (bytes.size() - offset < width) return false;
        std::size_t length = 0;
        for (std::uint8_t index = 0; index < width; ++index) {
            length = (length << 8U) | bytes[offset + index];
        }
        offset += width;
        if (length == 0 || length > bytes.size() - offset) return false;
        offset += length;
        ++units;
    }
    return units != 0 && offset == bytes.size();
}

LayoutResult classifyPacket(std::span<const std::uint8_t> bytes)
{
    if (startsWithStartCode(bytes)) {
        return LayoutResult::success(
            MediaEncodedPacketLayout::startCodeDelimited());
    }
    std::uint8_t selected = 0;
    for (std::uint8_t width = 1; width <= 4; ++width) {
        if (!isCompleteLengthPrefixed(bytes, width)) continue;
        if (selected != 0) {
            return LayoutResult::failure(::media::ErrorInfo::unsupported(
                "encoded probe packet has ambiguous length-prefix geometry"));
        }
        selected = width;
    }
    if (selected == 0) {
        return LayoutResult::failure(::media::ErrorInfo::unsupported(
            "encoded probe packet exposes neither start-code nor length-prefix geometry"));
    }
    return MediaEncodedPacketLayout::lengthPrefixed(selected);
}

LayoutResult classifyExtradata(const AVCodecContext& context)
{
    if (!context.extradata || context.extradata_size <= 0) {
        return LayoutResult::failure(::media::ErrorInfo::notInitialized(
            "opened encoder has no packet-layout extradata"));
    }
    const auto bytes = std::span<const std::uint8_t>(
        context.extradata, static_cast<std::size_t>(context.extradata_size));
    if (startsWithStartCode(bytes)) {
        return LayoutResult::success(
            MediaEncodedPacketLayout::startCodeDelimited());
    }
    std::uint8_t width = 0;
    if (context.codec_id == AV_CODEC_ID_H264 && bytes.size() > 4 && bytes[0] == 1) {
        width = static_cast<std::uint8_t>((bytes[4] & 0x03U) + 1U);
    } else if (context.codec_id == AV_CODEC_ID_HEVC && bytes.size() > 21 && bytes[0] == 1) {
        width = static_cast<std::uint8_t>((bytes[21] & 0x03U) + 1U);
    }
    return width != 0
        ? MediaEncodedPacketLayout::lengthPrefixed(width)
        : LayoutResult::failure(::media::ErrorInfo::unsupported(
              "opened encoder extradata does not identify packet layout"));
}

LayoutResult encodeProbeFrame(AVCodecContext& context)
{
    auto frame = ::media::ffmpeg::makeFrame();
    auto packet = ::media::ffmpeg::makePacket();
    if (!frame || !packet) {
        return LayoutResult::failure(::media::ErrorInfo::allocationFailed(
            "encoder packet-layout probe frame or packet"));
    }
    frame->format = context.pix_fmt;
    frame->width = context.width;
    frame->height = context.height;
    frame->pts = 0;
    int allocated = 0;
    if (context.hw_frames_ctx) {
        allocated = av_hwframe_get_buffer(context.hw_frames_ctx, frame.get(), 0);
        if (allocated >= 0) {
            const auto* frames = reinterpret_cast<const AVHWFramesContext*>(
                context.hw_frames_ctx->data);
            auto software = ::media::ffmpeg::makeFrame();
            if (!frames || !software || frames->sw_format == AV_PIX_FMT_NONE) {
                return LayoutResult::failure(::media::ErrorInfo::notInitialized(
                    "encoder packet-layout probe has no hardware surface format"));
            }
            software->format = frames->sw_format;
            software->width = context.width;
            software->height = context.height;
            int softwareAllocated = av_frame_get_buffer(software.get(), 32);
            if (softwareAllocated >= 0) {
                softwareAllocated = av_frame_make_writable(software.get());
            }
            if (softwareAllocated < 0) {
                return LayoutResult::failure(FFmpegGraphError::statusFromCode(
                    softwareAllocated,
                    "allocate encoder packet-layout software probe frame").error());
            }
            clearFrameBuffers(*software);
            allocated = av_hwframe_transfer_data(frame.get(), software.get(), 0);
        }
    } else {
        const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(context.pix_fmt);
        if (!descriptor || (descriptor->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0) {
            return LayoutResult::failure(::media::ErrorInfo::unsupported(
                "opened encoder has no probeable frame allocation contract"));
        }
        allocated = av_frame_get_buffer(frame.get(), 32);
        if (allocated >= 0) {
            allocated = av_frame_make_writable(frame.get());
        }
        if (allocated >= 0) clearFrameBuffers(*frame);
    }
    if (allocated < 0) {
        return LayoutResult::failure(FFmpegGraphError::statusFromCode(
            allocated, "allocate encoder packet-layout probe frame").error());
    }
    int code = avcodec_send_frame(&context, frame.get());
    if (code < 0) {
        return LayoutResult::failure(FFmpegGraphError::statusFromCode(
            code, "submit encoder packet-layout probe frame").error());
    }
    bool flushed = false;
    for (;;) {
        code = avcodec_receive_packet(&context, packet.get());
        if (code == 0) {
            if (packet->data && packet->size > 0) {
                return classifyPacket(std::span<const std::uint8_t>(
                    packet->data, static_cast<std::size_t>(packet->size)));
            }
            av_packet_unref(packet.get());
            continue;
        }
        if (code == AVERROR(EAGAIN) && !flushed) {
            code = avcodec_send_frame(&context, nullptr);
            if (code < 0 && code != AVERROR_EOF) {
                return LayoutResult::failure(FFmpegGraphError::statusFromCode(
                    code, "flush encoder packet-layout probe").error());
            }
            flushed = true;
            continue;
        }
        if (code == AVERROR_EOF) {
            return LayoutResult::failure(::media::ErrorInfo::unsupported(
                "encoder packet-layout probe ended without an encoded packet"));
        }
        return LayoutResult::failure(FFmpegGraphError::statusFromCode(
            code, "receive encoder packet-layout probe packet").error());
    }
}

} // namespace

::media::Result<MediaEncodedPacketLayout>
MediaEncoderPacketLayoutCapabilityProvider::probeOpenedContext(
    AVCodecContext& context)
{
    auto extradata = classifyExtradata(context);
    return extradata ? extradata : encodeProbeFrame(context);
}

} // namespace media::ffmpeg::graph
