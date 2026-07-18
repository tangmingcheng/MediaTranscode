#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"
#include "unit/fixtures/ScheduledRtpDecodeSampleFixture.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSessionFactory.h"
#include "internal/graph/nodes/output/MediaDualMediaSdpPublisherNode.h"
#include "internal/graph/nodes/output/MediaScheduledRtpOpenTransaction.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "internal/graph/runtime/filesystem/MediaWin32AtomicFileReplacePort.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpDatagramSenderSocket.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::ScheduledRtpDecodeReceiver;
using media_transcode::test::ScheduledRtpDecodeSampleFixture;
using media_transcode::test::scheduledRtpFrameMd5HasData;

constexpr MediaRunningTime milliseconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class TemporaryArtifacts final {
public:
    TemporaryArtifacts()
    {
        const auto suffix = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto directory = std::filesystem::temp_directory_path();
        m_sdp = directory / ("scheduled_rtp_decode_" + suffix + ".sdp");
        m_video = directory /
            ("scheduled_rtp_decode_video_" + suffix + ".framemd5");
        m_audio = directory /
            ("scheduled_rtp_decode_audio_" + suffix + ".framemd5");
        m_log = directory / ("scheduled_rtp_decode_" + suffix + ".log");
    }

    TemporaryArtifacts(const TemporaryArtifacts&) = delete;
    TemporaryArtifacts& operator=(const TemporaryArtifacts&) = delete;

    ~TemporaryArtifacts()
    {
        std::error_code ignored;
        std::filesystem::remove(m_sdp, ignored);
        std::filesystem::remove(m_video, ignored);
        std::filesystem::remove(m_audio, ignored);
        std::filesystem::remove(m_log, ignored);
    }

    const std::filesystem::path& sdp() const noexcept { return m_sdp; }
    const std::filesystem::path& video() const noexcept { return m_video; }
    const std::filesystem::path& audio() const noexcept { return m_audio; }
    const std::filesystem::path& log() const noexcept { return m_log; }

private:
    std::filesystem::path m_sdp;
    std::filesystem::path m_video;
    std::filesystem::path m_audio;
    std::filesystem::path m_log;
};

class OpenSenderResources final {
public:
    explicit OpenSenderResources(MediaOpenedScheduledRtpSender opened) noexcept
        : m_transport(opened.releaseTransport()),
          m_sender(opened.releaseSender())
    {
    }

    OpenSenderResources(OpenSenderResources&&) noexcept = default;
    OpenSenderResources& operator=(OpenSenderResources&&) noexcept = default;
    OpenSenderResources(const OpenSenderResources&) = delete;
    OpenSenderResources& operator=(const OpenSenderResources&) = delete;

    ~OpenSenderResources()
    {
        if (m_transport) (void)m_transport->close();
    }

    ScheduledRtpSenderSession& sender() noexcept { return *m_sender; }

private:
    std::unique_ptr<MediaRtpUdpSenderTransport> m_transport;
    std::unique_ptr<ScheduledRtpSenderSession> m_sender;
};

struct PreparedSender final {
    MediaRtpUdpSenderConfig transport;
    ScheduledRtpSenderConfig sender;
    MediaBufferRef description;
};

int failureExit(const ::media::ErrorInfo& error)
{
    std::cerr << error.describe() << '\n';
    return error.code == ::media::ErrorCode::Unsupported ? 77 : 1;
}

std::filesystem::path samplePath()
{
    return std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
        "tests/samples/sample_h264_aac_2560x1440.mp4";
}

std::filesystem::path ffmpegPath()
{
#if defined(_WIN32)
    std::array<wchar_t, 32'768> module{};
    const DWORD length = GetModuleFileNameW(
        nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length > 0 && length < module.size()) {
        return std::filesystem::path(
                   std::wstring(module.data(), length))
                   .parent_path() /
            "ffmpeg.exe";
    }
#endif
    return "ffmpeg";
}

MediaRealtimeRtpTranscodeRequest request(
    std::uint16_t basePort,
    const std::filesystem::path& sdp)
{
    MediaRealtimeRtpTranscodeRequest value;
    value.mediaId = "scheduled-rtp-decode-integration";
    value.input.type = RealtimeInputType::RtpPort;
    value.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    value.input.openTimeoutMs = 5'000;
    value.input.readTimeoutMs = 5'000;
    value.input.analyzeDurationUs = 500'000;
    value.input.probeSizeBytes = 512 * 1024;
    value.input.lowLatency = true;
    value.input.videoRtp.url = "rtp://127.0.0.1:5004";
    value.input.videoRtp.codecName = "h264";
    value.input.videoRtp.payloadType = 96;
    value.input.videoRtp.clockRate = 90'000;
    value.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets="
        "Z01AMpWQAoALWwEQAAA+gAAOpghA,aOuPIA==;"
        "profile-level-id=4D4032";
    value.input.audioRtp.url = "rtp://127.0.0.1:5006";
    value.input.audioRtp.codecName = "aac";
    value.input.audioRtp.payloadType = 97;
    value.input.audioRtp.clockRate = 44'100;
    value.input.audioRtp.channels = 2;
    value.input.audioRtp.bitrateKbps = 320;
    value.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1210;"
        "sizelength=13;indexlength=3;indexdeltalength=3";
    value.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    value.output.host = "127.0.0.1";
    value.output.basePort = basePort;
    value.output.sdpPath = sdp.string();
    value.output.packetSize = 1'200;
    value.parameters.execution.includeAudio = true;
    value.parameters.execution.disableHardware = true;
    value.parameters.video.codecName = "h264";
    value.parameters.video.bitrateKbps = 8'000;
    value.parameters.audio.codecName = "aac";
    value.parameters.audio.sampleRate = 44'100;
    value.parameters.audio.bitrateKbps = 320;
    value.parameters.audio.channels = 2;
    value.parameters.queues.metadata = 8;
    value.parameters.queues.packet = 256;
    value.parameters.queues.frame = 128;
    value.parameters.queues.mux = 256;
    value.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    value.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    value.avSyncStartup.maximumGap = milliseconds(40);
    return value;
}

::media::Result<PreparedSender> prepareSender(
    const MediaScheduledRtpOutputPlan& output,
    const MediaSeparateRtpSdpRuntimePlan& sdp,
    const AVCodecContext& codec,
    const MediaSharedNtpEpoch& ntp,
    const MediaPlaybackEpoch& epoch)
{
    auto materialized = MediaScheduledRtpSenderMaterializer::materialize(
        output, sdp, codec, ntp, epoch);
    if (!materialized) {
        return ::media::Result<PreparedSender>::failure(
            materialized.error());
    }
    return ::media::Result<PreparedSender>::success(PreparedSender{
        materialized.value().releaseTransportConfig(),
        materialized.value().releaseSenderConfig(),
        materialized.value().releaseDescription()});
}

::media::Status publishSdp(
    const std::filesystem::path& path,
    MediaBufferRef videoDescription,
    MediaBufferRef audioDescription)
{
    MediaGraph graph;
    const MediaNodeId videoSource = graph.addNode(
        MediaNodeKind::DebugDump, "video-description");
    const MediaNodeId audioSource = graph.addNode(
        MediaNodeKind::DebugDump, "audio-description");
    const MediaNodeId publisher = graph.addNode(
        MediaNodeKind::DualMediaSdpPublisher, "sdp-publisher");
    graph.addOutputPort(
        videoSource, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    graph.addOutputPort(
        audioSource, "description", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    graph.addInputPort(
        publisher, "video", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    graph.addInputPort(
        publisher, "audio", MediaStreamKind::Metadata,
        MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, true, false);
    const auto policy = MediaGraphBuildSupport::blockingQueuePolicy(2);
    graph.connect(
        videoSource, "description", publisher, "video", "video", policy);
    graph.connect(
        audioSource, "description", publisher, "audio", "audio", policy);
    MediaGraphExecutionContext execution;
    auto compiled = execution.compile(graph);
    if (!compiled) return compiled;
    auto node = MediaDualMediaSdpPublisherNode::create(
        publisher, path.string(),
        std::make_unique<MediaWin32AtomicFileReplacePort>());
    if (!node) return ::media::Status::failure(node.error());
    auto started = node.value()->start(execution);
    if (!started) return started;
    MediaChannel* video = execution.findInputChannel(publisher, "video");
    MediaChannel* audio = execution.findInputChannel(publisher, "audio");
    if (!video || !audio || !video->push(std::move(videoDescription)) ||
        !audio->push(std::move(audioDescription))) {
        node.value()->abort(execution);
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP decode could not queue SDP descriptions"));
    }
    auto processed = node.value()->process(execution);
    if (!processed) {
        node.value()->abort(execution);
        return ::media::Status::failure(processed.error());
    }
    if (processed.value().state != MediaNodeProcessState::Finished) {
        node.value()->abort(execution);
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP decode SDP publisher did not finish"));
    }
    return node.value()->stop(execution);
}

::media::Status sendAccessUnits(
    const ScheduledRtpDecodeSampleFixture& sample,
    ScheduledRtpSenderSession& video,
    ScheduledRtpSenderSession& audio)
{
    const auto started = std::chrono::steady_clock::now();
    for (const auto& unit : sample.accessUnits()) {
        std::this_thread::sleep_until(
            started + std::chrono::nanoseconds(
                unit.dispatchOffset.nanoseconds()));
        ScheduledRtpSenderSession& sender =
            unit.stream == MediaScheduledStream::Video ? video : audio;
        auto sent = sender.sendAccessUnit(
            *unit.packet, unit.presentationOnMaster);
        if (!sent) return sent;
    }
    const MediaRunningTime finalReport = milliseconds(2'000);
    auto videoReport = video.dispatchSenderReport(finalReport);
    if (!videoReport) return ::media::Status::failure(videoReport.error());
    auto audioReport = audio.dispatchSenderReport(finalReport);
    if (!audioReport) return ::media::Status::failure(audioReport.error());
    return ::media::Status::success();
}

} // namespace

int main()
{
#if !defined(_WIN32)
    return 77;
#else
    TemporaryArtifacts artifacts;
    auto portBlock = ScheduledRtpDecodeReceiver::findAvailableIpv4PortBlock();
    if (!portBlock) return failureExit(portBlock.error());
    auto planned = MediaRealtimeRtpTranscodePlanner::plan(
        request(portBlock.value(), artifacts.sdp()));
    if (!planned) return failureExit(planned.error());
    if (!planned.value().avSyncRuntime) {
        return failureExit(::media::ErrorInfo::notInitialized(
            "scheduled RTP decode plan has no A/V sync runtime"));
    }
    auto* output = std::get_if<MediaSeparateRtpOutputRuntimePlan>(
        &planned.value().avSyncRuntime->protocolOutput);
    if (!output) {
        return failureExit(::media::ErrorInfo::notInitialized(
            "scheduled RTP decode plan has no separate RTP output"));
    }
    auto sample = ScheduledRtpDecodeSampleFixture::load(
        samplePath(), output->video.packetization,
        output->audio.packetization);
    if (!sample) return failureExit(sample.error());
    auto ntp = MediaSharedNtpEpoch::create(
        milliseconds(0),
        std::chrono::seconds(1'700'000'000));
    if (!ntp) return failureExit(ntp.error());
    const MediaPlaybackEpoch epoch{
        milliseconds(0), milliseconds(0), 1};
    auto videoPrepared = prepareSender(
        output->video, output->sdp, sample.value().videoCodecContext(),
        ntp.value(), epoch);
    auto audioPrepared = prepareSender(
        output->audio, output->sdp, sample.value().audioCodecContext(),
        ntp.value(), epoch);
    if (!videoPrepared) return failureExit(videoPrepared.error());
    if (!audioPrepared) return failureExit(audioPrepared.error());
    auto published = publishSdp(
        artifacts.sdp(),
        std::move(videoPrepared.value().description),
        std::move(audioPrepared.value().description));
    if (!published) return failureExit(published.error());

    auto receiver = ScheduledRtpDecodeReceiver::start(
        ffmpegPath(), artifacts.sdp(), artifacts.video(), artifacts.audio(),
        artifacts.log());
    if (!receiver) return failureExit(receiver.error());
    const std::array<std::uint16_t, 4> receiverPorts{
        portBlock.value(),
        static_cast<std::uint16_t>(portBlock.value() + 1),
        static_cast<std::uint16_t>(portBlock.value() + 2),
        static_cast<std::uint16_t>(portBlock.value() + 3)};
    auto ready = receiver.value().waitUntilPortsBound(
        receiverPorts, std::chrono::seconds(5));
    if (!ready) {
        std::cerr << ready.error().describe() << '\n'
                  << receiver.value().diagnostics();
        return 1;
    }

    auto socketRuntime = MediaSocketRuntime::create();
    if (!socketRuntime) return failureExit(socketRuntime.error());
    MediaUdpDatagramSenderSocketFactory videoTransportFactory(
        socketRuntime.value());
    MediaUdpDatagramSenderSocketFactory audioTransportFactory(
        socketRuntime.value());
    ScheduledRtpMuxFfmpegSessionFactory videoPacketizerFactory;
    ScheduledRtpMuxFfmpegSessionFactory audioPacketizerFactory;
    auto videoOpened = MediaScheduledRtpOpenTransaction::open(
        std::move(videoPrepared.value().transport),
        std::move(videoPrepared.value().sender),
        videoTransportFactory, videoPacketizerFactory);
    if (!videoOpened) return failureExit(videoOpened.error());
    OpenSenderResources video(std::move(videoOpened).value());
    auto audioOpened = MediaScheduledRtpOpenTransaction::open(
        std::move(audioPrepared.value().transport),
        std::move(audioPrepared.value().sender),
        audioTransportFactory, audioPacketizerFactory);
    if (!audioOpened) return failureExit(audioOpened.error());
    OpenSenderResources audio(std::move(audioOpened).value());
    auto sent = sendAccessUnits(
        sample.value(), video.sender(), audio.sender());
    if (!sent) {
        std::cerr << sent.error().describe() << '\n'
                  << receiver.value().diagnostics();
        return 1;
    }
    auto decoded = receiver.value().waitForSuccess(
        std::chrono::seconds(5));
    if (!decoded) {
        std::cerr << decoded.error().describe() << '\n'
                  << receiver.value().diagnostics();
        return 1;
    }
    const bool videoDecoded = scheduledRtpFrameMd5HasData(
        artifacts.video());
    const bool audioDecoded = scheduledRtpFrameMd5HasData(
        artifacts.audio());
    if (!videoDecoded || !audioDecoded) {
        std::cerr << "generated SDP did not decode both RTP elementary streams\n"
                  << receiver.value().diagnostics();
        return 1;
    }
    std::cout << "generated-SDP decode passed: video framemd5 and audio "
                 "framemd5 contain decoded frames\n";
    return 0;
#endif
}
