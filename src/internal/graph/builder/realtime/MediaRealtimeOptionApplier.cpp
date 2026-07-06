#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpUrl.h"

#include <cctype>
#include <sstream>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeOptionApplier";

const char* inputModeName(MediaRealtimeInputMode mode) noexcept
{
    switch (mode) {
    case MediaRealtimeInputMode::Url:
        return "url";
    case MediaRealtimeInputMode::RawRtp:
        return "raw_rtp";
    }
    return "url";
}

std::string effectiveInputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.input.url.empty() ? options.input.url : options.inputUrl;
}

std::string effectiveOutputUrl(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.outputUrl.empty()
        ? options.outputUrl
        : std::string("rtp://") + options.output.host + ":" + std::to_string(options.output.basePort);
}

std::string effectiveSdpPath(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.output.sdpPath.empty() ? options.output.sdpPath : options.sdpPath;
}

std::string upperAscii(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::string rtpmapCodecName(const std::string& codecName)
{
    const std::string upper = upperAscii(codecName);
    if (upper == "H264" || upper == "HEVC" || upper == "VP8" || upper == "VP9") {
        return upper;
    }
    if (upper == "AAC") {
        return "MPEG4-GENERIC";
    }
    if (upper == "OPUS") {
        return "opus";
    }
    return codecName;
}

const char* mediaName(MediaStreamKind kind) noexcept
{
    return kind == MediaStreamKind::Audio ? "audio" : "video";
}

std::string synthesizeSdpFromHints(const MediaRealtimeGraphBuilderOptions& options)
{
    if (options.input.codecHints.empty()) {
        return {};
    }

    const std::size_t basePort = parseRealtimeRtpUrlPort(effectiveInputUrl(options)).value_or(0);
    std::ostringstream sdp;
    sdp << "v=0\r\n"
        << "o=- 0 0 IN IP4 127.0.0.1\r\n"
        << "s=MediaTranscode RTP input\r\n"
        << "c=IN IP4 0.0.0.0\r\n"
        << "t=0 0\r\n";

    std::size_t hintIndex = 0;
    for (const MediaRtpCodecHint& hint : options.input.codecHints) {
        const std::size_t port = basePort == 0 ? 0 : basePort + hintIndex * 2;
        sdp << "m=" << mediaName(hint.streamKind) << ' ' << port << " RTP/AVP " << hint.payloadType << "\r\n";
        sdp << "a=rtpmap:" << hint.payloadType << ' ' << rtpmapCodecName(hint.codecName) << '/'
            << hint.clockRate;
        if (hint.streamKind == MediaStreamKind::Audio && hint.channels > 1) {
            sdp << '/' << hint.channels;
        }
        sdp << "\r\n";
        if (!hint.fmtp.empty()) {
            sdp << "a=fmtp:" << hint.payloadType << ' ' << hint.fmtp << "\r\n";
        }
        ++hintIndex;
    }
    return sdp.str();
}

std::string effectiveInputSdpText(const MediaRealtimeGraphBuilderOptions& options)
{
    return !options.input.sdpText.empty() ? options.input.sdpText : synthesizeSdpFromHints(options);
}

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

} // namespace

::media::Result<void> MediaRealtimeOptionApplier::applyInputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "url", effectiveInputUrl(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.mode", inputModeName(options.input.mode)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.sdp_text", effectiveInputSdpText(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.sdp_path", options.input.sdpPath); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.read_timeout_ms", std::to_string(options.input.readTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.reconnect", options.input.reconnect ? "1" : "0"); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.max_reconnect_attempts", std::to_string(options.input.maxReconnectAttempts)); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyOutputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "url", effectiveOutputUrl(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.host", options.output.host); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.base_port", std::to_string(options.output.basePort)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.video_port", std::to_string(options.output.videoRtpPort)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.audio_port", std::to_string(options.output.audioRtpPort)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.ttl", std::to_string(options.output.ttl)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.write_timeout_ms", std::to_string(options.output.writeTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "sdp.path", effectiveSdpPath(options)); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applySdpWriterOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeGraphBuilderOptions& options)
{
    if (auto status = setOption(graph, nodeId, "path", effectiveSdpPath(options)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.host", options.output.host); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.base_port", std::to_string(options.output.basePort)); !status) return status;
    if (!options.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", options.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
