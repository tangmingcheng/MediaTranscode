#include "internal/graph/planner/capability/MediaDecoderInputRetentionAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace media::ffmpeg::graph {
std::optional<MediaDecoderInputRetention>
MediaDecoderInputRetentionAdapter::readAfterOpen(
    const AVCodecContext& context, std::string_view hardwareBackend)
{
    if (!context.codec || !context.codec->name || context.thread_count <= 0) return {};
    const std::string_view name(context.codec->name);
    const auto workers = (context.active_thread_type & FF_THREAD_FRAME)
        ? static_cast<std::uint64_t>(context.thread_count) : 0;
    // decode.c moves buffer_pkt through the BSF; these are aliases, not extra
    // allocations. pthread_frame owns one submitted avpkt per frame worker.
    MediaDecoderInputRetention facts{context.thread_count, context.thread_type,
                                    1, 0, 0, {}};
    if (hardwareBackend.empty() && workers == 0 &&
        context.codec_type == AVMEDIA_TYPE_AUDIO && (name == "aac" || name == "aac_fixed" || name == "opus")) {
        // decode.c owns buffer_pkt and may retain an incompletely consumed
        // in_pkt between simple callback calls. AAC returns consumed bytes;
        // native Opus returns the complete avpkt->size and keeps no AVPacket.
        // No frame workers or codec-private compressed-packet owner exists.
        facts.serialPrivatePackets = name == "opus" ? 0 : 1;
        facts.authority = name == "opus"
            ? "ffmpeg-native-opus-full-packet-simple-callback+decode-buffer-pkt"
            : "ffmpeg-native-aac-partial-simple-callback+decode-buffer-pkt+in-pkt";
        return facts;
    }
    if ((name == "h264" || name == "hevc") &&
        (hardwareBackend.empty() || hardwareBackend == "cuda" ||
         hardwareBackend == "d3d11va" || hardwareBackend == "dxva2" ||
         hardwareBackend == "vaapi" || hardwareBackend == "videotoolbox")) {
        if (name == "h264") {
            // The simple video callback consumes and unrefs its entire packet.
            facts.frameWorkerPackets = workers;
            facts.authority = "ffmpeg-h264-simple-callback+frame-thread-packet-handoff";
        } else {
            // HEVC receive_frame can emit its output FIFO before fetching a new
            // packet: each worker may retain old in_pkt plus its next avpkt.
            facts.frameWorkerPackets = workers * 2;
            facts.serialPrivatePackets = workers == 0 ? 1 : 0;
            facts.authority = "ffmpeg-hevc-receive-fifo+frame-thread-packet-handoff";
        }
        // Native hardware adapters borrow the packet only during decoding:
        // DXVA copies to GetDecoderBuffer, VAAPI vaCreateBuffer copies data to
        // server storage, and VideoToolbox builds a private bitstream copy.
        facts.authority += hardwareBackend.empty() ? ":software" : ":" + std::string(hardwareBackend);
        return facts;
    }
    if ((name == "h264_cuvid" || name == "hevc_cuvid") && workers == 0) {
        // cuvid_decode_frame unrefs ctx->pkt immediately after parse submission.
        facts.authority = "ffmpeg-cuvid-synchronous-parse-packet-release";
        return facts;
    }
    if ((name == "h264_rkmpp" || name == "hevc_rkmpp") && workers == 0) {
        if (!MediaRkmppDependencyIdentity::matchesRuntime()) return {};
        // The verified rkmpp_decode_receive_frame has one last_pkt, released
        // unconditionally by decoder close in the deployed ownership fix.
        // EAGAIN may return a decoded frame while retaining that packet; a
        // successful put_packet immediately unrefs it. MPP c08762ebf copies
        // non-MppBuffer input in mpp_put_packet -> mpp_packet_copy_init before
        // submission, so its internal bitstream does not pin our AVBuffer.
        // The common decode buffer_pkt handoff remains a separate slot.
        facts.serialPrivatePackets = 1;
        facts.authority = std::string(MediaRkmppDependencyIdentity::sourceRevision) +
            ":last_pkt-close-release+mpp-c08762ebf-copy-on-put";
        return facts;
    }
    return {};
}
} // namespace media::ffmpeg::graph
