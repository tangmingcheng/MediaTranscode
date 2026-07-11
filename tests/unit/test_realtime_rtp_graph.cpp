#include "common/TestAssert.h"
#include "common/GraphRuntimeTestSupport.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/nodes/packet/AvPacketStartBarrierNode.h"
#include "internal/graph/nodes/packet/PacketStartGateNode.h"
#include "internal/graph/nodes/mux/RtpMuxStateMachine.h"
#include "internal/graph/nodes/audio/AudioMonotonicTimestamp.h"
#include "internal/graph/nodes/audio/AudioDecodeNode.h"
#include "internal/graph/nodes/audio/AudioEncodeNode.h"
#include "internal/graph/nodes/audio/AudioResampleNode.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/queue/MediaBlockingQueue.h"
#include "internal/graph/runtime/queue/MediaSpscRingQueue.h"
#include "internal/graph/runtime/threading/MediaGraphWorker.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../../tools/common/GraphCliSupport.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <chrono>
#include <memory>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using media_transcode::test::TestContext;
using media_transcode::test::makePacketBuffer;
using namespace media::ffmpeg::graph;

std::string sampleVideoPath()
{
    return (std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
            "tests" /
            "samples" /
            "sample_h264_aac_2560x1440.mp4").string();
}

std::uint32_t testProcessId() noexcept
{
#ifdef _WIN32
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

std::uint16_t localMpegTsUdpPort(std::uint16_t offset) noexcept
{
    const std::uint32_t base = 20000 + ((testProcessId() * 17) % 20000);
    return static_cast<std::uint16_t>(base + offset);
}

std::uint16_t findAvailableUdpPort() noexcept
{
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return localMpegTsUdpPort(0);
    }

    SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        WSACleanup();
        return localMpegTsUdpPort(0);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closesocket(socketHandle);
        WSACleanup();
        return localMpegTsUdpPort(0);
    }

    int addressLength = sizeof(address);
    std::uint16_t port = localMpegTsUdpPort(0);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0) {
        port = ntohs(address.sin_port);
    }

    closesocket(socketHandle);
    WSACleanup();
    return port;
#else
    const int socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketHandle < 0) {
        return localMpegTsUdpPort(0);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(socketHandle);
        return localMpegTsUdpPort(0);
    }

    socklen_t addressLength = sizeof(address);
    std::uint16_t port = localMpegTsUdpPort(0);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0) {
        port = ntohs(address.sin_port);
    }

    close(socketHandle);
    return port;
#endif
}

std::uint16_t mpegTsUdpInputPort() noexcept
{
    static const std::uint16_t port = findAvailableUdpPort();
    return port;
}

std::uint16_t mpegTsUdpOutputPort() noexcept
{
    const std::uint16_t inputPort = mpegTsUdpInputPort();
    if (inputPort <= 65533) {
        return static_cast<std::uint16_t>(inputPort + 2);
    }
    return localMpegTsUdpPort(2);
}

std::string mpegTsUdpInputUrl()
{
    return "udp://127.0.0.1:" + std::to_string(mpegTsUdpInputPort()) + "?fifo_size=1000000&overrun_nonfatal=1";
}

std::string mpegTsUdpOutputUrl()
{
    return "udp://127.0.0.1:" + std::to_string(mpegTsUdpOutputPort());
}

std::filesystem::path ffmpegExecutablePath()
{
#ifdef _WIN32
    const std::filesystem::path bundled = "D:/mabs/local64/bin-video/ffmpeg.exe";
    if (std::filesystem::exists(bundled)) {
        return bundled;
    }
#else
    const std::filesystem::path bundled = "/usr/bin/ffmpeg";
    if (std::filesystem::exists(bundled)) {
        return bundled;
    }
#endif
    return "ffmpeg";
}

#ifdef _WIN32
std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

class LocalMpegTsUdpSource final {
public:
    static ::media::Result<LocalMpegTsUdpSource> start()
    {
        const std::filesystem::path ffmpeg = ffmpegExecutablePath();
        const std::string command =
            "\"" + ffmpeg.string() + "\" -hide_banner -loglevel error -re -stream_loop -1 -i \"" +
            sampleVideoPath() + "\" -map 0:v:0 -map 0:a:0? -c copy -f mpegts \"udp://127.0.0.1:" +
            std::to_string(mpegTsUdpInputPort()) + "?pkt_size=1316\"";

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        std::wstring commandLine = widenAscii(command);

        if (!CreateProcessW(nullptr,
                            commandLine.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            CREATE_NO_WINDOW,
                            nullptr,
                            nullptr,
                            &startupInfo,
                            &processInfo)) {
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Failed to start local FFmpeg MPEG-TS UDP source"));
        }

        CloseHandle(processInfo.hThread);
        LocalMpegTsUdpSource source(processInfo.hProcess);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        return ::media::Result<LocalMpegTsUdpSource>::success(std::move(source));
    }

    LocalMpegTsUdpSource(LocalMpegTsUdpSource&& other) noexcept
        : m_process(other.m_process)
    {
        other.m_process = nullptr;
    }

    LocalMpegTsUdpSource& operator=(LocalMpegTsUdpSource&& other) noexcept
    {
        if (this != &other) {
            stop();
            m_process = other.m_process;
            other.m_process = nullptr;
        }
        return *this;
    }

    LocalMpegTsUdpSource(const LocalMpegTsUdpSource&) = delete;
    LocalMpegTsUdpSource& operator=(const LocalMpegTsUdpSource&) = delete;

    ~LocalMpegTsUdpSource()
    {
        stop();
    }

private:
    explicit LocalMpegTsUdpSource(HANDLE process)
        : m_process(process)
    {
    }

    void stop() noexcept
    {
        if (!m_process) {
            return;
        }
        TerminateProcess(m_process, 0);
        WaitForSingleObject(m_process, 2000);
        CloseHandle(m_process);
        m_process = nullptr;
    }

    HANDLE m_process = nullptr;
};
#else
class LocalMpegTsUdpSource final {
public:
    static ::media::Result<LocalMpegTsUdpSource> start()
    {
        const std::filesystem::path ffmpeg = ffmpegExecutablePath();
        const std::string input = sampleVideoPath();
        const std::string output = "udp://127.0.0.1:" + std::to_string(mpegTsUdpInputPort()) + "?pkt_size=1316";
        const pid_t pid = fork();
        if (pid < 0) {
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Failed to fork local FFmpeg MPEG-TS UDP source"));
        }
        if (pid == 0) {
            execlp(ffmpeg.string().c_str(),
                   ffmpeg.filename().string().c_str(),
                   "-hide_banner",
                   "-loglevel",
                   "error",
                   "-re",
                   "-stream_loop",
                   "-1",
                   "-i",
                   input.c_str(),
                   "-map",
                   "0:v:0",
                   "-map",
                   "0:a:0?",
                   "-c",
                   "copy",
                   "-f",
                   "mpegts",
                   output.c_str(),
                   static_cast<char*>(nullptr));
            _exit(127);
        }

        LocalMpegTsUdpSource source(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            source.m_process = -1;
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Local FFmpeg MPEG-TS UDP source exited before probe"));
        }
        return ::media::Result<LocalMpegTsUdpSource>::success(std::move(source));
    }

    LocalMpegTsUdpSource(LocalMpegTsUdpSource&& other) noexcept
        : m_process(other.m_process)
    {
        other.m_process = -1;
    }

    LocalMpegTsUdpSource& operator=(LocalMpegTsUdpSource&& other) noexcept
    {
        if (this != &other) {
            stop();
            m_process = other.m_process;
            other.m_process = -1;
        }
        return *this;
    }

    LocalMpegTsUdpSource(const LocalMpegTsUdpSource&) = delete;
    LocalMpegTsUdpSource& operator=(const LocalMpegTsUdpSource&) = delete;

    ~LocalMpegTsUdpSource()
    {
        stop();
    }

private:
    explicit LocalMpegTsUdpSource(pid_t process)
        : m_process(process)
    {
    }

    void stop() noexcept
    {
        if (m_process <= 0) {
            return;
        }
        kill(m_process, SIGTERM);
        waitpid(m_process, nullptr, 0);
        m_process = -1;
    }

    pid_t m_process = -1;
};
#endif

MediaRealtimeRtpTranscodeRequest validRealtimeOptions()
{
    MediaRealtimeRtpTranscodeRequest options;
    options.input.type = RealtimeInputType::Url;
    options.input.streamLayout = RealtimeInputStreamLayout::SessionDescribed;
    options.input.url = sampleVideoPath();
    options.input.rtspTransport = "tcp";
    options.input.openTimeoutMs = 5000;
    options.input.readTimeoutMs = 5000;
    options.input.analyzeDurationUs = 500000;
    options.input.probeSizeBytes = 512 * 1024;
    options.input.lowLatency = true;
    options.output.host = "127.0.0.1";
    options.output.basePort = 5004;
    options.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    options.output.sdpPath = "realtime-test.sdp";
    options.output.packetSize = 1200;
    options.parameters.execution.includeAudio = false;
    options.parameters.execution.disableHardware = true;
    options.parameters.queues.metadata = 1;
    options.parameters.queues.packet = 256;
    options.parameters.queues.frame = 128;
    options.parameters.queues.mux = 256;
    options.parameters.video.codecName = "h264";
    return options;
}

const MediaNode* findNodeByKind(const MediaGraph& graph, MediaNodeKind kind)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == kind) {
            return &node;
        }
    }
    return nullptr;
}

const MediaNode* findNodeByName(const MediaGraph& graph, const std::string& name)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.name == name) {
            return &node;
        }
    }
    return nullptr;
}

std::size_t countNodesByKind(const MediaGraph& graph, MediaNodeKind kind)
{
    std::size_t count = 0;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == kind) {
            ++count;
        }
    }
    return count;
}

std::size_t countOccurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

const MediaEdge* findEdgeBetweenKinds(const MediaGraph& graph,
                                      MediaNodeKind fromKind,
                                      MediaNodeKind toKind,
                                      MediaEdgeKind edgeKind)
{
    for (const MediaEdge& edge : graph.edges()) {
        const MediaNode* from = graph.findNode(edge.from.nodeId);
        const MediaNode* to = graph.findNode(edge.to.nodeId);
        if (from && to && from->kind == fromKind && to->kind == toKind && edge.edgeKind == edgeKind) {
            return &edge;
        }
    }
    return nullptr;
}

const MediaEdge* findEdgeByNames(const MediaGraph& graph,
                                 const std::string& fromName,
                                 const std::string& fromPort,
                                 const std::string& toName,
                                 const std::string& toPort)
{
    for (const MediaEdge& edge : graph.edges()) {
        const MediaNode* from = graph.findNode(edge.from.nodeId);
        const MediaNode* to = graph.findNode(edge.to.nodeId);
        const MediaPort* sourcePort = graph.findPort(edge.from.portId);
        const MediaPort* targetPort = graph.findPort(edge.to.portId);
        if (from && to &&
            sourcePort && targetPort &&
            from->name == fromName &&
            sourcePort->name == fromPort &&
            to->name == toName &&
            targetPort->name == toPort) {
            return &edge;
        }
    }
    return nullptr;
}

const MediaEdge* findInputEdgeToNode(const MediaGraph& graph,
                                     const std::string& toName,
                                     const std::string& toPort,
                                     MediaStreamKind streamKind,
                                     MediaEdgeKind edgeKind)
{
    for (const MediaEdge& edge : graph.edges()) {
        const MediaNode* to = graph.findNode(edge.to.nodeId);
        const MediaPort* targetPort = graph.findPort(edge.to.portId);
        if (to &&
            targetPort &&
            to->name == toName &&
            targetPort->name == toPort &&
            edge.streamKind == streamKind &&
            edge.edgeKind == edgeKind) {
            return &edge;
        }
    }
    return nullptr;
}

const MediaEdge* findInputEdgeToNodeWithPayload(const MediaGraph& graph,
                                                const std::string& toName,
                                                const std::string& toPort,
                                                MediaStreamKind streamKind,
                                                MediaPayloadKind payloadKind)
{
    for (const MediaEdge& edge : graph.edges()) {
        const MediaNode* to = graph.findNode(edge.to.nodeId);
        const MediaPort* targetPort = graph.findPort(edge.to.portId);
        if (to &&
            targetPort &&
            to->name == toName &&
            targetPort->name == toPort &&
            edge.streamKind == streamKind &&
            edge.payloadKind == payloadKind) {
            return &edge;
        }
    }
    return nullptr;
}

bool isRealtimeDataEdge(MediaEdgeKind kind) noexcept
{
    return kind == MediaEdgeKind::InputPacket ||
           kind == MediaEdgeKind::RawFrame ||
           kind == MediaEdgeKind::HardwareFrame ||
           kind == MediaEdgeKind::SoftwareFrame ||
           kind == MediaEdgeKind::EncodedPacket ||
           kind == MediaEdgeKind::CopiedPacket ||
           kind == MediaEdgeKind::MuxedPacket;
}

bool isAudioDataEdge(const MediaEdge& edge) noexcept
{
    return edge.streamKind == MediaStreamKind::Audio && isRealtimeDataEdge(edge.edgeKind);
}

std::size_t countRealtimeDataEdgesWithQueueMode(const MediaGraph& graph, MediaQueueMode mode)
{
    std::size_t count = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (isRealtimeDataEdge(edge.edgeKind) && edge.policy.queuePolicy.mode == mode) {
            ++count;
        }
    }
    return count;
}

std::size_t countAudioDataEdgesWithOverflowPolicy(const MediaGraph& graph,
                                                  MediaQueueOverflowPolicy policy)
{
    std::size_t count = 0;
    for (const MediaEdge& edge : graph.edges()) {
        if (isAudioDataEdge(edge) && edge.policy.queuePolicy.overflowPolicy == policy) {
            ++count;
        }
    }
    return count;
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void expectPlannerInvalidArgument(TestContext& ctx, const MediaRealtimeRtpTranscodeRequest& options)
{
    if (options.input.type && *options.input.type == RealtimeInputType::RtpPort) {
        const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
        EXPECT_FALSE(ctx, plan);
        if (!plan) EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
        return;
    }
    const auto status = MediaRealtimeRtpTranscodePlanner::validateRealtimeRequestNoIo(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

::media::Result<MediaRealtimeRtpTranscodePlan> preparedPlan(
    const MediaRealtimeRtpTranscodeRequest& request)
{
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(request);
    if (!preflight) {
        return ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(preflight.error());
    }
    return ::media::Result<MediaRealtimeRtpTranscodePlan>::success(
        std::move(preflight).value().plan);
}

::media::Result<MediaGraph> preparedGraph(const MediaRealtimeRtpTranscodeRequest& request)
{
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(request);
    if (!preflight) return ::media::Result<MediaGraph>::failure(preflight.error());
    auto executable = MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(std::move(preflight).value());
    if (!executable) return ::media::Result<MediaGraph>::failure(executable.error());
    return ::media::Result<MediaGraph>::success(std::move(executable).value().graph);
}

std::string repositoryFile(const std::string& relativePath)
{
    return readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) / relativePath);
}

::media::ffmpeg::CodecContextPtr makeTestAudioCodecContext(int sampleRate,
                                                           AVSampleFormat sampleFormat,
                                                           int channels)
{
    auto codec = ::media::ffmpeg::makeCodecContext(nullptr);
    if (!codec) {
        return nullptr;
    }
    codec->codec_type = AVMEDIA_TYPE_AUDIO;
    codec->sample_rate = sampleRate;
    codec->sample_fmt = sampleFormat;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_default(&codec->ch_layout, channels);
#else
    codec->channels = channels;
    codec->channel_layout = av_get_default_channel_layout(channels);
#endif
    return codec;
}

::media::Result<MediaBufferRef> makeTestAudioFrame(int sampleRate,
                                                   AVSampleFormat sampleFormat,
                                                   int channels,
                                                   int64_t pts,
                                                   int samples)
{
    auto frame = ::media::ffmpeg::makeFrame();
    if (!frame) {
        return ::media::Result<MediaBufferRef>::failure(
            media::ErrorInfo::allocationFailed("test failed to allocate audio frame"));
    }
    frame->format = sampleFormat;
    frame->sample_rate = sampleRate;
    frame->nb_samples = samples;
    frame->pts = pts;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    av_channel_layout_default(&frame->ch_layout, channels);
#else
    frame->channels = channels;
    frame->channel_layout = av_get_default_channel_layout(channels);
#endif
    const int bufferRet = av_frame_get_buffer(frame.get(), 0);
    if (bufferRet < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            media::ErrorInfo::invalidArgument("test failed to allocate audio frame buffer"));
    }

    auto buffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
    if (!buffer) {
        return buffer;
    }
    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = MediaRational{ 1, sampleRate };
    buffer.value()->setTimeDescriptor(timeDescriptor);
    return buffer;
}

MediaNodeId addAudioResampleHarnessGraph(MediaGraph& graph)
{
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::AudioCodecResolver, "test.codec_source");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::AudioDecode, "test.frame_source");
    const MediaNodeId resample = graph.addNode(MediaNodeKind::AudioResample, "test.audio_resample");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::AudioEncode, "test.audio_sink");

    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, true);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(resample, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext, true, false);
    graph.addInputPort(resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame, true, true);
    graph.addOutputPort(resample, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame, true, true);

    const MediaEdgePolicy policy = MediaGraphBuildSupport::blockingQueuePolicy(8);
    graph.connect(codecSource, "codec", resample, "codec", "test.codec -> resample.codec", policy);
    graph.connect(frameSource, "frame", resample, "frame", "test.frame -> resample.frame", policy);
    graph.connect(resample, "frame", sink, "frame", "test.resample.frame -> sink.frame", policy);
    return resample;
}

::media::Status bindAudioResampleCodec(MediaGraphExecutionContext& execution,
                                       AudioResampleNode& node,
                                       MediaNodeId nodeId,
                                       ::media::ffmpeg::CodecContextPtr codec)
{
    auto codecBuffer = FFmpegBufferFactory::wrapCodecContext(std::move(codec));
    if (!codecBuffer) {
        return ::media::Status::failure(codecBuffer.error());
    }
    MediaChannel* codecInput = execution.findInputChannel(nodeId, "codec");
    if (!codecInput) {
        return ::media::Status::failure(media::ErrorInfo::internalError("test missing codec input channel"));
    }
    auto pushed = codecInput->push(codecBuffer.value());
    if (!pushed) {
        return pushed;
    }
    auto processed = node.process(execution);
    return processed ? ::media::Status::success() : ::media::Status::failure(processed.error());
}

::media::Result<MediaBufferRef> processAudioResampleFrame(MediaGraphExecutionContext& execution,
                                                          AudioResampleNode& node,
                                                          MediaNodeId nodeId,
                                                          const MediaBufferRef& frame)
{
    MediaChannel* frameInput = execution.findInputChannel(nodeId, "frame");
    MediaChannel* frameOutput = execution.findOutputChannel(nodeId, "frame");
    if (!frameInput || !frameOutput) {
        return ::media::Result<MediaBufferRef>::failure(
            media::ErrorInfo::internalError("test missing frame channel"));
    }
    auto pushed = frameInput->push(frame);
    if (!pushed) {
        return ::media::Result<MediaBufferRef>::failure(pushed.error());
    }
    auto processed = node.process(execution);
    if (!processed) {
        return ::media::Result<MediaBufferRef>::failure(processed.error());
    }
    MediaBufferRef output;
    if (!frameOutput->tryPop(output)) {
        return ::media::Result<MediaBufferRef>::failure(
            media::ErrorInfo::internalError("test expected resampled frame output"));
    }
    return ::media::Result<MediaBufferRef>::success(output);
}

void expectTextContains(TestContext& ctx, const std::string& text, const std::string& needle)
{
    EXPECT_TRUE(ctx, text.find(needle) != std::string::npos);
}

void expectTextNotContains(TestContext& ctx, const std::string& text, const std::string& needle)
{
    EXPECT_FALSE(ctx, text.find(needle) != std::string::npos);
}

MediaRealtimeRtpTranscodeRequest validRawRtpOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.type = RealtimeInputType::RtpPort;
    options.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    options.input.url.clear();
    options.input.videoRtp.url = "rtp://127.0.0.1:5004";
    options.input.videoRtp.codecName = "h264";
    options.input.videoRtp.payloadType = 96;
    options.input.videoRtp.clockRate = 90000;
    options.input.videoRtp.fmtp = "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    options.parameters.video.bitrateKbps = 8406;
    return options;
}

MediaRealtimeRtpTranscodeRequest validMpegTsUdpOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.type = RealtimeInputType::MpegTsUdp;
    options.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    options.input.url = mpegTsUdpInputUrl();
    options.input.rtspTransport.clear();
    options.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    options.output.url = mpegTsUdpOutputUrl();
    options.output.host.clear();
    options.output.basePort.reset();
    options.output.sdpPath.clear();
    options.parameters.video.bitrateKbps = 8406;
    return options;
}

MediaRealtimeRtpTranscodeRequest validRawRtpAudioVideoOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.bitrateKbps = 320;
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;
    options.input.audioRtp.url = "rtp://127.0.0.1:5006";
    options.input.audioRtp.codecName = "aac";
    options.input.audioRtp.payloadType = 97;
    options.input.audioRtp.clockRate = 48000;
    options.input.audioRtp.channels = 2;
    options.input.audioRtp.bitrateKbps = 320;
    options.input.audioRtp.fmtp = "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    return options;
}

void testValidationRejectsMissingInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.url.clear();

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testLegacyArchitectureFilesAreRemoved(TestContext& ctx)
{
    const std::filesystem::path root(MEDIA_TRANSCODE_SOURCE_DIR);
    const std::vector<std::filesystem::path> legacyPaths = {
        "src/local",
        "src/realtime",
        "src/internal/core",
        "src/internal/input",
        "src/internal/output",
        "src/MediaTranscode.cpp",
        "tools/local_transcode_probe",
        "tools/realtime_probe",
        "examples/api",
        "include/media_transcode/LocalVideoTranscode.h",
        "include/media_transcode/MediaTranscode.h",
        "include/media_transcode/MediaTypes.h",
        "tests/unit/test_local_transcode_api.cpp",
        "tests/integration/test_local_transcode_integration.cpp",
        "tests/integration/test_local_transcode_missing_input.cpp",
        "tests/compile/test_public_headers.cpp"
    };

    for (const auto& legacyPath : legacyPaths) {
        const auto fullPath = root / legacyPath;
        EXPECT_FALSE(ctx, std::filesystem::exists(fullPath));
        if (std::filesystem::exists(fullPath)) {
            std::cerr << "legacy architecture path still exists: "
                      << fullPath.string() << '\n';
        }
    }

    const std::filesystem::path internalRoot = root / "src" / "internal";
    for (const auto& entry : std::filesystem::directory_iterator(internalRoot)) {
        EXPECT_EQ(ctx, entry.path().filename().string(), std::string("graph"));
        if (entry.path().filename() != "graph") {
            std::cerr << "non-graph internal source still exists: "
                      << entry.path().string() << '\n';
        }
    }
}

void testVideoToolsAreSplitIntoDedicatedTargets(TestContext& ctx)
{
    const std::string cmake = repositoryFile("CMakeLists.txt");

    expectTextContains(ctx, cmake, "TARGET_LOCAL_VIDEO_CLI");
    expectTextContains(ctx, cmake, "TARGET_REALTIME_VIDEO_CLI");
    expectTextContains(ctx, cmake, "tools/local_video_cli/main.cpp");
    expectTextContains(ctx, cmake, "tools/realtime_video_cli/main.cpp");
    expectTextNotContains(ctx, cmake, "TARGET_GRAPH_TRANSCODE_CLI");
    expectTextNotContains(ctx, cmake, "TARGET_REALTIME_RTP_INPUT_CLI");
    EXPECT_FALSE(ctx, std::filesystem::exists(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                              "tools" / "graph_transcode_cli" / "main.cpp"));
    EXPECT_FALSE(ctx, std::filesystem::exists(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                              "tools" / "realtime_rtp_input_cli" / "main.cpp"));
}

void testVideoToolsRejectLegacyBusinessSwitches(TestContext& ctx)
{
    const std::filesystem::path sourceDir = MEDIA_TRANSCODE_SOURCE_DIR;
    const std::filesystem::path localTool = sourceDir / "tools" / "local_video_cli" / "main.cpp";
    const std::filesystem::path realtimeTool = sourceDir / "tools" / "realtime_video_cli" / "main.cpp";
    EXPECT_TRUE(ctx, std::filesystem::exists(localTool));
    EXPECT_TRUE(ctx, std::filesystem::exists(realtimeTool));

    const std::string localSource = readTextFile(localTool);
    const std::string realtimeSource = readTextFile(realtimeTool);
    const std::string commonSource = repositoryFile("tools/common/VideoCliTranscodeOptions.h");
    const std::string combined = localSource + "\n" + realtimeSource + "\n" + commonSource;

    expectTextNotContains(ctx, combined, "\"--mode\"");
    expectTextNotContains(ctx, combined, "\"--video\"");
    expectTextNotContains(ctx, combined, "\"--audio\"");
    expectTextNotContains(ctx, combined, "\"--enable-hw\"");
    expectTextContains(ctx, combined, "--disable-hw");
    expectTextContains(ctx, combined, "--no-audio");
}

void testGraphRejectsBehaviorDefaultImplementations(TestContext& ctx)
{
    const std::string plannerHeader = repositoryFile("src/internal/graph/planner/MediaPipelinePlanner.h");
    const std::string audioPlannerHeader = repositoryFile("src/internal/graph/planner/MediaAudioPipelinePlanner.h");
    const std::string audioPlanner = repositoryFile("src/internal/graph/planner/MediaAudioPipelinePlanner.cpp");
    const std::string encoderBuilder = repositoryFile("src/internal/graph/builder/codec/CodecResolverEncoderContextBuilder.cpp");
    const std::string audioResolver = repositoryFile("src/internal/graph/nodes/audio/AudioCodecResolverNode.cpp");
    const std::string transcodeParameters = repositoryFile("src/internal/graph/model/MediaTranscodeParameters.h");
    const std::string presetHeader = repositoryFile("src/internal/graph/preset/MediaPipelinePreset.h");
    const std::string realtimePlanner = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp");

    expectTextContains(ctx, plannerHeader, "MediaPipelinePlannerOptions() = delete");
    expectTextNotContains(ctx, plannerHeader, "MediaPipelinePlannerOptions options = {}");
    expectTextNotContains(ctx, plannerHeader, "includeVideo");
    expectTextNotContains(ctx, transcodeParameters, "includeVideo");
    expectTextNotContains(ctx, presetHeader, "includeVideo");
    expectTextNotContains(ctx, realtimePlanner, "requires video branch");
    expectTextContains(ctx, audioPlannerHeader, "MediaAudioPipelinePlannerOptions() = delete");
    expectTextNotContains(ctx, audioPlannerHeader, "bool includeAudio = true");
    expectTextNotContains(ctx, audioPlanner, "plan.reason = \"no_audio\"");
    expectTextNotContains(ctx, audioResolver, "supported_samplerates[0]");
    expectTextContains(ctx, audioResolver, "audio sample rate is not supported by selected encoder");
    expectTextNotContains(ctx, encoderBuilder, "defaultBufferSizeFromRate");
    expectTextNotContains(ctx, encoderBuilder, "default buffer size");
    expectTextNotContains(ctx, encoderBuilder, "rc_min_rate = encoderContext->bit_rate");
    expectTextNotContains(ctx, encoderBuilder, "rc_max_rate = encoderContext->bit_rate");
}

void testCapabilityScanningResponsibilitiesAreSeparated(TestContext& ctx)
{
    const std::string facade = repositoryFile("src/internal/graph/planner/MediaPipelineCapabilityScanner.cpp");
    const std::string inputProbe = repositoryFile("src/internal/graph/planner/capability/MediaInputCapabilityProbe.h");
    const std::string audioProbe = repositoryFile("src/internal/graph/planner/capability/MediaAudioCapabilityProbe.h");
    const std::string videoScanner = repositoryFile("src/internal/graph/planner/capability/MediaVideoCapabilityScanner.h");
    const std::string hardwareProbe = repositoryFile("src/internal/graph/planner/capability/MediaHardwareCapabilityProbe.h");
    const std::string streamProbe = repositoryFile("src/internal/graph/planner/capability/MediaStreamCapabilityProbe.h");

    expectTextContains(ctx, inputProbe, "class MediaInputCapabilityProbe final");
    expectTextContains(ctx, audioProbe, "class MediaAudioCapabilityProbe final");
    expectTextContains(ctx, videoScanner, "class MediaVideoCapabilityScanner final");
    expectTextContains(ctx, hardwareProbe, "class MediaHardwareCapabilityProbe final");
    expectTextContains(ctx, streamProbe, "class MediaStreamCapabilityProbe final");
    expectTextNotContains(ctx, facade, "avformat_open_input");
    expectTextNotContains(ctx, facade, "av_hwdevice_ctx_create");
    expectTextNotContains(ctx, facade, "avformat_find_stream_info");
    expectTextNotContains(ctx, facade, "av_find_best_stream");
}

void testRealtimePlannerOutputPolicyIsSeparated(TestContext& ctx)
{
    const std::string planner = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp");
    const std::string outputPolicy = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeOutputPolicyPlanner.h");
    expectTextContains(ctx, outputPolicy, "class MediaRealtimeOutputPolicyPlanner final");
    expectTextNotContains(ctx, planner, "applyRtpOutputWritePacing");
    expectTextNotContains(ctx, planner, "planMuxedTransportStreamOutput");
}

void testRealtimeOutputPolicyInitializesEveryMuxExpectation(TestContext& ctx)
{
    auto separate = MediaRealtimeRtpTranscodePlanner::plan(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, separate);
    if (separate) {
        EXPECT_TRUE(ctx, separate.value().videoMux.expectVideo);
        EXPECT_FALSE(ctx, separate.value().videoMux.expectAudio);
        EXPECT_FALSE(ctx, separate.value().audioMux.expectVideo);
        EXPECT_TRUE(ctx, separate.value().audioMux.expectAudio);
    }

    auto request = validMpegTsUdpOptions();
    request.parameters.execution.includeAudio = true;
    MediaRealtimeOutputUrls urls;
    urls.muxed = request.output.url;
    urls.muxedFormat = "mpegts";
    MediaRealtimeRtpTranscodePlan muxed;
    const auto status = MediaRealtimeOutputPolicyPlanner::apply(request, urls, muxed);
    EXPECT_TRUE(ctx, status);
    EXPECT_TRUE(ctx, muxed.videoMux.expectVideo);
    EXPECT_TRUE(ctx, muxed.videoMux.expectAudio);
    EXPECT_FALSE(ctx, muxed.audioMux.expectVideo);
    EXPECT_FALSE(ctx, muxed.audioMux.expectAudio);
}

void testRtpMuxStateMachineRejectsIllegalTransitions(TestContext& ctx)
{
    RtpMuxStateMachine state;
    EXPECT_FALSE(ctx, state.markHeaderWritten());
    EXPECT_FALSE(ctx, state.bindExpectations(false, false, true, 0));
    EXPECT_FALSE(ctx, state.bindExpectations(true, true, true, 0));
    EXPECT_TRUE(ctx, state.bindExpectations(true, false, true, 10));
    EXPECT_FALSE(ctx, state.bindExpectations(true, false, true, 10));
    EXPECT_FALSE(ctx, state.markHeaderWritten());
    EXPECT_TRUE(ctx, state.bindOutput());
    EXPECT_TRUE(ctx, state.outputBound());
    EXPECT_FALSE(ctx, state.bindOutput());
    EXPECT_FALSE(ctx, state.markPacingSessionStarted());
    EXPECT_TRUE(ctx, state.markStartupDelayElapsed());
    EXPECT_FALSE(ctx, state.markStartupDelayElapsed());
    EXPECT_TRUE(ctx, state.markPacingSessionStarted());
    EXPECT_TRUE(ctx, state.markHeaderWritten());
    EXPECT_FALSE(ctx, state.markHeaderWritten());
    EXPECT_TRUE(ctx, state.markFormatEmitted());
    EXPECT_FALSE(ctx, state.markFormatEmitted());
    EXPECT_FALSE(ctx, state.markTrailerWritten());
    state.setExpectedInputs({"video"}, {"packet"});
    EXPECT_TRUE(ctx, state.markConfigReady("video"));
    EXPECT_TRUE(ctx, state.markInputEof("packet"));
    EXPECT_TRUE(ctx, state.markTrailerWritten());
    EXPECT_FALSE(ctx, state.markTrailerWritten());
    EXPECT_FALSE(ctx, state.bindOutput());
    state.reset();
    EXPECT_FALSE(ctx, state.expectationsBound());
    EXPECT_FALSE(ctx, state.headerWritten());
    EXPECT_FALSE(ctx, state.trailerWritten());
}

void testAudioDecodeWaitsForCodecMetadataBeforePackets(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(4);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio.codec_source");
    const MediaNodeId packetSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio.packet_source");
    const MediaNodeId decoder = graph.addNode(MediaNodeKind::AudioDecode, "test.audio.decoder");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "test.audio.frame_sink");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(packetSource, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.addInputPort(decoder, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(decoder, "packet", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(decoder, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.addInputPort(sink, "frame", MediaStreamKind::Audio, MediaEdgeKind::RawFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", decoder, "codec", "test.audio.codec", policy);
    graph.connect(packetSource, "packet", decoder, "packet", "test.audio.packet", policy);
    graph.connect(decoder, "frame", sink, "frame", "test.audio.frame", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* packetInput = execution.findInputChannel(decoder, "packet");
    EXPECT_TRUE(ctx, packetInput != nullptr);
    if (!packetInput) return;
    auto packet = makePacketBuffer(true, 0, MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, packet);
    if (!packet) return;
    EXPECT_TRUE(ctx, packetInput->push(packet.value()));
    AudioDecodeNode node(decoder);
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result);
    if (result) EXPECT_EQ(ctx, result.value().state, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, packetInput->size(), static_cast<std::size_t>(1));
}

void testAudioEncodeWaitsForCodecMetadataBeforeFrames(TestContext& ctx)
{
    MediaGraph graph;
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(4);
    const MediaNodeId codecSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio.encoder_codec_source");
    const MediaNodeId frameSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio.encoder_frame_source");
    const MediaNodeId encoder = graph.addNode(MediaNodeKind::AudioEncode, "test.audio.encoder");
    graph.addOutputPort(codecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addOutputPort(frameSource, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.addInputPort(encoder, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(encoder, "frame", MediaStreamKind::Audio, MediaEdgeKind::SoftwareFrame, MediaPayloadKind::Frame);
    graph.connect(codecSource, "codec", encoder, "codec", "test.audio.encoder_codec", policy);
    graph.connect(frameSource, "frame", encoder, "frame", "test.audio.encoder_frame", policy);

    MediaGraphExecutionContext execution;
    EXPECT_TRUE(ctx, execution.compile(graph));
    MediaChannel* frameInput = execution.findInputChannel(encoder, "frame");
    EXPECT_TRUE(ctx, frameInput != nullptr);
    if (!frameInput) return;
    auto frame = ::media::ffmpeg::makeFrame();
    EXPECT_TRUE(ctx, frame != nullptr);
    if (!frame) return;
    auto frameBuffer = FFmpegBufferFactory::wrapFrame(std::move(frame), MediaStreamKind::Audio);
    EXPECT_TRUE(ctx, frameBuffer);
    if (!frameBuffer) return;
    EXPECT_TRUE(ctx, frameInput->push(frameBuffer.value()));
    AudioEncodeNode node(encoder);
    auto result = node.process(execution);
    EXPECT_TRUE(ctx, result);
    if (result) EXPECT_EQ(ctx, result.value().state, MediaNodeProcessState::Waiting);
    EXPECT_EQ(ctx, frameInput->size(), static_cast<std::size_t>(1));
}

void testRealtimeOutputPolicyRejectsMissingAudioPacingBitrate(TestContext& ctx)
{
    auto request = validRawRtpAudioVideoOptions();
    request.parameters.audio.bitrateKbps.reset();
    auto urls = MediaRealtimeOutputPolicyPlanner::planUrls(request);
    EXPECT_TRUE(ctx, urls);
    if (!urls) return;
    MediaRealtimeRtpTranscodePlan plan;
    plan.videoPlan.outputCodecName = "h264";
    plan.videoParameters.bitrateKbps = 8406;
    const auto status = MediaRealtimeOutputPolicyPlanner::apply(request, urls.value(), plan);
    EXPECT_FALSE(ctx, status);
}

void testRealtimePlannerNaturallySelectsAudioVideoTranscode(TestContext& ctx)
{
    auto request = validRawRtpAudioVideoOptions();
    request.parameters.audio.sampleRate = 44100;
    request.parameters.audio.bitrateKbps = 256;
    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(request);
    EXPECT_TRUE(ctx, plan);
    if (!plan) return;
    EXPECT_EQ(ctx, plan.value().videoPlan.branchMode, MediaBranchMode::TranscodeFrame);
    EXPECT_EQ(ctx, plan.value().audioPlan.branchMode, MediaBranchMode::TranscodeFrame);
    EXPECT_FALSE(ctx, plan.value().audioPlan.followsSourceParameters);
}

void testRealtimePlannerValidationAndInputPlanningAreSeparated(TestContext& ctx)
{
    const std::string planner = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.cpp");
    const std::string validator = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeRequestValidator.h");
    const std::string inputPlanner = repositoryFile("src/internal/graph/planner/realtime/MediaRealtimeInputPlanner.h");
    expectTextContains(ctx, validator, "class MediaRealtimeRequestValidator final");
    expectTextContains(ctx, inputPlanner, "class MediaRealtimeInputPlanner final");
    expectTextNotContains(ctx, planner, "validateExplicitStreamClassification");
    expectTextNotContains(ctx, planner, "appendRawRtpSdpMedia");
    expectTextNotContains(ctx, planner, "prepareRealtimeInput(");
}

void testRtpMuxResponsibilitiesAreSeparated(TestContext& ctx)
{
    const std::string node = repositoryFile("src/internal/graph/nodes/mux/RtpMuxNode.cpp");
    const std::string session = repositoryFile("src/internal/graph/nodes/mux/RtpMuxFfmpegSession.h");
    const std::string protocol = repositoryFile("src/internal/graph/nodes/mux/RtpMuxProtocolIo.h");
    const std::string state = repositoryFile("src/internal/graph/nodes/mux/RtpMuxStateMachine.h");
    expectTextContains(ctx, session, "class RtpMuxFfmpegSession final");
    expectTextContains(ctx, protocol, "class RtpMuxProtocolIo final");
    expectTextContains(ctx, state, "class RtpMuxStateMachine final");
    expectTextNotContains(ctx, node, "avformat_write_header");
    expectTextNotContains(ctx, node, "av_interleaved_write_frame");
    expectTextNotContains(ctx, node, "av_write_trailer");
}

void testRuntimeCompilationAndLifecycleAreSeparated(TestContext& ctx)
{
    const std::string runtime = repositoryFile("src/internal/graph/runtime/MediaGraphRuntime.cpp");
    const std::string compiler = repositoryFile("src/internal/graph/runtime/compilation/MediaGraphRuntimeCompiler.h");
    const std::string lifecycle = repositoryFile("src/internal/graph/runtime/lifecycle/MediaGraphRuntimeLifecycleExecutor.h");
    expectTextContains(ctx, compiler, "class MediaGraphRuntimeCompiler final");
    expectTextContains(ctx, lifecycle, "class MediaGraphRuntimeLifecycleExecutor final");
    expectTextNotContains(ctx, runtime, "processSchedulingStep");
    expectTextNotContains(ctx, runtime, "MediaRuntimeNodeFactory::create");
    expectTextNotContains(ctx, runtime, "preparedContext.compile");
}

void testPlannerRejectsUnresolvedBehaviorOptions(TestContext& ctx)
{
    MediaInputVideoStreamInfo input;
    input.streamIndex = 0;
    input.codecName = "h264";
    input.width = 1920;
    input.height = 1080;

    MediaPipelinePlannerOptions missingHardwarePreference(false, false, true, true, true, false);
    const auto missingHardware = MediaPipelinePlanner::planVideoTranscodeKnownInput(
        input,
        "rtp://127.0.0.1:5004",
        missingHardwarePreference);
    EXPECT_FALSE(ctx, missingHardware);
    if (!missingHardware) {
        EXPECT_EQ(ctx, missingHardware.error().code, media::ErrorCode::InvalidArgument);
    }

    MediaPipelinePlannerOptions missingRealtimeOptions(false, false, true, true, true, false);
    missingRealtimeOptions.preferredHardware = "auto";
    const auto missingRealtime = MediaPipelinePlanner::planVideoTranscodeRealtimeUrl(
        "rtsp://127.0.0.1/live",
        missingRealtimeOptions);
    EXPECT_FALSE(ctx, missingRealtime);
    if (!missingRealtime) {
        EXPECT_EQ(ctx, missingRealtime.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testValidationRejectsUnsupportedRealtimeInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.url = "rtp://127.0.0.1:5004";
    options.input.type = RealtimeInputType::Url;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::Unsupported);
    }

    options.input.url = "camera.sdp";
    const auto sdpStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, sdpStatus);
    if (!sdpStatus) {
        EXPECT_EQ(ctx, sdpStatus.error().code, media::ErrorCode::Unsupported);
    }

    options.input.url = "udp://127.0.0.1:5004";
    const auto udpStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, udpStatus);
    if (!udpStatus) {
        EXPECT_EQ(ctx, udpStatus.error().code, media::ErrorCode::Unsupported);
    }

    options.input.url = "sdp://camera";
    const auto sdpSchemeStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, sdpSchemeStatus);
    if (!sdpSchemeStatus) {
        EXPECT_EQ(ctx, sdpSchemeStatus.error().code, media::ErrorCode::Unsupported);
    }
}

void testValidationRequiresExplicitRealtimeStreamClassification(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest missingInputType = validRealtimeOptions();
    missingInputType.input.type.reset();
    expectPlannerInvalidArgument(ctx, missingInputType);

    MediaRealtimeRtpTranscodeRequest missingInputLayout = validRealtimeOptions();
    missingInputLayout.input.streamLayout.reset();
    expectPlannerInvalidArgument(ctx, missingInputLayout);

    MediaRealtimeRtpTranscodeRequest missingOutputLayout = validRealtimeOptions();
    missingOutputLayout.output.streamLayout.reset();
    expectPlannerInvalidArgument(ctx, missingOutputLayout);
}

void testExistingRealtimeModesMapToExplicitLayouts(TestContext& ctx)
{
    const auto urlPlan = preparedPlan(validRealtimeOptions());
    EXPECT_TRUE(ctx, urlPlan);
    if (urlPlan) {
        EXPECT_EQ(ctx, urlPlan.value().inputType, RealtimeInputType::Url);
        EXPECT_EQ(ctx, urlPlan.value().inputLayout, RealtimeInputStreamLayout::SessionDescribed);
        EXPECT_EQ(ctx, urlPlan.value().outputLayout, RealtimeOutputStreamLayout::SeparateStreams);
        EXPECT_FALSE(ctx, urlPlan.value().videoInputStartRequiresKeyFrame);
    }

    const auto rtpPlan = MediaRealtimeRtpTranscodePlanner::plan(validRawRtpOptions());
    EXPECT_TRUE(ctx, rtpPlan);
    if (rtpPlan) {
        EXPECT_EQ(ctx, rtpPlan.value().inputType, RealtimeInputType::RtpPort);
        EXPECT_EQ(ctx, rtpPlan.value().inputLayout, RealtimeInputStreamLayout::SeparateStreams);
        EXPECT_EQ(ctx, rtpPlan.value().outputLayout, RealtimeOutputStreamLayout::SeparateStreams);
        EXPECT_TRUE(ctx, rtpPlan.value().videoInputStartRequiresKeyFrame);
    }
}

void testUnsupportedRealtimeStreamCombinationsFailInPlanner(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest sdp = validRealtimeOptions();
    sdp.input.type = RealtimeInputType::Sdp;
    auto sdpPlan = MediaRealtimeRtpTranscodePlanner::plan(sdp);
    EXPECT_FALSE(ctx, sdpPlan);
    if (!sdpPlan) {
        EXPECT_EQ(ctx, sdpPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest externalTs = validMpegTsUdpOptions();
    externalTs.input.type = RealtimeInputType::ExternalMpegTsPacket;
    auto externalTsPlan = MediaRealtimeRtpTranscodePlanner::plan(externalTs);
    EXPECT_FALSE(ctx, externalTsPlan);
    if (!externalTsPlan) {
        EXPECT_EQ(ctx, externalTsPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest externalRtp = validRawRtpOptions();
    externalRtp.input.type = RealtimeInputType::ExternalRtpPacket;
    auto externalRtpPlan = MediaRealtimeRtpTranscodePlanner::plan(externalRtp);
    EXPECT_FALSE(ctx, externalRtpPlan);
    if (!externalRtpPlan) {
        EXPECT_EQ(ctx, externalRtpPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest bundled = validRawRtpOptions();
    bundled.input.streamLayout = RealtimeInputStreamLayout::BundledStream;
    auto bundledPlan = MediaRealtimeRtpTranscodePlanner::plan(bundled);
    EXPECT_FALSE(ctx, bundledPlan);
    if (!bundledPlan) {
        EXPECT_EQ(ctx, bundledPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest bundledOutput = validRawRtpOptions();
    bundledOutput.output.streamLayout = RealtimeOutputStreamLayout::BundledStream;
    auto bundledOutputPlan = MediaRealtimeRtpTranscodePlanner::plan(bundledOutput);
    EXPECT_FALSE(ctx, bundledOutputPlan);
    if (!bundledOutputPlan) {
        EXPECT_EQ(ctx, bundledOutputPlan.error().code, media::ErrorCode::Unsupported);
    }
}

void testMpegTsUdpRejectsNonUdpInputUrl(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validMpegTsUdpOptions();
    options.input.url = sampleVideoPath();

    const auto status = MediaRealtimeRtpTranscodePlanner::validateRealtimeRequestNoIo(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testSeparateRtpOutputRejectsSingleOutputUrl(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.output.url = "udp://127.0.0.1:6000";
    options.output.host.clear();
    options.output.basePort.reset();

    const auto plan = preparedPlan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRawRtpMissingMetadataFailsInPlanner(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    MediaRealtimeRtpTranscodeRequest missingCodec = options;
    missingCodec.input.videoRtp.codecName.clear();
    expectPlannerInvalidArgument(ctx, missingCodec);

    MediaRealtimeRtpTranscodeRequest missingPayloadType = options;
    missingPayloadType.input.videoRtp.payloadType.reset();
    expectPlannerInvalidArgument(ctx, missingPayloadType);

    MediaRealtimeRtpTranscodeRequest missingClockRate = options;
    missingClockRate.input.videoRtp.clockRate.reset();
    expectPlannerInvalidArgument(ctx, missingClockRate);

    MediaRealtimeRtpTranscodeRequest missingVideoFmtp = options;
    missingVideoFmtp.input.videoRtp.fmtp.clear();
    expectPlannerInvalidArgument(ctx, missingVideoFmtp);

    MediaRealtimeRtpTranscodeRequest missingPacketizationMode = options;
    missingPacketizationMode.input.videoRtp.fmtp =
        "sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;profile-level-id=4D4032";
    expectPlannerInvalidArgument(ctx, missingPacketizationMode);

    MediaRealtimeRtpTranscodeRequest missingProfileLevelId = options;
    missingProfileLevelId.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==";
    expectPlannerInvalidArgument(ctx, missingProfileLevelId);

    MediaRealtimeRtpTranscodeRequest emptySpropParameterSets = options;
    emptySpropParameterSets.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets= ;profile-level-id=4D4032";
    expectPlannerInvalidArgument(ctx, emptySpropParameterSets);
}

void testRawRtpRejectsUnsupportedMetadata(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    MediaRealtimeRtpTranscodeRequest unsupportedVideo = options;
    unsupportedVideo.input.videoRtp.codecName = "vp9";
    expectPlannerInvalidArgument(ctx, unsupportedVideo);

    MediaRealtimeRtpTranscodeRequest staticPayloadType = options;
    staticPayloadType.input.videoRtp.payloadType = 35;
    expectPlannerInvalidArgument(ctx, staticPayloadType);

    MediaRealtimeRtpTranscodeRequest invalidPayloadType = options;
    invalidPayloadType.input.videoRtp.payloadType = 128;
    expectPlannerInvalidArgument(ctx, invalidPayloadType);

    MediaRealtimeRtpTranscodeRequest invalidClockRate = options;
    invalidClockRate.input.videoRtp.clockRate = 48000;
    expectPlannerInvalidArgument(ctx, invalidClockRate);

    MediaRealtimeRtpTranscodeRequest emptyHevcVps = options;
    emptyHevcVps.input.videoRtp.codecName = "hevc";
    emptyHevcVps.input.videoRtp.fmtp =
        "sprop-vps= ;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA==";
    expectPlannerInvalidArgument(ctx, emptyHevcVps);

    MediaRealtimeRtpTranscodeRequest missingPort = options;
    missingPort.input.videoRtp.url = "rtp://127.0.0.1";
    expectPlannerInvalidArgument(ctx, missingPort);

    MediaRealtimeRtpTranscodeRequest pathUrl = options;
    pathUrl.input.videoRtp.url = "rtp://127.0.0.1:5004/video";
    expectPlannerInvalidArgument(ctx, pathUrl);

    MediaRealtimeRtpTranscodeRequest queryUrl = options;
    queryUrl.input.videoRtp.url = "udp://127.0.0.1:5004?pkt_size=1200";
    expectPlannerInvalidArgument(ctx, queryUrl);

    MediaRealtimeRtpTranscodeRequest fragmentUrl = options;
    fragmentUrl.input.videoRtp.url = "rtp://127.0.0.1:5004#stream";
    expectPlannerInvalidArgument(ctx, fragmentUrl);

    MediaRealtimeRtpTranscodeRequest userInfoUrl = options;
    userInfoUrl.input.videoRtp.url = "rtp://user@127.0.0.1:5004";
    expectPlannerInvalidArgument(ctx, userInfoUrl);
}

void testRawRtpPlansH264AndHevcInput(TestContext& ctx)
{
    auto h264Options = validRawRtpOptions();
    const auto h264Plan = MediaRealtimeRtpTranscodePlanner::plan(h264Options);
    EXPECT_TRUE(ctx, h264Plan);
    if (h264Plan) {
        EXPECT_EQ(ctx, h264Plan.value().inputType, RealtimeInputType::RtpPort);
        EXPECT_EQ(ctx, h264Plan.value().videoPlan.inputCodecName, std::string("h264"));
        EXPECT_EQ(ctx, h264Plan.value().videoPlan.sourceStreamIndex, 0);
    }

    auto hevcOptions = validRawRtpOptions();
    hevcOptions.input.videoRtp.codecName = "hevc";
    hevcOptions.input.videoRtp.fmtp = "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA==";
    const auto hevcPlan = MediaRealtimeRtpTranscodePlanner::plan(hevcOptions);
    EXPECT_TRUE(ctx, hevcPlan);
    if (!hevcPlan) {
        std::cerr << hevcPlan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, hevcPlan.value().inputType, RealtimeInputType::RtpPort);
    EXPECT_EQ(ctx, hevcPlan.value().videoPlan.inputCodecName, std::string("hevc"));
    EXPECT_TRUE(ctx, hevcPlan.value().input.sdpText.find("H265/90000") != std::string::npos);
}

void testRawRtpAudioEndpointRequiredWhenAudioEnabled(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest missingAudio = validRawRtpOptions();
    missingAudio.parameters.execution.includeAudio = true;
    missingAudio.parameters.audio.codecName = "aac";
    expectPlannerInvalidArgument(ctx, missingAudio);

    MediaRealtimeRtpTranscodeRequest missingFmtp = validRawRtpAudioVideoOptions();
    missingFmtp.input.audioRtp.fmtp.clear();
    expectPlannerInvalidArgument(ctx, missingFmtp);
}

void testRawRtpPlansAudioVideoInput(TestContext& ctx)
{
    auto options = validRawRtpAudioVideoOptions();
    options.input.audioRtp.url = "rtp://192.0.2.10:5006";

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, plan.value().audioPlan.enabled);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceStreamIndex, 0);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("aac"));
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=video 5004 RTP/AVP 96") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=audio 5006 RTP/AVP 97") == std::string::npos);
    EXPECT_TRUE(ctx, plan.value().audioInput.sdpText.find("m=audio 5006 RTP/AVP 97") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=video 5004 RTP/AVP 96\r\nc=IN IP4 127.0.0.1\r\n") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().audioInput.sdpText.find("m=audio 5006 RTP/AVP 97\r\nc=IN IP4 192.0.2.10\r\n") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().audioInput.sdpText.find("a=fmtp:97 ") != std::string::npos);
    EXPECT_EQ(ctx,
              plan.value().videoOutput.url,
              std::string("rtp://127.0.0.1:5004?localrtpport=0&localrtcpport=0"));
    EXPECT_EQ(ctx,
              plan.value().audioOutput.url,
              std::string("rtp://127.0.0.1:5006?localrtpport=0&localrtcpport=0"));
}

void testRawRtpAudioVideoGraphUsesIsolatedInputs(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RawRtpInput), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::Demux), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, findNodeByName(graph, "realtime.video.input") != nullptr);
    EXPECT_TRUE(ctx, findNodeByName(graph, "realtime.audio.input") != nullptr);
    EXPECT_TRUE(ctx, findNodeByName(graph, "realtime.video.input.demux") != nullptr);
    EXPECT_TRUE(ctx, findNodeByName(graph, "realtime.audio.input.demux") != nullptr);

    const MediaEdge* videoFormatToDemux = findEdgeByNames(
        graph,
        "realtime.video.input",
        "format",
        "realtime.video.input.demux",
        "format");
    const MediaEdge* audioFormatToDemux = findEdgeByNames(
        graph,
        "realtime.audio.input",
        "format",
        "realtime.audio.input.demux",
        "format");
    EXPECT_TRUE(ctx, videoFormatToDemux != nullptr);
    EXPECT_TRUE(ctx, audioFormatToDemux != nullptr);
}

void testSeparateRtpH264OutputRequestsGlobalHeader(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaNode* codecResolver = findNodeByKind(graphResult.value(), MediaNodeKind::CodecResolver);
    EXPECT_TRUE(ctx, codecResolver != nullptr);
    if (codecResolver) {
        EXPECT_EQ(ctx, codecResolver->options.value(MediaTranscodeOptionKey::VideoGlobalHeader), std::string("1"));
    }
}

void testSeparateRtpInheritedH264OutputRequestsGlobalHeader(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.video.codecName.clear();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }
    EXPECT_EQ(ctx, plan.value().videoPlan.outputCodecName, std::string("h264"));

    const auto graphResult = preparedGraph(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaNode* codecResolver = findNodeByKind(graphResult.value(), MediaNodeKind::CodecResolver);
    EXPECT_TRUE(ctx, codecResolver != nullptr);
    if (codecResolver) {
        EXPECT_EQ(ctx, codecResolver->options.value(MediaTranscodeOptionKey::VideoGlobalHeader), std::string("1"));
    }
}

void testEncoderContextBuilderAppliesGlobalHeaderOption(TestContext& ctx)
{
    const std::string encoderBuilder =
        repositoryFile("src/internal/graph/builder/codec/CodecResolverEncoderContextBuilder.cpp");
    expectTextContains(ctx, encoderBuilder, "MediaTranscodeOptionKey::VideoGlobalHeader");
    expectTextContains(ctx, encoderBuilder, "AV_CODEC_FLAG_GLOBAL_HEADER");
}

void testRawRtpInheritsSourceCodecsWhenTranscodeCodecsAreOmitted(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.video.codecName.clear();
    options.parameters.audio.codecName.clear();

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.inputCodecName, std::string("h264"));
    EXPECT_EQ(ctx, plan.value().videoPlan.outputCodecName, std::string("h264"));
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("aac"));
    EXPECT_EQ(ctx, plan.value().audioPlan.targetCodecName, std::string("aac"));
}

void testRawRtpMatchingAudioPlansPacketCopy(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, plan.value().audioPlan.enabled);
    EXPECT_EQ(ctx, plan.value().audioPlan.branchMode, MediaBranchMode::CopyPacket);
    EXPECT_TRUE(ctx, plan.value().audioPlan.followsSourceParameters);
    EXPECT_TRUE(ctx, plan.value().audioPlan.monotonicPacketTimestamps);

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::AudioDecode), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::AudioResample), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::AudioEncode), static_cast<std::size_t>(0));
    const MediaNode* audioNormalize = findNodeByName(graph, "realtime.audio.copy.normalize");
    EXPECT_TRUE(ctx, audioNormalize != nullptr);
    if (audioNormalize) {
        EXPECT_EQ(ctx,
                  audioNormalize->options.value(MediaTranscodeOptionKey::PacketMonotonicTimestamps),
                  std::string("1"));
    }
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph,
                                          MediaNodeKind::PacketNormalize,
                                          MediaNodeKind::AvPacketStartBarrier,
                                          MediaEdgeKind::EncodedPacket) != nullptr);
}

void testRawRtpAudioTranscodesWhenTargetDiffers(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.audio.codecName = "opus";
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().audioPlan.branchMode, MediaBranchMode::TranscodeFrame);
    EXPECT_FALSE(ctx, plan.value().audioPlan.followsSourceParameters);
    EXPECT_FALSE(ctx, plan.value().audioPlan.monotonicPacketTimestamps);
}

void testRealtimePlanOwnsThreadingPolicy(TestContext& ctx)
{
    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().threadingPolicy.mode, MediaThreadingMode::PerNodeWorker);
}

void testRawRtpRejectsMissingVideoBitrate(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.bitrateKbps.reset();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRawRtpRejectsZeroVideoBitrate(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.bitrateKbps = 0;

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testLocalTranscodeRejectsZeroVideoBitrate(TestContext& ctx)
{
    LocalFileTranscodeOptions options;
    options.inputUrl = sampleVideoPath();
    options.outputUrl = "local-zero-bitrate-test.mp4";
    options.parameters.execution.includeAudio = false;
    options.parameters.execution.disableHardware = true;
    options.parameters.queues.metadata = 1;
    options.parameters.queues.packet = 256;
    options.parameters.queues.frame = 128;
    options.parameters.queues.mux = 256;
    options.parameters.video.codecName = "h264";
    options.parameters.video.bitrateKbps = 0;

    const auto graph = LocalFileTranscodeGraphBuilder::build(options);
    EXPECT_FALSE(ctx, graph);
    if (!graph) {
        EXPECT_EQ(ctx, graph.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRawRtpRejectsUnknownSourceCodecWhenCodecIsNotExplicit(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.codecName.clear();
    options.input.videoRtp.codecName = "unknown-source-codec";

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
}

void testRealtimeNoResizeDoesNotScoreFilterStage(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.selected.score, 600);
}

void testRealtimeResizeScoresFilterStage(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.width = 1280;
    options.parameters.video.height = 720;

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.selected.score, 900);
}

void testRawRtpPlansOpusAudioInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.audio.codecName = "opus";
    options.input.audioRtp.codecName = "opus";
    options.input.audioRtp.payloadType = 98;
    options.input.audioRtp.clockRate = 48000;
    options.input.audioRtp.channels = 2;
    options.input.audioRtp.fmtp.clear();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, plan.value().audioPlan.enabled);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("opus"));
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=audio 5006 RTP/AVP 98") == std::string::npos);
    EXPECT_TRUE(ctx, plan.value().audioInput.sdpText.find("m=audio 5006 RTP/AVP 98") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().audioInput.sdpText.find("a=rtpmap:98 opus/48000/2") != std::string::npos);
    EXPECT_FALSE(ctx, plan.value().audioInput.sdpText.find("a=fmtp:98 ") != std::string::npos);
}

void testValidationRejectsOddRtpPort(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.output.basePort = 5005;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testValidationRejectsAudioRtpPortOverflow(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.bitrateKbps = 320;
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;
    options.output.basePort = 65534;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRealtimeNoAudioProbeDoesNotRequestAudio(TestContext& ctx)
{
    const auto header = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "planner" /
                                     "MediaPipelineCapabilityScanner.h");
    EXPECT_TRUE(ctx, header.find("detectRealtimeInputStreamInfo(") != std::string::npos);
    EXPECT_TRUE(ctx, header.find("bool includeAudio") != std::string::npos);

    const auto inputPlanner = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                           "src" /
                                           "internal" /
                                           "graph" /
                                           "planner" /
                                           "realtime" /
                                           "MediaRealtimeInputPlanner.cpp");
    EXPECT_TRUE(ctx, inputPlanner.find("prepareRealtimeInput(") != std::string::npos);
    EXPECT_TRUE(ctx, inputPlanner.find("audioRequested(request)") != std::string::npos);
}

void testRealtimeUrlInheritsObservableVideoBitrate(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.video.bitrateKbps.reset();

    const auto plan = preparedPlan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }
    EXPECT_TRUE(ctx, plan.value().videoParameters.bitrateKbps.has_value());
    if (!plan.value().videoParameters.bitrateKbps) {
        return;
    }
    EXPECT_TRUE(ctx, *plan.value().videoParameters.bitrateKbps > 0);

    const auto graphResult = preparedGraph(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaNode* codecResolver = findNodeByKind(graphResult.value(), MediaNodeKind::CodecResolver);
    EXPECT_TRUE(ctx, codecResolver != nullptr);
    if (codecResolver) {
        EXPECT_EQ(ctx,
                  codecResolver->options.value(MediaTranscodeOptionKey::VideoBitrateKbps),
                  std::to_string(*plan.value().videoParameters.bitrateKbps));
    }
}

void testBuildPlansVideoStreamAndSoftwareExecution(TestContext& ctx)
{
    const auto graphResult = preparedGraph(validRealtimeOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RealtimeInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoEncode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpMux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::SdpWriter) != nullptr);

    const MediaNode* encode = findNodeByKind(graph, MediaNodeKind::VideoEncode);
    EXPECT_TRUE(ctx, encode != nullptr);
    if (encode) {
        EXPECT_EQ(ctx, encode->options.value("pipeline.chain"), std::string("software"));
        EXPECT_EQ(ctx, encode->options.value("encoder"), std::string("libx264"));
    }
}

void testBuildPlansRealtimeUrlAudioBranch(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.bitrateKbps = 320;
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;

    const auto graphResult = preparedGraph(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioResample) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioEncode) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(2));

    const MediaNode* audioOutput = findNodeByName(graph, "realtime.audio.rtp.output");
    EXPECT_TRUE(ctx, audioOutput != nullptr);
    if (audioOutput) {
        EXPECT_EQ(ctx,
                  audioOutput->options.value("url"),
                  std::string("rtp://127.0.0.1:5006?localrtpport=0&localrtcpport=0"));
    }

    const MediaNode* sdpWriter = findNodeByKind(graph, MediaNodeKind::SdpWriter);
    EXPECT_TRUE(ctx, sdpWriter != nullptr);
    if (sdpWriter) {
        EXPECT_EQ(ctx, sdpWriter->options.value("sdp.expected_contexts"), std::string("2"));
    }
    const auto source = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "nodes" /
                                     "output" /
                                     "SdpWriterNode.cpp");
    EXPECT_EQ(ctx, countOccurrences(source, "av_sdp_create("), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, source.find("contexts.data(), static_cast<int>(contexts.size())") != std::string::npos);
}

void testBuildPlansRawRtpH264Graph(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RawRtpInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::PacketNormalize) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::PacketStartGate) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoEncode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpMux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::SdpWriter) != nullptr);
    const MediaNode* timestampNode = findNodeByKind(graph, MediaNodeKind::VideoTimestamp);
    EXPECT_TRUE(ctx, timestampNode != nullptr);
    if (timestampNode) {
        EXPECT_EQ(ctx, timestampNode->options.value(MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps), std::string("1"));
    }
    const MediaNode* codecResolver = findNodeByKind(graph, MediaNodeKind::CodecResolver);
    EXPECT_TRUE(ctx, codecResolver != nullptr);
    if (codecResolver) {
        EXPECT_EQ(ctx, codecResolver->options.value(MediaTranscodeOptionKey::VideoBitrateKbps), std::string("8406"));
    }
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::RawRtpInput, MediaNodeKind::Demux, MediaEdgeKind::Metadata) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::Demux, MediaNodeKind::StreamSplit, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::StreamSplit, MediaNodeKind::PacketNormalize, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::PacketNormalize, MediaNodeKind::PacketStartGate, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::PacketStartGate, MediaNodeKind::VideoDecode, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::VideoEncode, MediaNodeKind::RtpMux, MediaEdgeKind::EncodedPacket) != nullptr);
}

void testBuildPlansRawRtpAudioVideoGraph(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RawRtpInput) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::AudioDecode), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::AudioEncode), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph,
                                          MediaNodeKind::PacketNormalize,
                                          MediaNodeKind::AvPacketStartBarrier,
                                          MediaEdgeKind::EncodedPacket) != nullptr);
}

void testRealtimeRtpDataPathUsesPlannedNonBlockingQueues(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_EQ(ctx,
              countRealtimeDataEdgesWithQueueMode(graph, MediaQueueMode::Blocking),
              static_cast<std::size_t>(0));
    EXPECT_EQ(ctx,
              countAudioDataEdgesWithOverflowPolicy(graph, MediaQueueOverflowPolicy::DropNonKeyFrame),
              static_cast<std::size_t>(0));
}

void testRealtimeCliAppliesPlannerThreadingPolicy(TestContext& ctx)
{
    const std::string cliSource = repositoryFile("tools/realtime_video_cli/main.cpp");
    expectTextContains(ctx, cliSource, "MediaRealtimeRtpTranscodePlanner::preflight(options)");
    expectTextContains(ctx, cliSource, "MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(std::move(preflight))");
    expectTextContains(ctx, cliSource, "runtime.setThreadingPolicy(threadingPolicy)");
}

void testRtpMuxEmitsSdpSnapshotInsteadOfBorrowedLiveContext(TestContext& ctx)
{
    const std::string sessionSource = repositoryFile("src/internal/graph/nodes/mux/RtpMuxFfmpegSession.cpp");
    expectTextContains(ctx, sessionSource, "makeSdpFormatSnapshot()");
    expectTextContains(ctx, sessionSource, "avformat_alloc_output_context2(&raw, nullptr, \"rtp\", url)");
    expectTextNotContains(ctx, sessionSource, "FFmpegBufferFactory::borrowFormatContext");
}

void testSdpWriterOrdersSeparateRtpContextsByMediaType(TestContext& ctx)
{
    const std::string sdpWriterSource = repositoryFile("src/internal/graph/nodes/output/SdpWriterNode.cpp");
    expectTextContains(ctx, sdpWriterSource, "std::stable_sort(contexts.begin(), contexts.end()");
    expectTextContains(ctx, sdpWriterSource, "AVMEDIA_TYPE_VIDEO");
    expectTextContains(ctx, sdpWriterSource, "AVMEDIA_TYPE_AUDIO");
}

void testGraphFixedSleepsAreClassified(TestContext& ctx)
{
    const std::filesystem::path graphRoot = std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                           "src" /
                                           "internal" /
                                           "graph";
    const std::vector<std::pair<std::string, std::string>> allowedSleepFiles = {
        {"runtime/threading/MediaGraphWorker.cpp", "non-realtime idle backoff"},
        {"nodes/mux/RtpMuxNode.cpp", "rtp_mux.startup_delay_elapsed"},
        {"runtime/MediaGraphPacingClock.cpp", "m_policy.enablePacing"},
        {"runtime/ffmpeg/FFmpegPacedAvio.cpp", "pacedWritePacket"},
    };

    for (const auto& entry : std::filesystem::recursive_directory_iterator(graphRoot)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }

        const std::string relative = entry.path().lexically_relative(graphRoot).generic_string();
        const std::string contents = readTextFile(entry.path());
        const bool hasSleepFor = contents.find("sleep_for(") != std::string::npos;
        const bool hasSleepUntil = contents.find("sleep_until(") != std::string::npos;
        if (!hasSleepFor && !hasSleepUntil) {
            continue;
        }

        bool allowed = false;
        for (const auto& allowedFile : allowedSleepFiles) {
            if (relative == allowedFile.first &&
                contents.find(allowedFile.second) != std::string::npos) {
                allowed = true;
                break;
            }
        }
        EXPECT_TRUE(ctx, allowed);
        if (!allowed) {
            std::cerr << "unclassified graph fixed sleep in " << relative << '\n';
        }
    }
}

void testRealtimeWorkerUsesEventDrivenWait(TestContext& ctx)
{
    const std::string executorSource = repositoryFile("src/internal/graph/runtime/threading/MediaGraphThreadedExecutor.cpp");
    EXPECT_TRUE(ctx, executorSource.find("idleSleepMs") == std::string::npos);
    EXPECT_TRUE(ctx, executorSource.find("maxIdleSpins") == std::string::npos);

    const std::string workerSource = repositoryFile("src/internal/graph/runtime/threading/MediaGraphWorker.cpp");
    expectTextContains(ctx, workerSource, "MediaNodeProcessState::Waiting");
    expectTextContains(ctx, workerSource, "waitForChange");
    EXPECT_TRUE(ctx, workerSource.find("std::this_thread::yield") == std::string::npos);
}

void testRealtimeRtpMuxUsesPlannerPacingPolicy(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        return;
    }
    EXPECT_TRUE(ctx, plan.value().videoMux.pacingPolicy.enablePacing);
    EXPECT_TRUE(ctx, plan.value().audioMux.pacingPolicy.enablePacing);
    EXPECT_TRUE(ctx, plan.value().videoMux.monotonicPacketTimestamps);
    EXPECT_TRUE(ctx, plan.value().audioMux.monotonicPacketTimestamps);
    EXPECT_TRUE(ctx, plan.value().videoMux.startupDelayMs > 0);
    EXPECT_TRUE(ctx, plan.value().audioMux.startupDelayMs > 0);

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(std::move(plan).value());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        return;
    }

    const MediaGraph& graph = graphResult.value();
    const MediaNode* videoMux = findNodeByName(graph, "realtime.video.rtp.mux");
    const MediaNode* audioMux = findNodeByName(graph, "realtime.audio.rtp.mux");
    EXPECT_TRUE(ctx, videoMux != nullptr);
    EXPECT_TRUE(ctx, audioMux != nullptr);
    if (videoMux && audioMux) {
        EXPECT_EQ(ctx, videoMux->options.value("rtp.pacing.enabled"), std::string("1"));
        EXPECT_EQ(ctx, audioMux->options.value("rtp.pacing.enabled"), std::string("1"));
        EXPECT_EQ(ctx, videoMux->options.value("rtp.packet_timestamps.monotonic"), std::string("1"));
        EXPECT_EQ(ctx, audioMux->options.value("rtp.packet_timestamps.monotonic"), std::string("1"));
        EXPECT_TRUE(ctx, !videoMux->options.value("rtp.startup_delay_ms").empty());
        EXPECT_TRUE(ctx, !audioMux->options.value("rtp.startup_delay_ms").empty());
    }

    const std::string muxSource = repositoryFile("src/internal/graph/nodes/mux/RtpMuxNode.cpp");
    const std::string protocolSource = repositoryFile("src/internal/graph/nodes/mux/RtpMuxProtocolIo.cpp");
    expectTextContains(ctx, muxSource, "m_session.pacingClock().waitUntil");
    expectTextContains(ctx, muxSource, "normalizePacketTimestamps");
    expectTextContains(ctx, muxSource, "startPacingSessionIfNeeded");
    expectTextContains(ctx, muxSource, "rtp_mux.pacing_session_started");
    expectTextContains(ctx, protocolSource, "av_interleaved_write_frame");
}

void testRealtimeRtpOutputUsesPlannerWritePacingPolicy(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        return;
    }

    EXPECT_TRUE(ctx, plan.value().videoOutput.writePacingEnabled);
    EXPECT_TRUE(ctx, plan.value().audioOutput.writePacingEnabled);
    EXPECT_TRUE(ctx, plan.value().videoOutput.writePacingBytesPerSecond > 0);
    EXPECT_TRUE(ctx, plan.value().audioOutput.writePacingBytesPerSecond > 0);
    EXPECT_TRUE(ctx, plan.value().videoOutput.writePacingBurstBytes > 0);
    EXPECT_TRUE(ctx, plan.value().audioOutput.writePacingBurstBytes > 0);

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(std::move(plan).value());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        return;
    }

    const MediaGraph& graph = graphResult.value();
    const MediaNode* videoOutput = findNodeByName(graph, "realtime.video.rtp.output");
    const MediaNode* audioOutput = findNodeByName(graph, "realtime.audio.rtp.output");
    EXPECT_TRUE(ctx, videoOutput != nullptr);
    EXPECT_TRUE(ctx, audioOutput != nullptr);
    if (videoOutput && audioOutput) {
        EXPECT_EQ(ctx, videoOutput->options.value("rtp.write_pacing.enabled"), std::string("1"));
        EXPECT_EQ(ctx, audioOutput->options.value("rtp.write_pacing.enabled"), std::string("1"));
        EXPECT_TRUE(ctx, !videoOutput->options.value("rtp.write_pacing.bytes_per_second").empty());
        EXPECT_TRUE(ctx, !audioOutput->options.value("rtp.write_pacing.bytes_per_second").empty());
        EXPECT_TRUE(ctx, !videoOutput->options.value("rtp.write_pacing.burst_bytes").empty());
        EXPECT_TRUE(ctx, !audioOutput->options.value("rtp.write_pacing.burst_bytes").empty());
    }

    const std::string outputSource = repositoryFile("src/internal/graph/nodes/output/RtpOutputNode.cpp");
    expectTextContains(ctx, outputSource, "openPacedWriteAvio");
    expectTextContains(ctx, outputSource, "rtp.write_pacing.bytes_per_second");

    const std::string pacedAvioSource = repositoryFile("src/internal/graph/runtime/ffmpeg/FFmpegPacedAvio.cpp");
    expectTextContains(ctx, pacedAvioSource, "pacedWritePacket");
    expectTextContains(ctx, pacedAvioSource, "resetPacedWriteAvio");
    expectTextContains(ctx, pacedAvioSource, "std::this_thread::sleep_until");
}

void testRealtimePlannerOwnsShortGopForRtpKeyFrameRecovery(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.video.gop.reset();

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        return;
    }
    EXPECT_TRUE(ctx, plan.value().videoParameters.gop.has_value());
    if (plan.value().videoParameters.gop) {
        EXPECT_TRUE(ctx, *plan.value().videoParameters.gop > 0);
        EXPECT_TRUE(ctx, *plan.value().videoParameters.gop <= 30);
    }

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(std::move(plan).value());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        return;
    }

    const MediaNode* codecResolver = findNodeByKind(graphResult.value(), MediaNodeKind::CodecResolver);
    EXPECT_TRUE(ctx, codecResolver != nullptr);
    if (codecResolver) {
        EXPECT_EQ(ctx, codecResolver->options.value(MediaTranscodeOptionKey::VideoGop), std::string("30"));
        EXPECT_EQ(ctx, codecResolver->options.value(MediaTranscodeOptionKey::VideoBFrames), std::string("0"));
    }
}

void testPacketKeyFrameFlagMapsToMediaBuffer(TestContext& ctx)
{
    auto key = makePacketBuffer(true);
    auto delta = makePacketBuffer(false);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, delta);
    if (key && delta) {
        EXPECT_TRUE(ctx, key.value()->isKeyFrame());
        EXPECT_FALSE(ctx, delta.value()->isKeyFrame());
    }
}

void testRealtimeQueueDropPoliciesAreNormalBackpressure(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.capacity = 1;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;

    MediaSpscRingQueue spsc(policy);
    auto key = makePacketBuffer(true);
    auto delta = makePacketBuffer(false);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, delta);
    if (key && delta) {
        EXPECT_TRUE(ctx, spsc.push(key.value()));
        EXPECT_TRUE(ctx, spsc.push(delta.value()));
        EXPECT_EQ(ctx, spsc.size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, spsc.metrics().dropped, static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, spsc.metrics().failedPushes, static_cast<std::uint64_t>(0));
    }

    policy.mode = MediaQueueMode::Blocking;
    MediaBlockingQueue blocking(policy);
    key = makePacketBuffer(true);
    delta = makePacketBuffer(false);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, delta);
    if (key && delta) {
        EXPECT_TRUE(ctx, blocking.push(key.value()));
        EXPECT_TRUE(ctx, blocking.push(delta.value()));
        EXPECT_EQ(ctx, blocking.size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, blocking.metrics().dropped, static_cast<std::uint64_t>(1));
        EXPECT_EQ(ctx, blocking.metrics().failedPushes, static_cast<std::uint64_t>(0));
    }
}

void testDropNonKeyFramePreservesQueuedKeyFrames(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.mode = MediaQueueMode::SpscRing;
    policy.capacity = 2;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;

    MediaSpscRingQueue queue(policy);
    auto key = makePacketBuffer(true, 1);
    auto delta = makePacketBuffer(false, 2);
    auto nextKey = makePacketBuffer(true, 3);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, delta);
    EXPECT_TRUE(ctx, nextKey);
    if (!key || !delta || !nextKey) {
        return;
    }

    EXPECT_TRUE(ctx, queue.push(key.value()));
    EXPECT_TRUE(ctx, queue.push(delta.value()));
    EXPECT_TRUE(ctx, queue.push(nextKey.value()));
    EXPECT_EQ(ctx, queue.metrics().dropped, static_cast<std::uint64_t>(1));
    EXPECT_EQ(ctx, queue.metrics().failedPushes, static_cast<std::uint64_t>(0));

    MediaBufferRef first;
    MediaBufferRef second;
    EXPECT_TRUE(ctx, queue.pop(first));
    EXPECT_TRUE(ctx, queue.pop(second));
    EXPECT_TRUE(ctx, first && first->isKeyFrame());
    EXPECT_TRUE(ctx, second && second->isKeyFrame());
    if (first && second) {
        EXPECT_EQ(ctx, first->pts(), static_cast<MediaTimeValue>(1));
        EXPECT_EQ(ctx, second->pts(), static_cast<MediaTimeValue>(3));
    }
}

void testDropNonKeyFrameRejectsKeyFrameWhenNoNonKeyCanBeDropped(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.capacity = 1;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;

    policy.mode = MediaQueueMode::SpscRing;
    MediaSpscRingQueue spsc(policy);
    auto key = makePacketBuffer(true, 1);
    auto nextKey = makePacketBuffer(true, 2);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, nextKey);
    if (key && nextKey) {
        EXPECT_TRUE(ctx, spsc.push(key.value()));
        EXPECT_EQ(ctx, spsc.pushOutcome(nextKey.value()), MediaQueuePushOutcome::WouldBlock);
        EXPECT_EQ(ctx, spsc.metrics().dropped, static_cast<std::uint64_t>(0));
        EXPECT_EQ(ctx, spsc.size(), static_cast<std::size_t>(1));
    }

    policy.mode = MediaQueueMode::Blocking;
    MediaBlockingQueue blocking(policy);
    key = makePacketBuffer(true, 1);
    nextKey = makePacketBuffer(true, 2);
    EXPECT_TRUE(ctx, key);
    EXPECT_TRUE(ctx, nextKey);
    if (key && nextKey) {
        EXPECT_TRUE(ctx, blocking.push(key.value()));
        EXPECT_EQ(ctx, blocking.pushOutcome(nextKey.value()), MediaQueuePushOutcome::WouldBlock);
        EXPECT_EQ(ctx, blocking.metrics().dropped, static_cast<std::uint64_t>(0));
        EXPECT_EQ(ctx, blocking.size(), static_cast<std::size_t>(1));
    }
}

void testDropNonKeyFramePushWaitsToPreserveKeyFrames(TestContext& ctx)
{
    MediaQueuePolicy policy;
    policy.capacity = 1;
    policy.bounded = true;
    policy.overflowPolicy = MediaQueueOverflowPolicy::DropNonKeyFrame;

    auto exercise = [&](MediaQueue& queue) {
        auto key = makePacketBuffer(true, 1);
        auto nextKey = makePacketBuffer(true, 2);
        EXPECT_TRUE(ctx, key);
        EXPECT_TRUE(ctx, nextKey);
        if (!key || !nextKey) {
            return;
        }

        EXPECT_TRUE(ctx, queue.push(key.value()));
        bool pushOk = false;
        std::thread producer([&] {
            pushOk = static_cast<bool>(queue.push(nextKey.value()));
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(ctx, queue.size(), static_cast<std::size_t>(1));
        EXPECT_EQ(ctx, queue.metrics().dropped, static_cast<std::uint64_t>(0));

        MediaBufferRef first;
        EXPECT_TRUE(ctx, queue.pop(first));
        producer.join();

        MediaBufferRef second;
        EXPECT_TRUE(ctx, pushOk);
        EXPECT_TRUE(ctx, queue.tryPop(second));
        EXPECT_TRUE(ctx, first && first->isKeyFrame());
        EXPECT_TRUE(ctx, second && second->isKeyFrame());
        if (first && second) {
            EXPECT_EQ(ctx, first->pts(), static_cast<MediaTimeValue>(1));
            EXPECT_EQ(ctx, second->pts(), static_cast<MediaTimeValue>(2));
        }
    };

    policy.mode = MediaQueueMode::SpscRing;
    MediaSpscRingQueue spsc(policy);
    exercise(spsc);

    policy.mode = MediaQueueMode::Blocking;
    MediaBlockingQueue blocking(policy);
    exercise(blocking);
}

void testRealtimeMuxEdgesUseStreamSpecificPolicies(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        return;
    }

    const MediaGraph& graph = graphResult.value();
    const MediaEdge* videoMuxEdge = findInputEdgeToNode(
        graph,
        "realtime.video.rtp.mux",
        "packet",
        MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket);
    const MediaEdge* audioMuxEdge = findInputEdgeToNodeWithPayload(
        graph,
        "realtime.audio.rtp.mux",
        "packet",
        MediaStreamKind::Audio,
        MediaPayloadKind::Packet);
    EXPECT_TRUE(ctx, videoMuxEdge != nullptr);
    EXPECT_TRUE(ctx, audioMuxEdge != nullptr);
    if (videoMuxEdge && audioMuxEdge) {
        EXPECT_EQ(ctx, videoMuxEdge->policy.queuePolicy.overflowPolicy, MediaQueueOverflowPolicy::DropNonKeyFrame);
        EXPECT_EQ(ctx, audioMuxEdge->policy.queuePolicy.overflowPolicy, MediaQueueOverflowPolicy::DropOldest);
    }
}

void testSeparateRtpAudioVideoUsesStartBarrier(TestContext& ctx)
{
    auto plan = MediaRealtimeRtpTranscodePlanner::plan(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        return;
    }
    EXPECT_TRUE(ctx, plan.value().videoInputStartRequiresKeyFrame);

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(std::move(plan).value());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    const MediaNode* barrier = findNodeByName(graph, "realtime.av.start_barrier");
    EXPECT_TRUE(ctx, barrier != nullptr);
    if (!barrier) {
        return;
    }
    EXPECT_EQ(ctx, barrier->kind, MediaNodeKind::AvPacketStartBarrier);
    EXPECT_EQ(ctx, barrier->options.value("av_start_barrier.expect_video"), std::string("1"));
    EXPECT_EQ(ctx, barrier->options.value("av_start_barrier.expect_audio"), std::string("1"));
    EXPECT_EQ(ctx, barrier->options.value("av_start_barrier.require_video_key_frame"), std::string("1"));

    EXPECT_TRUE(ctx,
                findEdgeByNames(graph,
                                "realtime.av.start_barrier",
                                "video_packet",
                                "realtime.video.rtp.mux",
                                "packet") != nullptr);
    EXPECT_TRUE(ctx,
                findEdgeByNames(graph,
                                "realtime.av.start_barrier",
                                "audio_packet",
                                "realtime.audio.rtp.mux",
                                "packet") != nullptr);
    EXPECT_TRUE(ctx,
                findEdgeByNames(graph,
                                "realtime.audio.copy.normalize",
                                "packet",
                                "realtime.av.start_barrier",
                                "audio_packet") != nullptr);

    const MediaNode* packetStartGate = findNodeByName(graph, "realtime.video.transcode.packet_start_gate");
    EXPECT_TRUE(ctx, packetStartGate != nullptr);
    if (packetStartGate) {
        EXPECT_EQ(ctx, packetStartGate->kind, MediaNodeKind::PacketStartGate);
        EXPECT_EQ(ctx, packetStartGate->options.value("packet_start_gate.require_key_frame"), std::string("1"));
    }
    EXPECT_TRUE(ctx,
                findEdgeByNames(graph,
                                "realtime.raw_rtp.normalize",
                                "packet",
                                "realtime.video.transcode.packet_start_gate",
                                "packet") != nullptr);
    EXPECT_TRUE(ctx,
                findEdgeByNames(graph,
                                "realtime.video.transcode.packet_start_gate",
                                "packet",
                                "realtime.video.transcode.decode",
                                "packet") != nullptr);
}

void testStartBarrierKeepsLatestPreOpenPacket(TestContext& ctx)
{
    MediaGraph graph;
    const MediaEdgePolicy policy = MediaGraphBuildSupport::blockingQueuePolicy(8);

    const MediaNodeId videoCodecSource = graph.addNode(MediaNodeKind::DebugDump, "test.video_codec_source");
    const MediaNodeId videoPacketSource = graph.addNode(MediaNodeKind::DebugDump, "test.video_packet_source");
    const MediaNodeId audioCodecSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio_codec_source");
    const MediaNodeId audioPacketSource = graph.addNode(MediaNodeKind::DebugDump, "test.audio_packet_source");
    const MediaNodeId barrier = graph.addNode(MediaNodeKind::AvPacketStartBarrier, "test.av_start_barrier");
    const MediaNodeId videoCodecSink = graph.addNode(MediaNodeKind::DebugDump, "test.video_codec_sink");
    const MediaNodeId videoPacketSink = graph.addNode(MediaNodeKind::DebugDump, "test.video_packet_sink");
    const MediaNodeId audioCodecSink = graph.addNode(MediaNodeKind::DebugDump, "test.audio_codec_sink");
    const MediaNodeId audioPacketSink = graph.addNode(MediaNodeKind::DebugDump, "test.audio_packet_sink");

    graph.setNodeOption(barrier, "av_start_barrier.expect_video", "1");
    graph.setNodeOption(barrier, "av_start_barrier.expect_audio", "1");
    graph.setNodeOption(barrier, "av_start_barrier.require_video_key_frame", "1");

    graph.addOutputPort(videoCodecSource, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(videoPacketSource, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(audioCodecSource, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(audioPacketSource, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);

    graph.addInputPort(barrier, "video_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(barrier, "video_packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(barrier, "audio_codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(barrier, "audio_packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(barrier, "video_codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(barrier, "video_packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(barrier, "audio_codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addOutputPort(barrier, "audio_packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);

    graph.addInputPort(videoCodecSink, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(videoPacketSink, "packet", MediaStreamKind::Video, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    graph.addInputPort(audioCodecSink, "codec", MediaStreamKind::Audio, MediaEdgeKind::Metadata, MediaPayloadKind::Unknown);
    graph.addInputPort(audioPacketSink, "packet", MediaStreamKind::Audio, MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);

    graph.connect(videoCodecSource, "codec", barrier, "video_codec", "test.video_codec -> barrier", policy);
    graph.connect(videoPacketSource, "packet", barrier, "video_packet", "test.video_packet -> barrier", policy);
    graph.connect(audioCodecSource, "codec", barrier, "audio_codec", "test.audio_codec -> barrier", policy);
    graph.connect(audioPacketSource, "packet", barrier, "audio_packet", "test.audio_packet -> barrier", policy);
    graph.connect(barrier, "video_codec", videoCodecSink, "codec", "test.barrier -> video_codec", policy);
    graph.connect(barrier, "video_packet", videoPacketSink, "packet", "test.barrier -> video_packet", policy);
    graph.connect(barrier, "audio_codec", audioCodecSink, "codec", "test.barrier -> audio_codec", policy);
    graph.connect(barrier, "audio_packet", audioPacketSink, "packet", "test.barrier -> audio_packet", policy);

    MediaGraphExecutionContext execution;
    const auto compileStatus = execution.compile(graph);
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
        return;
    }

    AvPacketStartBarrierNode node(barrier);
    MediaChannel* audioInput = execution.findInputChannel(barrier, "audio_packet");
    MediaChannel* videoInput = execution.findInputChannel(barrier, "video_packet");
    MediaChannel* audioOutput = execution.findOutputChannel(barrier, "audio_packet");
    MediaChannel* videoOutput = execution.findOutputChannel(barrier, "video_packet");
    EXPECT_TRUE(ctx, audioInput != nullptr);
    EXPECT_TRUE(ctx, videoInput != nullptr);
    EXPECT_TRUE(ctx, audioOutput != nullptr);
    EXPECT_TRUE(ctx, videoOutput != nullptr);
    if (!audioInput || !videoInput || !audioOutput || !videoOutput) {
        return;
    }

    auto earlyAudio = makePacketBuffer(true, 100, MediaStreamKind::Audio);
    auto latestAudio = makePacketBuffer(true, 200, MediaStreamKind::Audio);
    auto deltaVideo = makePacketBuffer(false, 250, MediaStreamKind::Video);
    auto firstVideo = makePacketBuffer(true, 300, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, earlyAudio);
    EXPECT_TRUE(ctx, latestAudio);
    EXPECT_TRUE(ctx, deltaVideo);
    EXPECT_TRUE(ctx, firstVideo);
    if (!earlyAudio || !latestAudio || !deltaVideo || !firstVideo) {
        return;
    }

    EXPECT_TRUE(ctx, audioInput->push(earlyAudio.value()));
    auto earlyResult = node.process(execution);
    EXPECT_TRUE(ctx, earlyResult);
    if (earlyResult) EXPECT_EQ(ctx, earlyResult.value().state, MediaNodeProcessState::Progress);
    MediaBufferRef output;
    EXPECT_FALSE(ctx, audioOutput->tryPop(output));

    EXPECT_TRUE(ctx, audioInput->push(latestAudio.value()));
    auto latestResult = node.process(execution);
    EXPECT_TRUE(ctx, latestResult);
    if (latestResult) EXPECT_EQ(ctx, latestResult.value().state, MediaNodeProcessState::Progress);
    EXPECT_FALSE(ctx, audioOutput->tryPop(output));

    EXPECT_TRUE(ctx, videoInput->push(deltaVideo.value()));
    auto deltaResult = node.process(execution);
    EXPECT_TRUE(ctx, deltaResult);
    if (deltaResult) EXPECT_EQ(ctx, deltaResult.value().state, MediaNodeProcessState::Progress);
    EXPECT_FALSE(ctx, videoOutput->tryPop(output));
    EXPECT_FALSE(ctx, audioOutput->tryPop(output));

    EXPECT_TRUE(ctx, videoInput->push(firstVideo.value()));
    EXPECT_TRUE(ctx, node.process(execution));

    EXPECT_TRUE(ctx, videoOutput->tryPop(output));
    if (output) {
        EXPECT_EQ(ctx, output->pts(), static_cast<MediaTimeValue>(300));
    }
    EXPECT_TRUE(ctx, audioOutput->tryPop(output));
    if (output) {
        EXPECT_EQ(ctx, output->pts(), static_cast<MediaTimeValue>(200));
    }

    auto queuedVideo1 = makePacketBuffer(true, 400, MediaStreamKind::Video);
    auto queuedVideo2 = makePacketBuffer(true, 500, MediaStreamKind::Video);
    auto queuedVideo3 = makePacketBuffer(true, 600, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, queuedVideo1 && queuedVideo2 && queuedVideo3);
    if (queuedVideo1 && queuedVideo2 && queuedVideo3) {
        EXPECT_TRUE(ctx, videoInput->push(queuedVideo1.value()));
        EXPECT_TRUE(ctx, videoInput->push(queuedVideo2.value()));
        EXPECT_TRUE(ctx, videoInput->push(queuedVideo3.value()));
        MediaGraphWorker worker(node, execution);
        EXPECT_TRUE(ctx, worker.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        EXPECT_EQ(ctx, videoInput->size(), static_cast<std::size_t>(0));
        EXPECT_TRUE(ctx, worker.metrics().progress.load() >= static_cast<std::uint64_t>(3));
        worker.requestStop();
        worker.join();
    }
}

void testPacketStartGateOpensOnKeyFrame(TestContext& ctx)
{
    MediaGraph graph;
    const MediaEdgePolicy policy = MediaGraphBuildSupport::blockingQueuePolicy(4);

    const MediaNodeId source = graph.addNode(MediaNodeKind::DebugDump, "test.packet_source");
    const MediaNodeId gate = graph.addNode(MediaNodeKind::PacketStartGate, "test.packet_start_gate");
    const MediaNodeId sink = graph.addNode(MediaNodeKind::DebugDump, "test.packet_sink");

    graph.setNodeOption(gate, "packet_start_gate.require_key_frame", "1");
    graph.addOutputPort(source, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.addInputPort(gate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.addOutputPort(gate, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.addInputPort(sink, "packet", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.connect(source, "packet", gate, "packet", "test.source -> gate", policy);
    graph.connect(gate, "packet", sink, "packet", "test.gate -> sink", policy);

    MediaGraphExecutionContext execution;
    const auto compileStatus = execution.compile(graph);
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        return;
    }

    PacketStartGateNode node(gate);
    MediaChannel* input = execution.findInputChannel(gate, "packet");
    MediaChannel* output = execution.findOutputChannel(gate, "packet");
    EXPECT_TRUE(ctx, input != nullptr);
    EXPECT_TRUE(ctx, output != nullptr);
    if (!input || !output) {
        return;
    }

    auto delta = makePacketBuffer(false, 1, MediaStreamKind::Video);
    auto key = makePacketBuffer(true, 2, MediaStreamKind::Video);
    EXPECT_TRUE(ctx, delta);
    EXPECT_TRUE(ctx, key);
    if (!delta || !key) {
        return;
    }

    EXPECT_TRUE(ctx, input->push(delta.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    MediaBufferRef outputBuffer;
    EXPECT_FALSE(ctx, output->tryPop(outputBuffer));

    EXPECT_TRUE(ctx, input->push(key.value()));
    EXPECT_TRUE(ctx, node.process(execution));
    EXPECT_TRUE(ctx, output->tryPop(outputBuffer));
    if (outputBuffer) {
        EXPECT_TRUE(ctx, outputBuffer->isKeyFrame());
        EXPECT_EQ(ctx, outputBuffer->pts(), static_cast<MediaTimeValue>(2));
    }
}

void testBuildPlansMpegTsUdpMuxedOutputGraph(TestContext& ctx)
{
    auto source = LocalMpegTsUdpSource::start();
    EXPECT_TRUE(ctx, source);
    if (!source) {
        std::cerr << source.error().describe() << '\n';
        return;
    }

    auto preflightResult = MediaRealtimeRtpTranscodePlanner::preflight(validMpegTsUdpOptions());
    auto planResult = preflightResult
        ? ::media::Result<MediaRealtimeRtpTranscodePlan>::success(preflightResult.value().plan)
        : ::media::Result<MediaRealtimeRtpTranscodePlan>::failure(preflightResult.error());
    EXPECT_TRUE(ctx, planResult);
    if (!planResult) {
        std::cerr << planResult.error().describe() << '\n';
        return;
    }
    EXPECT_TRUE(ctx, planResult.value().videoInputStartRequiresKeyFrame);

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(std::move(planResult).value());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RealtimeInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::StreamSplit) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::FileOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::FileMux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::PacketStartGate) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::SdpWriter), static_cast<std::size_t>(0));

    const MediaNode* fileOutput = findNodeByKind(graph, MediaNodeKind::FileOutput);
    EXPECT_TRUE(ctx, fileOutput != nullptr);
    if (fileOutput) {
        EXPECT_EQ(ctx, fileOutput->options.value("url"), mpegTsUdpOutputUrl());
        EXPECT_EQ(ctx, fileOutput->options.value("format"), std::string("mpegts"));
    }
}

void testRealtimeBuilderDoesNotOwnPlannerDecisions(TestContext& ctx)
{
    const auto source = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "builder" /
                                     "realtime" /
                                     "MediaRealtimeRtpTranscodeGraphBuilder.cpp");
    EXPECT_FALSE(ctx, source.empty());
    EXPECT_FALSE(ctx, source.find("MediaPipelinePlanner::") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("MediaPipelinePlannerOptions") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("planRealtimeQueuePolicy") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("blockingQueuePolicy") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("isUnsupportedRealtimeInputUrl") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("preferredHardware") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("outputCodecName") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("bFrames") != std::string::npos);
}

void testPacketNormalizeMonotonicPolicyIncludesPtsAndDts(TestContext& ctx)
{
    const auto source = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "nodes" /
                                     "packet" /
                                     "PacketNormalizeNode.cpp");
    EXPECT_FALSE(ctx, source.empty());
    EXPECT_TRUE(ctx, source.find("packet->dts < m_nextPacketDts") != std::string::npos);
    EXPECT_TRUE(ctx, source.find("packet->pts < m_nextPacketDts") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("const int64_t packetDts = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts") != std::string::npos);
}

void expectGraphCompiles(TestContext& ctx, MediaGraph graph)
{
    const auto validation = MediaGraphValidation::validate(graph);
    EXPECT_TRUE(ctx, validation.ok());
    if (!validation.ok()) {
        for (const auto& issue : validation.issues) {
            std::cerr << "validation issue: " << issue.message
                      << " node=" << issue.nodeId.value
                      << " port=" << issue.portId.value
                      << " edge=" << issue.edgeId.value << '\n';
        }
        return;
    }

    MediaGraphRuntime runtime;
    const auto compileStatus = runtime.compile(std::move(graph));
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
    }
}

void testRuntimeCompileSupportsSoftwareChain(TestContext& ctx)
{
    auto graphResult = preparedGraph(validRealtimeOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testRuntimeCompileSupportsRawRtpChain(TestContext& ctx)
{
    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testRuntimeCompileSupportsAutoHardwareChain(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.execution.disableHardware = false;

    auto graphResult = preparedGraph(options);
    if (!graphResult) {
        if (graphResult.error().code == ::media::ErrorCode::HardwareUnavailable) {
            std::cout << "SKIPPED: unsupported auto hardware chain: "
                      << graphResult.error().describe() << '\n';
            return;
        }
        EXPECT_TRUE(ctx, graphResult);
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testUrlRedactionHidesUserInfo(TestContext& ctx)
{
    EXPECT_EQ(ctx,
              redactUrlUserInfo("rtsp://user:password@example.invalid:554/Streaming/Channels/302"),
              std::string("rtsp://<redacted>@example.invalid:554/Streaming/Channels/302"));
    EXPECT_EQ(ctx,
              redactUrlUserInfo("rtsp://example.invalid/Streaming/Channels/302"),
              std::string("rtsp://example.invalid/Streaming/Channels/302"));
}

void testTimestampRescaleBumpsQuantizedDuplicates(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational encoderTimeBase{ 1, 25 };

    const auto first = rescaleStrictlyIncreasingTimestamp(540000,
                                                          sourceTimeBase,
                                                          encoderTimeBase,
                                                          AV_NOPTS_VALUE);
    EXPECT_TRUE(ctx, first);
    if (!first) {
        return;
    }
    EXPECT_EQ(ctx, first.value(), 150);

    const auto duplicate = rescaleStrictlyIncreasingTimestamp(540010,
                                                              sourceTimeBase,
                                                              encoderTimeBase,
                                                              first.value());
    EXPECT_TRUE(ctx, duplicate);
    if (!duplicate) {
        return;
    }
    EXPECT_EQ(ctx, duplicate.value(), 151);

    const auto nextQuantized = rescaleStrictlyIncreasingTimestamp(543600,
                                                                  sourceTimeBase,
                                                                  encoderTimeBase,
                                                                  duplicate.value());
    EXPECT_TRUE(ctx, nextQuantized);
    if (nextQuantized) {
        EXPECT_EQ(ctx, nextQuantized.value(), 152);
    }
}

void testTimestampRescaleRejectsInvalidBoundaryValues(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational encoderTimeBase{ 1, 25 };

    const auto invalidPts = rescaleStrictlyIncreasingTimestamp(AV_NOPTS_VALUE,
                                                               sourceTimeBase,
                                                               encoderTimeBase,
                                                               AV_NOPTS_VALUE);
    EXPECT_FALSE(ctx, invalidPts);
    if (!invalidPts) {
        EXPECT_EQ(ctx, invalidPts.error().code, media::ErrorCode::InvalidArgument);
    }

    const auto invalidTimeBase = rescaleStrictlyIncreasingTimestamp(540000,
                                                                    AVRational{ 0, 1 },
                                                                    encoderTimeBase,
                                                                    AV_NOPTS_VALUE);
    EXPECT_FALSE(ctx, invalidTimeBase);
    if (!invalidTimeBase) {
        EXPECT_EQ(ctx, invalidTimeBase.error().code, media::ErrorCode::InvalidArgument);
    }

    const auto maxLastPts = rescaleStrictlyIncreasingTimestamp(540000,
                                                               sourceTimeBase,
                                                               encoderTimeBase,
                                                               std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(ctx, maxLastPts);
    if (!maxLastPts) {
        EXPECT_EQ(ctx, maxLastPts.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testSyntheticTimestampsAdvanceAfterRealTimestamp(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational frameStepTimeBase{ 1, 30 };

    const auto firstMissing = nextSyntheticTimestamp(AV_NOPTS_VALUE,
                                                     frameStepTimeBase,
                                                     sourceTimeBase);
    EXPECT_TRUE(ctx, firstMissing);
    if (firstMissing) {
        EXPECT_EQ(ctx, firstMissing.value(), 0);
    }

    const auto afterReal = nextSyntheticTimestamp(900000,
                                                  frameStepTimeBase,
                                                  sourceTimeBase);
    EXPECT_TRUE(ctx, afterReal);
    if (afterReal) {
        EXPECT_EQ(ctx, afterReal.value(), 903000);
    }

    const auto invalidTimeBase = nextSyntheticTimestamp(900000,
                                                        AVRational{ 0, 1 },
                                                        sourceTimeBase);
    EXPECT_FALSE(ctx, invalidTimeBase);
    if (!invalidTimeBase) {
        EXPECT_EQ(ctx, invalidTimeBase.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testAudioTimestampClampsBackwardRtpFrames(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 44100 };
    const AVRational encoderTimeBase{ 1, 44100 };

    const auto first = monotonicAudioFrameTimestamp(177152,
                                                    sourceTimeBase,
                                                    encoderTimeBase,
                                                    AV_NOPTS_VALUE);
    EXPECT_TRUE(ctx, first);
    if (!first) {
        return;
    }
    EXPECT_EQ(ctx, first.value(), 177152);

    const auto next = nextAudioFrameTimestamp(first.value(), 1024);
    EXPECT_TRUE(ctx, next);
    if (!next) {
        return;
    }
    EXPECT_EQ(ctx, next.value(), 178176);

    const auto backward = monotonicAudioFrameTimestamp(175190,
                                                       sourceTimeBase,
                                                       encoderTimeBase,
                                                       next.value());
    EXPECT_TRUE(ctx, backward);
    if (backward) {
        EXPECT_EQ(ctx, backward.value(), 178176);
    }
}

void testAudioTimestampRejectsMissingSourcePts(TestContext& ctx)
{
    const auto missingPts = monotonicAudioFrameTimestamp(AV_NOPTS_VALUE,
                                                        AVRational{ 1, 44100 },
                                                        AVRational{ 1, 44100 },
                                                        AV_NOPTS_VALUE);
    EXPECT_FALSE(ctx, missingPts);
    if (!missingPts) {
        EXPECT_EQ(ctx, missingPts.error().code, media::ErrorCode::InvalidArgument);
    }

    const auto invalidStep = nextAudioFrameTimestamp(1024, 0);
    EXPECT_FALSE(ctx, invalidStep);
    if (!invalidStep) {
        EXPECT_EQ(ctx, invalidStep.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testAudioResampleNodeClampsBackwardClonedFrameTimestamps(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId resampleId = addAudioResampleHarnessGraph(graph);
    MediaGraphExecutionContext execution;
    const auto compileStatus = execution.compile(graph);
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
        return;
    }

    AudioResampleNode node(resampleId);
    auto codec = makeTestAudioCodecContext(44100, AV_SAMPLE_FMT_FLTP, 2);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) {
        return;
    }
    const auto bindStatus = bindAudioResampleCodec(execution, node, resampleId, std::move(codec));
    EXPECT_TRUE(ctx, bindStatus);
    if (!bindStatus) {
        std::cerr << bindStatus.error().describe() << '\n';
        return;
    }

    auto firstInput = makeTestAudioFrame(44100, AV_SAMPLE_FMT_FLTP, 2, 177152, 1024);
    EXPECT_TRUE(ctx, firstInput);
    if (!firstInput) {
        std::cerr << firstInput.error().describe() << '\n';
        return;
    }
    auto firstOutput = processAudioResampleFrame(execution, node, resampleId, firstInput.value());
    EXPECT_TRUE(ctx, firstOutput);
    if (!firstOutput) {
        std::cerr << firstOutput.error().describe() << '\n';
        return;
    }
    const AVFrame* firstFrame = FFmpegFrameView::frame(firstOutput.value());
    EXPECT_TRUE(ctx, firstFrame != nullptr);
    if (!firstFrame) {
        return;
    }
    EXPECT_EQ(ctx, firstFrame->pts, 177152);
    EXPECT_EQ(ctx, firstFrame->pkt_dts, static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, firstFrame->duration, 1024);
    EXPECT_EQ(ctx, firstOutput.value()->pts(), 177152);
    EXPECT_EQ(ctx, firstOutput.value()->dts(), static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, firstOutput.value()->duration(), 1024);

    auto backwardInput = makeTestAudioFrame(44100, AV_SAMPLE_FMT_FLTP, 2, 175190, 1024);
    EXPECT_TRUE(ctx, backwardInput);
    if (!backwardInput) {
        std::cerr << backwardInput.error().describe() << '\n';
        return;
    }
    auto backwardOutput = processAudioResampleFrame(execution, node, resampleId, backwardInput.value());
    EXPECT_TRUE(ctx, backwardOutput);
    if (!backwardOutput) {
        std::cerr << backwardOutput.error().describe() << '\n';
        return;
    }
    const AVFrame* backwardFrame = FFmpegFrameView::frame(backwardOutput.value());
    EXPECT_TRUE(ctx, backwardFrame != nullptr);
    if (!backwardFrame) {
        return;
    }
    EXPECT_EQ(ctx, backwardFrame->pts, 178176);
    EXPECT_EQ(ctx, backwardFrame->pkt_dts, static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, backwardFrame->duration, 1024);
    EXPECT_EQ(ctx, backwardOutput.value()->pts(), 178176);
    EXPECT_EQ(ctx, backwardOutput.value()->dts(), static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, backwardOutput.value()->duration(), 1024);
}

void testAudioResampleNodeNormalizesResampledFrameTimestamps(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId resampleId = addAudioResampleHarnessGraph(graph);
    MediaGraphExecutionContext execution;
    const auto compileStatus = execution.compile(graph);
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
        return;
    }

    AudioResampleNode node(resampleId);
    auto codec = makeTestAudioCodecContext(44100, AV_SAMPLE_FMT_FLTP, 2);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) {
        return;
    }
    const auto bindStatus = bindAudioResampleCodec(execution, node, resampleId, std::move(codec));
    EXPECT_TRUE(ctx, bindStatus);
    if (!bindStatus) {
        std::cerr << bindStatus.error().describe() << '\n';
        return;
    }

    auto input = makeTestAudioFrame(48000, AV_SAMPLE_FMT_S16, 2, 48000, 1024);
    EXPECT_TRUE(ctx, input);
    if (!input) {
        std::cerr << input.error().describe() << '\n';
        return;
    }
    auto output = processAudioResampleFrame(execution, node, resampleId, input.value());
    EXPECT_TRUE(ctx, output);
    if (!output) {
        std::cerr << output.error().describe() << '\n';
        return;
    }
    const AVFrame* frame = FFmpegFrameView::frame(output.value());
    EXPECT_TRUE(ctx, frame != nullptr);
    if (!frame) {
        return;
    }
    const int64_t expectedPts = av_rescale_q(48000, AVRational{ 1, 48000 }, AVRational{ 1, 44100 });
    EXPECT_EQ(ctx, frame->pts, expectedPts);
    EXPECT_EQ(ctx, frame->pkt_dts, static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, frame->duration, frame->nb_samples);
    EXPECT_EQ(ctx, output.value()->pts(), expectedPts);
    EXPECT_EQ(ctx, output.value()->dts(), static_cast<int64_t>(AV_NOPTS_VALUE));
    EXPECT_EQ(ctx, output.value()->duration(), frame->nb_samples);
}

void testAudioResampleNodeRejectsMissingFramePts(TestContext& ctx)
{
    MediaGraph graph;
    const MediaNodeId resampleId = addAudioResampleHarnessGraph(graph);
    MediaGraphExecutionContext execution;
    const auto compileStatus = execution.compile(graph);
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
        return;
    }

    AudioResampleNode node(resampleId);
    auto codec = makeTestAudioCodecContext(44100, AV_SAMPLE_FMT_FLTP, 2);
    EXPECT_TRUE(ctx, codec != nullptr);
    if (!codec) {
        return;
    }
    const auto bindStatus = bindAudioResampleCodec(execution, node, resampleId, std::move(codec));
    EXPECT_TRUE(ctx, bindStatus);
    if (!bindStatus) {
        std::cerr << bindStatus.error().describe() << '\n';
        return;
    }

    auto input = makeTestAudioFrame(44100, AV_SAMPLE_FMT_FLTP, 2, AV_NOPTS_VALUE, 1024);
    EXPECT_TRUE(ctx, input);
    if (!input) {
        std::cerr << input.error().describe() << '\n';
        return;
    }
    const auto output = processAudioResampleFrame(execution, node, resampleId, input.value());
    EXPECT_FALSE(ctx, output);
    if (!output) {
        EXPECT_EQ(ctx, output.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testPreparedRealtimeInputIsMoveOnlyAndSourceBecomesEmpty(TestContext& ctx)
{
    auto context = ::media::ffmpeg::InputFormatContextPtr(avformat_alloc_context());
    EXPECT_TRUE(ctx, context != nullptr);
    if (!context) return;
    AVStream* stream = avformat_new_stream(context.get(), nullptr);
    EXPECT_TRUE(ctx, stream != nullptr && stream->codecpar != nullptr);
    if (!stream || !stream->codecpar) return;
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_H264;
    auto prepared = MediaPreparedRealtimeInput::create(std::move(context));
    EXPECT_TRUE(ctx, prepared);
    if (!prepared) return;
    MediaPreparedRealtimeInput source = std::move(prepared).value();
    EXPECT_TRUE(ctx, source.valid());
    MediaPreparedRealtimeInput destination = std::move(source);
    EXPECT_FALSE(ctx, source.valid());
    EXPECT_TRUE(ctx, destination.valid());
    EXPECT_TRUE(ctx, destination.inputStreamSnapshot(0) != nullptr);
    static_assert(!std::is_copy_constructible_v<MediaPreparedRealtimeInput>);
    static_assert(!std::is_copy_assignable_v<MediaPreparedRealtimeInput>);
}

MediaPreparedRealtimeInput makePreparedInputForBinding(TestContext& ctx)
{
    auto context = ::media::ffmpeg::InputFormatContextPtr(avformat_alloc_context());
    if (!context) return {};
    AVStream* stream = avformat_new_stream(context.get(), nullptr);
    if (!stream || !stream->codecpar) return {};
    stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codecpar->codec_id = AV_CODEC_ID_H264;
    auto prepared = MediaPreparedRealtimeInput::create(std::move(context));
    EXPECT_TRUE(ctx, prepared);
    return prepared ? std::move(prepared).value() : MediaPreparedRealtimeInput{};
}

void testPreparedRealtimeScannerInvokesInjectedOpenerOnce(TestContext& ctx)
{
    int openCount = 0;
    MediaPipelinePlannerOptions options(false, false, false, true, false, false);
    auto scanned = MediaPipelineCapabilityScanner::prepareRealtimeInput(
        sampleVideoPath(), options, false,
        [&openCount](const std::string& url, AVDictionary** inputOptions)
            -> ::media::Result<::media::ffmpeg::InputFormatContextPtr> {
            ++openCount;
            AVFormatContext* raw = nullptr;
            const int result = avformat_open_input(&raw, url.c_str(), nullptr, inputOptions);
            if (result < 0) {
                return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::failure(
                    ::media::ErrorInfo::ffmpegFailure("test opener failed", result));
            }
            return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::success(
                ::media::ffmpeg::InputFormatContextPtr(raw));
        });
    EXPECT_TRUE(ctx, scanned);
    EXPECT_EQ(ctx, openCount, 1);
    EXPECT_TRUE(ctx, scanned && scanned.value().prepared.valid());
}

void testExecutableRuntimeRejectsMissingAndDuplicatePreparedBindings(TestContext& ctx)
{
    MediaRealtimeExecutableGraph missing;
    const MediaNodeId inputId = missing.graph.addNode(MediaNodeKind::RealtimeInput, "prepared.input");
    MediaGraphRuntime missingRuntime;
    auto missingStatus = missingRuntime.compile(std::move(missing));
    EXPECT_FALSE(ctx, missingStatus);
    if (!missingStatus) EXPECT_EQ(ctx, missingStatus.error().code, ::media::ErrorCode::NotInitialized);

    MediaRealtimeExecutableGraph duplicate;
    const MediaNodeId duplicateId = duplicate.graph.addNode(MediaNodeKind::RealtimeInput, "prepared.input");
    duplicate.inputBindings.push_back({duplicateId, makePreparedInputForBinding(ctx)});
    duplicate.inputBindings.push_back({duplicateId, makePreparedInputForBinding(ctx)});
    {
        MediaGraphRuntime duplicateRuntime;
        auto duplicateStatus = duplicateRuntime.compile(std::move(duplicate));
        EXPECT_FALSE(ctx, duplicateStatus);
        if (!duplicateStatus) EXPECT_EQ(ctx, duplicateStatus.error().code, ::media::ErrorCode::InvalidArgument);
    }
}

MediaRealtimeExecutableGraph makePreparedExecutableGraph(TestContext& ctx)
{
    MediaRealtimeExecutableGraph executable;
    const MediaNodeId inputId = executable.graph.addNode(MediaNodeKind::RealtimeInput, "transaction.input");
    executable.inputBindings.push_back({inputId, makePreparedInputForBinding(ctx)});
    return executable;
}

MediaGraph makeInvalidUnconnectedRequiredInputGraph()
{
    MediaGraph graph;
    const MediaNodeId node = graph.addNode(MediaNodeKind::ControlSignal, "invalid.required.input");
    graph.addInputPort(node, "required", MediaStreamKind::Control,
                       MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, true);
    return graph;
}

void testRuntimeCompileFailurePreservesPreviousGraphAndBindings(TestContext& ctx)
{
    MediaGraphRuntime executableRuntime;
    auto executableStatus = executableRuntime.compile(makePreparedExecutableGraph(ctx));
    EXPECT_TRUE(ctx, executableStatus);
    const MediaGraph* executableGraph = executableRuntime.graph();
    EXPECT_TRUE(ctx, executableGraph && executableGraph->nodes().size() == 1);

    MediaRealtimeExecutableGraph missing;
    missing.graph.addNode(MediaNodeKind::RealtimeInput, "missing.input");
    EXPECT_FALSE(ctx, executableRuntime.compile(std::move(missing)));
    EXPECT_EQ(ctx, executableRuntime.state(), MediaGraphRuntimeState::Compiled);
    EXPECT_TRUE(ctx, executableRuntime.graph() == executableGraph);
    EXPECT_TRUE(ctx, executableRuntime.registerDefaultRuntimeNodes());

    MediaGraphRuntime plainRuntime;
    MediaGraph plain;
    plain.addNode(MediaNodeKind::ControlSignal, "transaction.control");
    EXPECT_TRUE(ctx, plainRuntime.compile(std::move(plain)));
    const MediaGraph* plainGraph = plainRuntime.graph();
    EXPECT_FALSE(ctx, plainRuntime.compile(makeInvalidUnconnectedRequiredInputGraph()));
    EXPECT_EQ(ctx, plainRuntime.state(), MediaGraphRuntimeState::Compiled);
    EXPECT_TRUE(ctx, plainRuntime.graph() == plainGraph);
    EXPECT_TRUE(ctx, plainRuntime.registerDefaultRuntimeNodes());
    auto run = plainRuntime.run();
    EXPECT_TRUE(ctx, run && run.value().completed);
}

void testSuccessfulPlainCompileReplacesPreparedBindingWithSameNodeId(TestContext& ctx)
{
    MediaGraphRuntime runtime;
    EXPECT_TRUE(ctx, runtime.compile(makePreparedExecutableGraph(ctx)));
    MediaGraph plain;
    const MediaNodeId plainId = plain.addNode(MediaNodeKind::ControlSignal, "plain.same.id");
    EXPECT_EQ(ctx, plainId.value, static_cast<std::uint32_t>(1));
    EXPECT_TRUE(ctx, runtime.compile(std::move(plain)));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    auto run = runtime.run();
    EXPECT_TRUE(ctx, run && run.value().completed);
}

void testInvalidPreflightDoesNotInvokeOpener(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest invalid = validMpegTsUdpOptions();
    invalid.input.url.clear();
    int openCount = 0;
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(
        invalid,
        [&openCount](const std::string&, AVDictionary**)
            -> ::media::Result<::media::ffmpeg::InputFormatContextPtr> {
            ++openCount;
            return ::media::Result<::media::ffmpeg::InputFormatContextPtr>::failure(
                ::media::ErrorInfo::internalError("opener must not run"));
        });
    EXPECT_FALSE(ctx, preflight);
    EXPECT_EQ(ctx, openCount, 0);
}

void testUrlAndMpegTsPurePlanApisRequirePreparedPreflight(TestContext& ctx)
{
    auto urlPlan = MediaRealtimeRtpTranscodePlanner::plan(validRealtimeOptions());
    EXPECT_FALSE(ctx, urlPlan);
    if (!urlPlan) EXPECT_EQ(ctx, urlPlan.error().code, ::media::ErrorCode::Unsupported);
    auto mpegGraph = MediaRealtimeRtpTranscodeGraphBuilder::build(validMpegTsUdpOptions());
    EXPECT_FALSE(ctx, mpegGraph);
    if (!mpegGraph) EXPECT_EQ(ctx, mpegGraph.error().code, ::media::ErrorCode::Unsupported);
}

} // namespace

int main()
{
    TestContext ctx;

    testPreparedRealtimeInputIsMoveOnlyAndSourceBecomesEmpty(ctx);
    testPreparedRealtimeScannerInvokesInjectedOpenerOnce(ctx);
    testExecutableRuntimeRejectsMissingAndDuplicatePreparedBindings(ctx);
    testRuntimeCompileFailurePreservesPreviousGraphAndBindings(ctx);
    testSuccessfulPlainCompileReplacesPreparedBindingWithSameNodeId(ctx);
    testInvalidPreflightDoesNotInvokeOpener(ctx);
    testUrlAndMpegTsPurePlanApisRequirePreparedPreflight(ctx);
    testValidationRejectsMissingInput(ctx);
    testLegacyArchitectureFilesAreRemoved(ctx);
    testVideoToolsAreSplitIntoDedicatedTargets(ctx);
    testVideoToolsRejectLegacyBusinessSwitches(ctx);
    testGraphRejectsBehaviorDefaultImplementations(ctx);
    testCapabilityScanningResponsibilitiesAreSeparated(ctx);
    testRealtimePlannerOutputPolicyIsSeparated(ctx);
    testRealtimeOutputPolicyInitializesEveryMuxExpectation(ctx);
    testRtpMuxStateMachineRejectsIllegalTransitions(ctx);
    testAudioDecodeWaitsForCodecMetadataBeforePackets(ctx);
    testAudioEncodeWaitsForCodecMetadataBeforeFrames(ctx);
    testRealtimeOutputPolicyRejectsMissingAudioPacingBitrate(ctx);
    testRealtimePlannerNaturallySelectsAudioVideoTranscode(ctx);
    testRealtimePlannerValidationAndInputPlanningAreSeparated(ctx);
    testRtpMuxResponsibilitiesAreSeparated(ctx);
    testRuntimeCompilationAndLifecycleAreSeparated(ctx);
    testPlannerRejectsUnresolvedBehaviorOptions(ctx);
    testValidationRejectsUnsupportedRealtimeInput(ctx);
    testValidationRequiresExplicitRealtimeStreamClassification(ctx);
    testExistingRealtimeModesMapToExplicitLayouts(ctx);
    testUnsupportedRealtimeStreamCombinationsFailInPlanner(ctx);
    testMpegTsUdpRejectsNonUdpInputUrl(ctx);
    testSeparateRtpOutputRejectsSingleOutputUrl(ctx);
    testRawRtpMissingMetadataFailsInPlanner(ctx);
    testRawRtpRejectsUnsupportedMetadata(ctx);
    testRawRtpPlansH264AndHevcInput(ctx);
    testRawRtpAudioEndpointRequiredWhenAudioEnabled(ctx);
    testRawRtpPlansAudioVideoInput(ctx);
    testRawRtpAudioVideoGraphUsesIsolatedInputs(ctx);
    testSeparateRtpH264OutputRequestsGlobalHeader(ctx);
    testSeparateRtpInheritedH264OutputRequestsGlobalHeader(ctx);
    testEncoderContextBuilderAppliesGlobalHeaderOption(ctx);
    testRawRtpInheritsSourceCodecsWhenTranscodeCodecsAreOmitted(ctx);
    testRawRtpMatchingAudioPlansPacketCopy(ctx);
    testRawRtpAudioTranscodesWhenTargetDiffers(ctx);
    testRealtimePlanOwnsThreadingPolicy(ctx);
    testRawRtpRejectsMissingVideoBitrate(ctx);
    testRawRtpRejectsZeroVideoBitrate(ctx);
    testLocalTranscodeRejectsZeroVideoBitrate(ctx);
    testRawRtpRejectsUnknownSourceCodecWhenCodecIsNotExplicit(ctx);
    testRealtimeNoResizeDoesNotScoreFilterStage(ctx);
    testRealtimeResizeScoresFilterStage(ctx);
    testRawRtpPlansOpusAudioInput(ctx);
    testValidationRejectsOddRtpPort(ctx);
    testValidationRejectsAudioRtpPortOverflow(ctx);
    testRealtimeNoAudioProbeDoesNotRequestAudio(ctx);
    testRealtimeUrlInheritsObservableVideoBitrate(ctx);
    testUrlRedactionHidesUserInfo(ctx);
    testTimestampRescaleBumpsQuantizedDuplicates(ctx);
    testTimestampRescaleRejectsInvalidBoundaryValues(ctx);
    testSyntheticTimestampsAdvanceAfterRealTimestamp(ctx);
    testAudioTimestampClampsBackwardRtpFrames(ctx);
    testAudioTimestampRejectsMissingSourcePts(ctx);
    testAudioResampleNodeClampsBackwardClonedFrameTimestamps(ctx);
    testAudioResampleNodeNormalizesResampledFrameTimestamps(ctx);
    testAudioResampleNodeRejectsMissingFramePts(ctx);
    testBuildPlansVideoStreamAndSoftwareExecution(ctx);
    testBuildPlansRealtimeUrlAudioBranch(ctx);
    testBuildPlansRawRtpH264Graph(ctx);
    testBuildPlansRawRtpAudioVideoGraph(ctx);
    testRealtimeRtpDataPathUsesPlannedNonBlockingQueues(ctx);
    testRealtimeCliAppliesPlannerThreadingPolicy(ctx);
    testRtpMuxEmitsSdpSnapshotInsteadOfBorrowedLiveContext(ctx);
    testSdpWriterOrdersSeparateRtpContextsByMediaType(ctx);
    testGraphFixedSleepsAreClassified(ctx);
    testRealtimeWorkerUsesEventDrivenWait(ctx);
    testRealtimeRtpMuxUsesPlannerPacingPolicy(ctx);
    testRealtimeRtpOutputUsesPlannerWritePacingPolicy(ctx);
    testRealtimePlannerOwnsShortGopForRtpKeyFrameRecovery(ctx);
    testPacketKeyFrameFlagMapsToMediaBuffer(ctx);
    testRealtimeQueueDropPoliciesAreNormalBackpressure(ctx);
    testDropNonKeyFramePreservesQueuedKeyFrames(ctx);
    testDropNonKeyFrameRejectsKeyFrameWhenNoNonKeyCanBeDropped(ctx);
    testDropNonKeyFramePushWaitsToPreserveKeyFrames(ctx);
    testRealtimeMuxEdgesUseStreamSpecificPolicies(ctx);
    testSeparateRtpAudioVideoUsesStartBarrier(ctx);
    testStartBarrierKeepsLatestPreOpenPacket(ctx);
    testPacketStartGateOpensOnKeyFrame(ctx);
    testBuildPlansMpegTsUdpMuxedOutputGraph(ctx);
    testRealtimeBuilderDoesNotOwnPlannerDecisions(ctx);
    testPacketNormalizeMonotonicPolicyIncludesPtsAndDts(ctx);
    testRuntimeCompileSupportsSoftwareChain(ctx);
    testRuntimeCompileSupportsRawRtpChain(ctx);
    testRuntimeCompileSupportsAutoHardwareChain(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " realtime graph test expectation(s) failed\n";
        return 1;
    }

    std::cout << "realtime graph tests passed\n";
    return 0;
}
