#include "internal/FFmpegRAII.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
}

namespace {

using namespace media::ffmpeg::graph;

std::string ffmpegErrorString(int errorCode)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(errorCode, text, sizeof(text));
    return text;
}

int fail(const std::string& message)
{
    std::cerr << "graph encode probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
}

std::size_t parsePositiveSize(const char* text, std::size_t fallback)
{
    if (!text) {
        return fallback;
    }

    const long value = std::strtol(text, nullptr, 10);
    return value > 0 ? static_cast<std::size_t>(value) : fallback;
}

MediaEdgePolicy queuePolicy(std::size_t capacity = 64)
{
    MediaEdgePolicy policy;
    policy.queuePolicy.mode = MediaQueueMode::Blocking;
    policy.queuePolicy.bounded = true;
    policy.queuePolicy.capacity = capacity;
    policy.queuePolicy.overflowPolicy = MediaQueueOverflowPolicy::BlockProducer;
    return policy;
}

struct VideoDecoderSetup {
    ::media::ffmpeg::InputFormatContextPtr inputContext;
    ::media::ffmpeg::CodecContextPtr codecContext;
    AVRational timeBase{1, 30};
    AVRational frameRate{30, 1};
    int videoStreamIndex = invalidMediaStreamIndex;
};

struct VideoEncoderSetup {
    ::media::ffmpeg::CodecContextPtr codecContext;
    std::string encoderName;
    std::string codecName;
};

struct EncodeStats {
    std::size_t demuxIterations = 0;
    std::size_t demuxPackets = 0;
    std::size_t videoPackets = 0;
    std::size_t skippedPackets = 0;
    std::size_t inputFrames = 0;
    std::size_t encodedPackets = 0;
    std::size_t keyPackets = 0;
    std::size_t controlBuffers = 0;
    std::size_t encodedBytes = 0;
    bool eofSeen = false;
    bool encoderBound = false;
    bool firstFrameSet = false;
    bool firstPacketSet = false;
    MediaTimeValue firstFramePts = 0;
    MediaTimeValue lastFramePts = 0;
    MediaTimeValue firstPacketPts = 0;
    MediaTimeValue lastPacketPts = 0;
    int width = 0;
    int height = 0;
    std::string pixelFormat = "unknown";
    std::string encoderName;
    std::string codecName;
};

bool pixelFormatSupported(const AVCodec* codec, AVPixelFormat format)
{
    if (!codec || !codec->pix_fmts) {
        return true;
    }

    for (const AVPixelFormat* current = codec->pix_fmts; *current != AV_PIX_FMT_NONE; ++current) {
        if (*current == format) {
            return true;
        }
    }

    return false;
}

void addEncoderCandidate(std::vector<const AVCodec*>& candidates, const AVCodec* codec)
{
    if (!codec) {
        return;
    }

    if (std::find(candidates.begin(), candidates.end(), codec) == candidates.end()) {
        candidates.push_back(codec);
    }
}

std::vector<const AVCodec*> encoderCandidates(const std::string& requestedName)
{
    std::vector<const AVCodec*> candidates;

    if (!requestedName.empty() && requestedName != "auto") {
        addEncoderCandidate(candidates, avcodec_find_encoder_by_name(requestedName.c_str()));
        if (requestedName == "h264") {
            addEncoderCandidate(candidates, avcodec_find_encoder(AV_CODEC_ID_H264));
        } else if (requestedName == "mpeg4") {
            addEncoderCandidate(candidates, avcodec_find_encoder(AV_CODEC_ID_MPEG4));
        }
        return candidates;
    }

    addEncoderCandidate(candidates, avcodec_find_encoder_by_name("libx264"));
    addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_mf"));
    addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_nvenc"));
    addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_qsv"));
    addEncoderCandidate(candidates, avcodec_find_encoder_by_name("h264_amf"));
    addEncoderCandidate(candidates, avcodec_find_encoder(AV_CODEC_ID_H264));
    addEncoderCandidate(candidates, avcodec_find_encoder(AV_CODEC_ID_MPEG4));
    return candidates;
}

void collectEncodedPackets(MediaChannel& packetChannel, EncodeStats& stats)
{
    MediaBufferRef packetBuffer;
    while (packetChannel.tryPop(packetBuffer)) {
        if (!packetBuffer) {
            continue;
        }

        if (packetBuffer->isEof() || packetBuffer->isFlush()) {
            ++stats.controlBuffers;
            continue;
        }

        if (packetBuffer->payloadKind() != MediaPayloadKind::Packet) {
            continue;
        }

        ++stats.encodedPackets;
        if (packetBuffer->isKeyFrame()) {
            ++stats.keyPackets;
        }

        if (!stats.firstPacketSet) {
            stats.firstPacketPts = packetBuffer->pts();
            stats.firstPacketSet = true;
        }
        stats.lastPacketPts = packetBuffer->pts();

        const auto* ffmpegPacket = dynamic_cast<const FFmpegPacketBuffer*>(packetBuffer.get());
        const AVPacket* packet = ffmpegPacket ? ffmpegPacket->packet() : nullptr;
        if (packet) {
            stats.encodedBytes += static_cast<std::size_t>(packet->size > 0 ? packet->size : 0);
            if ((packet->flags & AV_PKT_FLAG_KEY) && !packetBuffer->isKeyFrame()) {
                ++stats.keyPackets;
            }
        }
    }
}

::media::Result<VideoDecoderSetup> createVideoDecoder(const std::string& inputPath)
{
    AVFormatContext* rawFormat = nullptr;
    int ret = avformat_open_input(&rawFormat, inputPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_open_input: " + ffmpegErrorString(ret), ret));
    }

    VideoDecoderSetup setup;
    setup.inputContext.reset(rawFormat);

    ret = avformat_find_stream_info(setup.inputContext.get(), nullptr);
    if (ret < 0) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::ffmpegFailure("avformat_find_stream_info: " + ffmpegErrorString(ret), ret));
    }

    const int streamIndex = av_find_best_stream(
        setup.inputContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::ffmpegFailure("av_find_best_stream(video): " + ffmpegErrorString(streamIndex), streamIndex));
    }

    AVStream* stream = setup.inputContext->streams[streamIndex];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::unsupported("video decoder not found"));
    }

    auto codecContext = ::media::ffmpeg::makeCodecContext(decoder);
    if (!codecContext) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::allocationFailed("avcodec_alloc_context3 returned null"));
    }

    ret = avcodec_parameters_to_context(codecContext.get(), stream->codecpar);
    if (ret < 0) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::ffmpegFailure("avcodec_parameters_to_context: " + ffmpegErrorString(ret), ret));
    }

    ret = avcodec_open2(codecContext.get(), decoder, nullptr);
    if (ret < 0) {
        return ::media::Result<VideoDecoderSetup>::failure(
            ::media::ErrorInfo::ffmpegFailure("avcodec_open2(video): " + ffmpegErrorString(ret), ret));
    }

    setup.videoStreamIndex = streamIndex;
    setup.timeBase = stream->time_base.num > 0 && stream->time_base.den > 0 ? stream->time_base : AVRational{1, 30};
    setup.frameRate = av_guess_frame_rate(setup.inputContext.get(), stream, nullptr);
    if (setup.frameRate.num <= 0 || setup.frameRate.den <= 0) {
        setup.frameRate = AVRational{30, 1};
    }
    setup.codecContext = std::move(codecContext);
    return ::media::Result<VideoDecoderSetup>::success(std::move(setup));
}

::media::Result<VideoEncoderSetup> createVideoEncoder(const AVFrame& firstFrame,
                                                       const VideoDecoderSetup& decoderSetup,
                                                       const std::string& requestedEncoder)
{
    const AVPixelFormat pixelFormat = static_cast<AVPixelFormat>(firstFrame.format);
    if (firstFrame.width <= 0 || firstFrame.height <= 0 || pixelFormat == AV_PIX_FMT_NONE) {
        return ::media::Result<VideoEncoderSetup>::failure(
            ::media::ErrorInfo::invalidArgument("first decoded frame has invalid video format"));
    }

    std::string lastError = "no encoder candidates available";
    for (const AVCodec* encoder : encoderCandidates(requestedEncoder)) {
        if (!encoder) {
            continue;
        }

        if (!pixelFormatSupported(encoder, pixelFormat)) {
            lastError = std::string(encoder->name) + " does not support input pixel format " +
                        (av_get_pix_fmt_name(pixelFormat) ? av_get_pix_fmt_name(pixelFormat) : "unknown");
            continue;
        }

        auto codecContext = ::media::ffmpeg::makeCodecContext(encoder);
        if (!codecContext) {
            lastError = std::string(encoder->name) + ": avcodec_alloc_context3 returned null";
            continue;
        }

        codecContext->width = firstFrame.width;
        codecContext->height = firstFrame.height;
        codecContext->pix_fmt = pixelFormat;
        codecContext->time_base = decoderSetup.timeBase;
        codecContext->framerate = decoderSetup.frameRate;
        codecContext->bit_rate = 8'000'000;
        codecContext->gop_size = 60;
        codecContext->max_b_frames = 0;
        if (firstFrame.sample_aspect_ratio.num > 0 && firstFrame.sample_aspect_ratio.den > 0) {
            codecContext->sample_aspect_ratio = firstFrame.sample_aspect_ratio;
        }
        codecContext->color_range = firstFrame.color_range;
        codecContext->color_primaries = firstFrame.color_primaries;
        codecContext->color_trc = firstFrame.color_trc;
        codecContext->colorspace = firstFrame.colorspace;

        const int ret = avcodec_open2(codecContext.get(), encoder, nullptr);
        if (ret < 0) {
            lastError = std::string(encoder->name) + ": avcodec_open2: " + ffmpegErrorString(ret);
            continue;
        }

        VideoEncoderSetup setup;
        setup.codecContext = std::move(codecContext);
        setup.encoderName = encoder->name ? encoder->name : "unknown";
        setup.codecName = avcodec_get_name(encoder->id) ? avcodec_get_name(encoder->id) : "unknown";
        return ::media::Result<VideoEncoderSetup>::success(std::move(setup));
    }

    return ::media::Result<VideoEncoderSetup>::failure(
        ::media::ErrorInfo::unsupported("failed to create video encoder: " + lastError));
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_encode_probe.exe <input-media-file> [target-frames] [encoder-name|auto] [max-demux-packets]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::size_t targetFrames = argc >= 3 ? parsePositiveSize(argv[2], 100) : 100;
    const std::string requestedEncoder = argc >= 4 ? argv[3] : "auto";
    std::size_t maxDemuxPackets = targetFrames * 16 + 256;
    if (maxDemuxPackets < 512) {
        maxDemuxPackets = 512;
    }
    if (argc >= 5) {
        maxDemuxPackets = parsePositiveSize(argv[4], maxDemuxPackets);
    }

    auto decoderSetup = createVideoDecoder(inputPath);
    if (!decoderSetup) {
        return fail("create video decoder: " + decoderSetup.error().describe());
    }

    const int videoStreamIndex = decoderSetup.value().videoStreamIndex;

    MediaGraph graph;
    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "file-input");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "demux");
    const MediaNodeId demuxPacketSink = graph.addNode(MediaNodeKind::DebugDump, "demux-packet-sink");
    const MediaNodeId decodeCodecSource = graph.addNode(MediaNodeKind::DebugDump, "decode-codec-source");
    const MediaNodeId decodePacketSource = graph.addNode(MediaNodeKind::DebugDump, "decode-packet-source");
    const MediaNodeId videoDecode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    const MediaNodeId decodedFrameSink = graph.addNode(MediaNodeKind::DebugDump, "decoded-frame-sink");
    const MediaNodeId encodeCodecSource = graph.addNode(MediaNodeKind::DebugDump, "encode-codec-source");
    const MediaNodeId encodeFrameSource = graph.addNode(MediaNodeKind::DebugDump, "encode-frame-source");
    const MediaNodeId videoEncode = graph.addNode(MediaNodeKind::VideoEncode, "video-encode");
    const MediaNodeId encodedPacketSink = graph.addNode(MediaNodeKind::DebugDump, "encoded-packet-sink");

    graph.setNodeOption(fileInput, "path", inputPath);

    graph.addOutputPort(fileInput, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext, true, false);
    graph.addInputPort(demux, "format", MediaStreamKind::Metadata, MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext, true, false);
    graph.addOutputPort(demux, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);
    graph.addInputPort(demuxPacketSink, "packet", MediaStreamKind::Any, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);

    graph.addOutputPort(decodeCodecSource, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata,
                        MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoDecode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata,
                       MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(decodePacketSource, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet, true, true);
    graph.addInputPort(videoDecode, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet, true, true);
    graph.addOutputPort(videoDecode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);
    graph.addInputPort(decodedFrameSink, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);

    graph.addOutputPort(encodeCodecSource, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata,
                        MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(videoEncode, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata,
                       MediaPayloadKind::CodecContext, true, false);
    graph.addOutputPort(encodeFrameSource, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame, true, true);
    graph.addInputPort(videoEncode, "frame", MediaStreamKind::Video, MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(videoEncode, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                        MediaPayloadKind::Packet, true, true);
    graph.addInputPort(encodedPacketSink, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket,
                       MediaPayloadKind::Packet, true, true);

    graph.connect(fileInput, "format", demux, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(demux, "packet", demuxPacketSink, "packet", "demux-to-packet-sink", queuePolicy(128));
    graph.connect(decodeCodecSource, "codec", videoDecode, "codec", "codec-to-video-decode", queuePolicy(1));
    graph.connect(decodePacketSource, "packet", videoDecode, "packet", "packet-to-video-decode", queuePolicy(128));
    graph.connect(videoDecode, "frame", decodedFrameSink, "frame", "video-decode-to-frame-sink", queuePolicy(128));
    graph.connect(encodeCodecSource, "codec", videoEncode, "codec", "codec-to-video-encode", queuePolicy(1));
    graph.connect(encodeFrameSource, "frame", videoEncode, "frame", "frame-to-video-encode", queuePolicy(128));
    graph.connect(videoEncode, "packet", encodedPacketSink, "packet", "video-encode-to-packet-sink", queuePolicy(128));

    MediaGraphRuntime runtime;
    auto compileStatus = runtime.compile(std::move(graph));
    if (!compileStatus) {
        return failStatus("compile", compileStatus);
    }

    auto registerStatus = runtime.registerDefaultRuntimeNodes();
    if (!registerStatus) {
        return failStatus("register default runtime nodes", registerStatus);
    }

    MediaRuntimeNode* fileRuntime = runtime.scheduler().findNode(fileInput);
    MediaRuntimeNode* demuxRuntime = runtime.scheduler().findNode(demux);
    MediaRuntimeNode* decodeRuntime = runtime.scheduler().findNode(videoDecode);
    MediaRuntimeNode* encodeRuntime = runtime.scheduler().findNode(videoEncode);
    if (!fileRuntime || !demuxRuntime || !decodeRuntime || !encodeRuntime) {
        return fail("required runtime node not found");
    }

    MediaChannel* demuxPacketChannel = runtime.context().findOutputChannel(demux, "packet");
    MediaChannel* decodeCodecChannel = runtime.context().findInputChannel(videoDecode, "codec");
    MediaChannel* decodePacketChannel = runtime.context().findInputChannel(videoDecode, "packet");
    MediaChannel* decodedFrameChannel = runtime.context().findOutputChannel(videoDecode, "frame");
    MediaChannel* encodeCodecChannel = runtime.context().findInputChannel(videoEncode, "codec");
    MediaChannel* encodeFrameChannel = runtime.context().findInputChannel(videoEncode, "frame");
    MediaChannel* encodedPacketChannel = runtime.context().findOutputChannel(videoEncode, "packet");
    if (!demuxPacketChannel || !decodeCodecChannel || !decodePacketChannel || !decodedFrameChannel ||
        !encodeCodecChannel || !encodeFrameChannel || !encodedPacketChannel) {
        return fail("required channel not found");
    }

    auto decoderCodecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoderSetup.value().codecContext));
    if (!decoderCodecBuffer) {
        return fail("wrap decoder codec context: " + decoderCodecBuffer.error().describe());
    }
    auto decoderCodecPush = decodeCodecChannel->push(decoderCodecBuffer.value());
    if (!decoderCodecPush) {
        return failStatus("push decoder codec context", decoderCodecPush);
    }
    auto decoderBind = decodeRuntime->process(runtime.context());
    if (!decoderBind) {
        return failStatus("bind video decoder", decoderBind);
    }

    auto fileStatus = fileRuntime->process(runtime.context());
    if (!fileStatus) {
        return failStatus("file input process", fileStatus);
    }

    EncodeStats stats;

    auto encodeDecodedFrame = [&](MediaBufferRef frameBuffer) -> ::media::Status {
        const auto* ffmpegFrame = dynamic_cast<const FFmpegFrameBuffer*>(frameBuffer.get());
        const AVFrame* frame = ffmpegFrame ? ffmpegFrame->frame() : nullptr;
        if (!frame) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("decoded frame buffer is invalid"));
        }

        if (!stats.encoderBound) {
            auto encoderSetup = createVideoEncoder(*frame, decoderSetup.value(), requestedEncoder);
            if (!encoderSetup) {
                return ::media::Status::failure(encoderSetup.error());
            }

            stats.encoderName = encoderSetup.value().encoderName;
            stats.codecName = encoderSetup.value().codecName;
            stats.width = frame->width;
            stats.height = frame->height;
            const char* formatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
            stats.pixelFormat = formatName ? formatName : "unknown";

            auto encoderCodecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(encoderSetup.value().codecContext));
            if (!encoderCodecBuffer) {
                return ::media::Status::failure(encoderCodecBuffer.error());
            }
            auto encoderCodecPush = encodeCodecChannel->push(encoderCodecBuffer.value());
            if (!encoderCodecPush) {
                return encoderCodecPush;
            }
            auto encoderBind = encodeRuntime->process(runtime.context());
            if (!encoderBind) {
                return encoderBind;
            }
            stats.encoderBound = true;
        }

        ++stats.inputFrames;
        if (!stats.firstFrameSet) {
            stats.firstFramePts = frameBuffer->pts();
            stats.firstFrameSet = true;
        }
        stats.lastFramePts = frameBuffer->pts();

        auto framePush = encodeFrameChannel->push(std::move(frameBuffer));
        if (!framePush) {
            return framePush;
        }
        auto encodeStatus = encodeRuntime->process(runtime.context());
        if (!encodeStatus) {
            return encodeStatus;
        }
        collectEncodedPackets(*encodedPacketChannel, stats);
        return ::media::Status::success();
    };

    while (stats.inputFrames < targetFrames &&
           stats.demuxIterations < maxDemuxPackets &&
           !stats.eofSeen) {
        auto demuxStatus = demuxRuntime->process(runtime.context());
        if (!demuxStatus) {
            return failStatus("demux process", demuxStatus);
        }
        ++stats.demuxIterations;

        MediaBufferRef packetBuffer;
        while (demuxPacketChannel->tryPop(packetBuffer)) {
            if (!packetBuffer) {
                continue;
            }

            if (packetBuffer->isEof()) {
                stats.eofSeen = true;
                auto decoderEofPush = decodePacketChannel->push(packetBuffer);
                if (!decoderEofPush) {
                    return failStatus("push decoder eof", decoderEofPush);
                }
                auto decoderDrain = decodeRuntime->process(runtime.context());
                if (!decoderDrain) {
                    return failStatus("drain video decoder", decoderDrain);
                }
                break;
            }

            if (packetBuffer->payloadKind() != MediaPayloadKind::Packet) {
                continue;
            }

            ++stats.demuxPackets;
            const auto* ffmpegPacket = dynamic_cast<const FFmpegPacketBuffer*>(packetBuffer.get());
            const AVPacket* packet = ffmpegPacket ? ffmpegPacket->packet() : nullptr;
            if (!packet || packet->stream_index != videoStreamIndex) {
                ++stats.skippedPackets;
                continue;
            }

            ++stats.videoPackets;
            auto decoderPacketPush = decodePacketChannel->push(packetBuffer);
            if (!decoderPacketPush) {
                return failStatus("push video packet to decoder", decoderPacketPush);
            }
            auto decodeStatus = decodeRuntime->process(runtime.context());
            if (!decodeStatus) {
                return failStatus("video decode process", decodeStatus);
            }

            MediaBufferRef frameBuffer;
            while (stats.inputFrames < targetFrames && decodedFrameChannel->tryPop(frameBuffer)) {
                if (!frameBuffer || frameBuffer->payloadKind() != MediaPayloadKind::Frame) {
                    continue;
                }
                auto encodeStatus = encodeDecodedFrame(std::move(frameBuffer));
                if (!encodeStatus) {
                    return failStatus("encode decoded frame", encodeStatus);
                }
            }
        }
    }

    if (!stats.encoderBound) {
        return fail("encoder was not initialized; no decoded frames reached encoder");
    }

    auto encoderEof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    if (!encoderEof) {
        return fail("make encoder eof: " + encoderEof.error().describe());
    }
    auto encoderEofPush = encodeFrameChannel->push(encoderEof.value());
    if (!encoderEofPush) {
        return failStatus("push encoder eof", encoderEofPush);
    }
    auto encoderDrain = encodeRuntime->process(runtime.context());
    if (!encoderDrain) {
        return failStatus("drain video encoder", encoderDrain);
    }
    collectEncodedPackets(*encodedPacketChannel, stats);

    if (stats.inputFrames == 0) {
        return fail("no frames were sent to encoder");
    }
    if (stats.encodedPackets == 0) {
        return fail("no encoded packets were produced");
    }
    if (stats.inputFrames < targetFrames && !stats.eofSeen) {
        return fail("target frames not reached before max demux packet limit; increase max-demux-packets");
    }

    const bool targetReached = stats.inputFrames >= targetFrames;
    std::cout << "graph encode probe ok: "
              << "input_frames=" << stats.inputFrames
              << ", encoded_packets=" << stats.encodedPackets
              << ", key_packets=" << stats.keyPackets
              << ", encoded_bytes=" << stats.encodedBytes
              << ", target_frames=" << targetFrames
              << ", target_reached=" << (targetReached ? "true" : "false")
              << ", eof=" << (stats.eofSeen ? "true" : "false")
              << ", control_buffers=" << stats.controlBuffers
              << ", first_frame_pts=" << (stats.firstFrameSet ? std::to_string(stats.firstFramePts) : std::string("n/a"))
              << ", last_frame_pts=" << (stats.firstFrameSet ? std::to_string(stats.lastFramePts) : std::string("n/a"))
              << ", first_packet_pts=" << (stats.firstPacketSet ? std::to_string(stats.firstPacketPts) : std::string("n/a"))
              << ", last_packet_pts=" << (stats.firstPacketSet ? std::to_string(stats.lastPacketPts) : std::string("n/a"))
              << ", width=" << stats.width
              << ", height=" << stats.height
              << ", format=" << stats.pixelFormat
              << ", encoder=" << stats.encoderName
              << ", codec=" << stats.codecName
              << ", demux_iterations=" << stats.demuxIterations
              << ", demux_packets=" << stats.demuxPackets
              << ", video_packets=" << stats.videoPackets
              << ", skipped_packets=" << stats.skippedPackets
              << ", video_stream_index=" << videoStreamIndex
              << ", max_demux_packets=" << maxDemuxPackets
              << '\n';

    return 0;
}
