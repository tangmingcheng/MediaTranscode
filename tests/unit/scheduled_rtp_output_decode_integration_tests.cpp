#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"
#include "unit/fixtures/ScheduledRtpDecodeSampleFixture.h"
#include "unit/fixtures/ScheduledRtpOutputIntegrationRuntime.h"

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::ScheduledRtpDecodeReceiver;
using media_transcode::test::ScheduledRtpDecodeSampleFixture;
using media_transcode::test::ScheduledRtpOutputIntegrationRuntime;
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
        m_dependency = directory /
            ("scheduled_rtp_decode_dependency_" + suffix + ".exe");
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
        std::filesystem::remove(m_dependency, ignored);
    }

    const std::filesystem::path& sdp() const noexcept { return m_sdp; }
    const std::filesystem::path& video() const noexcept { return m_video; }
    const std::filesystem::path& audio() const noexcept { return m_audio; }
    const std::filesystem::path& log() const noexcept { return m_log; }
    const std::filesystem::path& dependency() const noexcept
    {
        return m_dependency;
    }

private:
    std::filesystem::path m_sdp;
    std::filesystem::path m_video;
    std::filesystem::path m_audio;
    std::filesystem::path m_log;
    std::filesystem::path m_dependency;
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

::media::Status verifyReceiverInputClassification(
    const TemporaryArtifacts& artifacts)
{
    {
        std::ofstream dependency(artifacts.dependency(), std::ios::binary);
        dependency << "classification-only";
        if (!dependency) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "scheduled RTP decode could not create classification dependency",
                -1));
        }
    }
    const auto missingExecutable =
        ScheduledRtpDecodeReceiver::preflightExecutable(
            artifacts.dependency().string() + ".missing");
    if (missingExecutable ||
        missingExecutable.error().code != ::media::ErrorCode::Unsupported) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "missing receiver executable must be an unsupported prerequisite"));
    }
    const auto availableExecutable =
        ScheduledRtpDecodeReceiver::preflightExecutable(
            artifacts.dependency());
    if (!availableExecutable) return availableExecutable;
    const auto missingGeneratedSdp =
        ScheduledRtpDecodeReceiver::validateGeneratedSdp(artifacts.sdp());
    if (missingGeneratedSdp ||
        missingGeneratedSdp.error().code != ::media::ErrorCode::IoFailure) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "missing generated SDP must be a test I/O failure"));
    }
    return ::media::Status::success();
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

} // namespace

int main()
{
#if !defined(_WIN32)
    return 77;
#else
    TemporaryArtifacts artifacts;
    auto classified = verifyReceiverInputClassification(artifacts);
    if (!classified) return failureExit(classified.error());
    auto receiverDependency =
        ScheduledRtpDecodeReceiver::preflightExecutable(ffmpegPath());
    if (!receiverDependency) return failureExit(receiverDependency.error());
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
    auto senders = ScheduledRtpOutputIntegrationRuntime::openSendersAndPublish(
        *planned.value().avSyncRuntime,
        sample.value().videoCodecContext(),
        sample.value().audioCodecContext());
    if (!senders) return failureExit(senders.error());

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

    auto sent = senders.value().sendAccessUnits(sample.value());
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
