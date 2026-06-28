#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace media::ffmpeg::graph;

const char* streamKindName(MediaStreamKind kind) noexcept
{
    switch (kind) {
    case MediaStreamKind::Video: return "video";
    case MediaStreamKind::Audio: return "audio";
    case MediaStreamKind::Subtitle: return "subtitle";
    case MediaStreamKind::Data: return "data";
    case MediaStreamKind::Attachment: return "attachment";
    case MediaStreamKind::Control: return "control";
    case MediaStreamKind::Metadata: return "metadata";
    case MediaStreamKind::Any: return "any";
    case MediaStreamKind::Unknown:
    default: return "unknown";
    }
}

int fail(const std::string& message)
{
    std::cerr << "graph file input probe failed: " << message << '\n';
    return 1;
}

int failStatus(const std::string& action, const ::media::Status& status)
{
    return fail(action + ": " + status.error().describe());
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

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "usage: media_transcode_graph_file_input_probe.exe <input-media-file> [max-packets]\n";
        return 2;
    }

    const std::string inputPath = argv[1];
    int maxPackets = 32;
    if (argc >= 3) {
        maxPackets = std::atoi(argv[2]);
        if (maxPackets <= 0) {
            maxPackets = 32;
        }
    }

    MediaGraph graph;
    const MediaNodeId fileInput = graph.addNode(MediaNodeKind::FileInput, "file-input");
    const MediaNodeId demux = graph.addNode(MediaNodeKind::Demux, "demux");
    const MediaNodeId packetSink = graph.addNode(MediaNodeKind::DebugDump, "packet-sink");

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
    graph.addInputPort(packetSink,
                       "packet",
                       MediaStreamKind::Any,
                       MediaEdgeKind::InputPacket,
                       MediaPayloadKind::Packet,
                       true,
                       true);

    graph.connect(fileInput, "format", demux, "format", "file-input-to-demux", queuePolicy(1));
    graph.connect(demux, "packet", packetSink, "packet", "demux-to-packet-sink", queuePolicy(128));

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
    if (!fileRuntime) {
        return fail("file input runtime node not found");
    }
    if (!demuxRuntime) {
        return fail("demux runtime node not found");
    }

    auto fileStatus = fileRuntime->process(runtime.context());
    if (!fileStatus) {
        return failStatus("file input process", fileStatus);
    }

    MediaChannel* demuxInputChannel = runtime.context().findInputChannel(demux, "format");
    MediaChannel* packetChannel = runtime.context().findOutputChannel(demux, "packet");
    if (!demuxInputChannel) {
        return fail("demux input channel not found");
    }
    if (!packetChannel) {
        return fail("demux packet output channel not found");
    }

    std::size_t packetCount = 0;
    std::size_t videoPackets = 0;
    std::size_t audioPackets = 0;
    std::size_t controlBuffers = 0;
    bool eofSeen = false;
    bool firstPtsSet = false;
    MediaTimeValue firstPts = 0;

    for (int i = 0; i < maxPackets && !eofSeen; ++i) {
        auto demuxStatus = demuxRuntime->process(runtime.context());
        if (!demuxStatus) {
            return failStatus("demux process", demuxStatus);
        }

        MediaBufferRef buffer;
        while (packetChannel->tryPop(buffer)) {
            if (!buffer) {
                continue;
            }

            if (buffer->isEof()) {
                eofSeen = true;
                ++controlBuffers;
                continue;
            }

            if (buffer->payloadKind() != MediaPayloadKind::Packet) {
                continue;
            }

            ++packetCount;
            if (!firstPtsSet) {
                firstPts = buffer->pts();
                firstPtsSet = true;
            }

            if (buffer->streamKind() == MediaStreamKind::Video) {
                ++videoPackets;
            } else if (buffer->streamKind() == MediaStreamKind::Audio) {
                ++audioPackets;
            }

            const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.get());
            if (packetBuffer && packetBuffer->packet()) {
                // Touch the wrapped AVPacket to ensure the graph buffer owns a real packet.
                (void)packetBuffer->packet()->stream_index;
            }
        }
    }

    if (packetCount == 0 && !eofSeen) {
        return fail("no packets were produced");
    }

    std::cout << "graph file input probe ok: "
              << "input_opened=true"
              << ", packets=" << packetCount
              << ", video_packets=" << videoPackets
              << ", audio_packets=" << audioPackets
              << ", control_buffers=" << controlBuffers
              << ", first_pts=" << (firstPtsSet ? std::to_string(firstPts) : std::string("n/a"))
              << ", eof=" << (eofSeen ? "true" : "false")
              << ", max_packets=" << maxPackets
              << '\n';

    if (packetCount > 0) {
        std::cout << "graph file input probe first packet stream kind: "
                  << streamKindName(videoPackets > 0 ? MediaStreamKind::Video :
                                    audioPackets > 0 ? MediaStreamKind::Audio :
                                    MediaStreamKind::Unknown)
                  << '\n';
    }

    return 0;
}
