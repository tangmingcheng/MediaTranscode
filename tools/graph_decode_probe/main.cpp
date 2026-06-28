#include "internal/FFmpegRAII.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <cstdlib>
#include <iostream>
#include <string>

extern "C" {
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
    std::cerr << "graph decode probe failed: " << message << '\n';
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
    int videoStreamIndex = invalidMediaStreamIndex;
};

struct DecodeStats {
    std::size_t demuxIterations = 0;
    std::size_t demuxPackets = 0;
    std::size_t videoPackets = 0;
    std::size_t skippedPackets = 0;
    std::size_t decodedFrames = 0;
    std::size_t controlBuffers = 0;
    bool eofSeen = false;
    bool firstFrameSet = false;
    MediaTimeValue firstFramePts = 0;
    MediaTimeValue lastFramePts = 0;
    int firstWidth = 0;
    int firstHeight = 0;
    std::string firstPixelFormat = "unknown";
};

void collectFrames(MediaChannel& frameChannel, DecodeStats& stats)
{
    MediaBufferRef frameBuffer;
    while (frameChannel.tryPop(frameBuffer)) {
        if (!frameBuffer) {
            continue;
        }

        if (frameBuffer->isEof() || frameBuffer->isFlush()) {
            ++stats.controlBuffers;
            continue;
        }

        if (frameBuffer->payloadKind() != MediaPayloadKind::Frame) {
            continue;
        }

        ++stats.decodedFrames;
        stats.lastFramePts = frameBuffer->pts();

        if (!stats.firstFrameSet) {
            stats.firstFramePts = frameBuffer->pts();
            stats.firstFrameSet = true;

            const auto* ffmpegFrame = dynamic_cast<const FFmpegFrameBuffer*>(frameBuffer.get());
            const AVFrame* frame = ffmpegFrame ? ffmpegFrame->frame() : nullptr;
            if (frame) {
                stats.firstWidth = frame->width;
                stats.firstHeight = frame->height;
                const char* formatName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
                stats.firstPixelFormat = formatName ? formatName : "unknown";
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
    setup.codecContext = std::move(codecContext);
    return ::media::Result<VideoDecoderSetup>::success(std::move(setup));
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_decode_probe.exe <input-media-file> [target-frames] [max-demux-packets]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    const std::size_t targetFrames = argc >= 3 ? parsePositiveSize(argv[2], 100) : 100;
    std::size_t maxDemuxPackets = targetFrames * 16 + 256;
    if (maxDemuxPackets < 512) {
        maxDemuxPackets = 512;
    }
    if (argc >= 4) {
        maxDemuxPackets = parsePositiveSize(argv[3], maxDemuxPackets);
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
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "video-codec-source");
    const MediaNodeId packetSource = graph.addNode(MediaNodeKind::DebugDump, "video-packet-source");
    const MediaNodeId videoDecode = graph.addNode(MediaNodeKind::VideoDecode, "video-decode");
    const MediaNodeId frameSink = graph.addNode(MediaNodeKind::DebugDump, "frame-sink");

    graph.setNodeOption(fileInput, "path", inputPath);

    graph.addOutputPort(fileInput,
                        "format",
                        MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::FormatContext,
                        true,
                        false);
    graph.addInputPort(demux,
                       "format",
                       MediaStreamKind::Metadata,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::FormatContext,
                       true,
                       false);
    graph.addOutputPort(demux,
                        "packet",
                        MediaStreamKind::Any,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        true);
    graph.addInputPort(demuxPacketSink,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.addOutputPort(codecSource,
                        "codec",
                        MediaStreamKind::Video,
                        MediaEdgeKind::Metadata,
                        MediaPayloadKind::CodecContext,
                        true,
                        false);
    graph.addInputPort(videoDecode,
                       "codec",
                       MediaStreamKind::Video,
                       MediaEdgeKind::Metadata,
                       MediaPayloadKind::CodecContext,
                       true,
                       false);

    graph.addOutputPort(packetSource,
                        "packet",
                        MediaStreamKind::Video,
                        MediaEdgeKind::InputPacket,
                        MediaPayloadKind::Packet,
                        true,
                        true);
    graph.addInputPort(videoDecode,
                       "packet",
                       MediaStreamKind::Video,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.addOutputPort(videoDecode,
                        "frame",
                        MediaStreamKind::Video,
                        MediaEdgeKind::RawFrame,
                        MediaPayloadKind::Frame,
                        true,
                        true);
    graph.addInputPort(frameSink,
                       "frame",
                       MediaStreamKind::Video,
                       MediaEdgeKind::RawFrame,
                       MediaPayloadKind::Frame,
                       true,
                       true);

    graph.connect(fileInput, "format", demux, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(demux, "packet", demuxPacketSink, "packet", "demux-to-packet-sink", queuePolicy(128));
    graph.connect(codecSource, "codec", videoDecode, "codec", "codec-to-video-decode", queuePolicy(1));
    graph.connect(packetSource, "packet", videoDecode, "packet", "packet-to-video-decode", queuePolicy(128));
    graph.connect(videoDecode, "frame", frameSink, "frame", "video-decode-to-frame-sink", queuePolicy(128));

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
    if (!fileRuntime) {
        return fail("file input runtime node not found");
    }
    if (!demuxRuntime) {
        return fail("demux runtime node not found");
    }
    if (!decodeRuntime) {
        return fail("video decode runtime node not found");
    }

    MediaChannel* demuxPacketChannel = runtime.context().findOutputChannel(demux, "packet");
    MediaChannel* decodeCodecChannel = runtime.context().findInputChannel(videoDecode, "codec");
    MediaChannel* decodePacketChannel = runtime.context().findInputChannel(videoDecode, "packet");
    MediaChannel* frameChannel = runtime.context().findOutputChannel(videoDecode, "frame");
    if (!demuxPacketChannel) {
        return fail("demux packet output channel not found");
    }
    if (!decodeCodecChannel) {
        return fail("decode codec input channel not found");
    }
    if (!decodePacketChannel) {
        return fail("decode packet input channel not found");
    }
    if (!frameChannel) {
        return fail("decode frame output channel not found");
    }

    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(decoderSetup.value().codecContext));
    if (!codecBuffer) {
        return fail("wrap codec context: " + codecBuffer.error().describe());
    }

    auto codecPushStatus = decodeCodecChannel->push(codecBuffer.value());
    if (!codecPushStatus) {
        return failStatus("push codec context", codecPushStatus);
    }

    auto bindStatus = decodeRuntime->process(runtime.context());
    if (!bindStatus) {
        return failStatus("bind video decoder", bindStatus);
    }

    auto fileStatus = fileRuntime->process(runtime.context());
    if (!fileStatus) {
        return failStatus("file input process", fileStatus);
    }

    DecodeStats stats;

    while (stats.decodedFrames < targetFrames &&
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
                auto eofPushStatus = decodePacketChannel->push(packetBuffer);
                if (!eofPushStatus) {
                    return failStatus("push decoder eof", eofPushStatus);
                }
                auto drainStatus = decodeRuntime->process(runtime.context());
                if (!drainStatus) {
                    return failStatus("drain video decoder", drainStatus);
                }
                collectFrames(*frameChannel, stats);
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
            auto packetPushStatus = decodePacketChannel->push(packetBuffer);
            if (!packetPushStatus) {
                return failStatus("push video packet", packetPushStatus);
            }

            auto decodeStatus = decodeRuntime->process(runtime.context());
            if (!decodeStatus) {
                return failStatus("video decode process", decodeStatus);
            }

            collectFrames(*frameChannel, stats);
            if (stats.decodedFrames >= targetFrames) {
                break;
            }
        }
    }

    if (stats.videoPackets == 0) {
        return fail("no video packets reached decoder");
    }
    if (stats.decodedFrames == 0) {
        return fail("no decoded frames were produced");
    }
    if (stats.decodedFrames < targetFrames && !stats.eofSeen) {
        return fail("target frames not reached before max demux packet limit; increase max-demux-packets");
    }

    const bool targetReached = stats.decodedFrames >= targetFrames;
    std::cout << "graph decode probe ok: "
              << "demux_iterations=" << stats.demuxIterations
              << ", demux_packets=" << stats.demuxPackets
              << ", video_packets=" << stats.videoPackets
              << ", skipped_packets=" << stats.skippedPackets
              << ", decoded_frames=" << stats.decodedFrames
              << ", target_frames=" << targetFrames
              << ", target_reached=" << (targetReached ? "true" : "false")
              << ", eof=" << (stats.eofSeen ? "true" : "false")
              << ", control_buffers=" << stats.controlBuffers
              << ", first_frame_pts=" << (stats.firstFrameSet ? std::to_string(stats.firstFramePts) : std::string("n/a"))
              << ", last_frame_pts=" << (stats.firstFrameSet ? std::to_string(stats.lastFramePts) : std::string("n/a"))
              << ", width=" << stats.firstWidth
              << ", height=" << stats.firstHeight
              << ", format=" << stats.firstPixelFormat
              << ", video_stream_index=" << videoStreamIndex
              << ", max_demux_packets=" << maxDemuxPackets
              << '\n';

    return 0;
}
