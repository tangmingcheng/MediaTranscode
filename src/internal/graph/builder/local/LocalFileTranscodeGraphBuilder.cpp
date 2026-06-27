#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"

namespace media::ffmpeg::graph {

MediaGraph LocalFileTranscodeGraphBuilder::build(const LocalFileTranscodeOptions& opt)
{
    MediaGraph graph;
    MediaGraphBuilder b(graph);

    auto fileIn = b.addNode(MediaNodeKind::FileInput, "file_input");
    b.addOutput(fileIn, "container_out", MediaStreamKind::Any, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);

    auto demux = b.addNode(MediaNodeKind::Demux, "demux");
    b.addInput(demux, "container_in", MediaStreamKind::Any, MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    b.addOutput(demux, "packet_out", MediaStreamKind::Any, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);

    b.connect(fileIn, "container_out", demux, "container_in");

    MediaNodeId lastVideo = MediaNodeId::invalid();
    MediaNodeId lastAudio = MediaNodeId::invalid();

    if (opt.includeVideo) {
        auto vp = b.addVideoTranscodeBranch({ .prefix = "video", .useHardwareTransfer = opt.useHardwareTransfer });
        b.connect(demux, "packet_out", vp.inputNode, vp.inputPort);
        lastVideo = vp.outputNode;
    }

    if (opt.includeAudio) {
        auto audio = b.addNode(MediaNodeKind::AudioCopy, "audio_copy");
        b.addInput(audio, "packet_in", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
        b.addOutput(audio, "packet_out", MediaStreamKind::Audio, MediaEdgeKind::CopiedPacket, MediaPayloadKind::Packet);

        b.connect(demux, "packet_out", audio, "packet_in");
        lastAudio = audio;
    }

    auto mux = b.addNode(MediaNodeKind::FileMux, "file_mux");
    b.addInput(mux, "video_in", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, false);
    b.addInput(mux, "audio_in", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet, false);
    b.addOutput(mux, "container_out", MediaStreamKind::Any, MediaEdgeKind::MuxedPacket, MediaPayloadKind::Packet);

    if (lastVideo.isValid()) {
        b.connect(lastVideo, "frame_out", mux, "video_in");
    }

    if (lastAudio.isValid()) {
        b.connect(lastAudio, "packet_out", mux, "audio_in");
    }

    return graph;
}

} // namespace media::ffmpeg::graph
