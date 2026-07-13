#include "common/TestAssert.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"
#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/network/MediaSocketRuntime.h"
#include "internal/graph/runtime/network/MediaUdpSocket.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <chrono>
#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#else
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

std::filesystem::path integrationFfmpegPath()
{
#ifdef _WIN32
    std::vector<wchar_t> modulePath(32768);
    const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(),
                                            static_cast<DWORD>(modulePath.size()));
    if (length > 0 && length < modulePath.size()) {
        const auto sibling = std::filesystem::path(
            std::wstring(modulePath.data(), length)).parent_path() / "ffmpeg.exe";
        if (std::filesystem::exists(sibling)) return sibling;
    }
#endif
    return "ffmpeg";
}

std::string integrationSamplePath()
{
    return (std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) / "tests" / "samples" /
            "sample_h264_aac_2560x1440.mp4").string();
}

std::uint16_t integrationBasePort() noexcept
{
#ifdef _WIN32
    const auto process = static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    const auto process = static_cast<std::uint32_t>(getpid());
#endif
    return static_cast<std::uint16_t>(46'000 + (process % 1'000) * 8);
}

class FfmpegMpegTsUdpSender final {
public:
    static ::media::Result<FfmpegMpegTsUdpSender> start(std::uint16_t port)
    {
        const std::string destination =
            "udp://127.0.0.1:" + std::to_string(port) + "?pkt_size=1316";
#ifdef _WIN32
        const std::string command =
            "\"" + integrationFfmpegPath().string() +
            "\" -hide_banner -loglevel error -re -stream_loop -1 -i \"" +
            integrationSamplePath() +
            "\" -map 0:v:0 -map 0:a:0 -c copy -f mpegts \"" + destination + "\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::wstring commandLine(command.begin(), command.end());
        if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
            return ::media::Result<FfmpegMpegTsUdpSender>::failure(
                ::media::ErrorInfo::ioFailure("Failed to start FFmpeg MPEG-TS UDP sender",
                                              static_cast<int>(GetLastError())));
        }
        CloseHandle(process.hThread);
        FfmpegMpegTsUdpSender sender(process.hProcess, process.dwProcessId);
#else
        const pid_t process = fork();
        if (process < 0) {
            return ::media::Result<FfmpegMpegTsUdpSender>::failure(
                ::media::ErrorInfo::ioFailure("Failed to fork FFmpeg MPEG-TS UDP sender"));
        }
        if (process == 0) {
            execlp(integrationFfmpegPath().string().c_str(), "ffmpeg", "-hide_banner",
                   "-loglevel", "error", "-re", "-stream_loop", "-1", "-i",
                   integrationSamplePath().c_str(), "-map", "0:v:0", "-map", "0:a:0",
                   "-c", "copy", "-f", "mpegts", destination.c_str(),
                   static_cast<char*>(nullptr));
            _exit(127);
        }
        FfmpegMpegTsUdpSender sender(process);
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(1'000));
        return ::media::Result<FfmpegMpegTsUdpSender>::success(std::move(sender));
    }

    FfmpegMpegTsUdpSender(FfmpegMpegTsUdpSender&& other) noexcept
        : m_process(other.m_process), m_processId(other.m_processId)
    {
        other.clear();
    }
    FfmpegMpegTsUdpSender& operator=(FfmpegMpegTsUdpSender&& other) noexcept
    {
        if (this != &other) {
            stopProcess();
            m_process = other.m_process;
            m_processId = other.m_processId;
            other.clear();
        }
        return *this;
    }
    FfmpegMpegTsUdpSender(const FfmpegMpegTsUdpSender&) = delete;
    FfmpegMpegTsUdpSender& operator=(const FfmpegMpegTsUdpSender&) = delete;
    ~FfmpegMpegTsUdpSender() { stopProcess(); }
    void stop() noexcept { stopProcess(); }
    bool pause() noexcept
    {
#ifdef _WIN32
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        bool paused = false;
        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID != m_processId) continue;
                const HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE,
                                                 entry.th32ThreadID);
                if (!thread) continue;
                paused = SuspendThread(thread) != static_cast<DWORD>(-1) || paused;
                CloseHandle(thread);
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return paused;
#else
        return m_process > 0 && kill(m_process, SIGSTOP) == 0;
#endif
    }

private:
#ifdef _WIN32
    explicit FfmpegMpegTsUdpSender(HANDLE process, DWORD processId)
        : m_process(process), m_processId(processId) {}
    void clear() noexcept { m_process = nullptr; m_processId = 0; }
    void stopProcess() noexcept
    {
        if (!m_process) return;
        TerminateProcess(m_process, 0);
        WaitForSingleObject(m_process, 2'000);
        CloseHandle(m_process);
        m_process = nullptr;
        m_processId = 0;
    }
    HANDLE m_process = nullptr;
    DWORD m_processId = 0;
#else
    explicit FfmpegMpegTsUdpSender(pid_t process) : m_process(process) {}
    void clear() noexcept { m_process = -1; m_processId = 0; }
    void stopProcess() noexcept
    {
        if (m_process <= 0) return;
        kill(m_process, SIGCONT);
        kill(m_process, SIGTERM);
        waitpid(m_process, nullptr, 0);
        m_process = -1;
    }
    pid_t m_process = -1;
    std::uint32_t m_processId = 0;
#endif
};

MediaRealtimeRtpTranscodeRequest integrationRequest(std::uint16_t inputPort,
                                                     std::uint16_t outputPort)
{
    MediaRealtimeRtpTranscodeRequest request;
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.input.url = "udp://127.0.0.1:" + std::to_string(inputPort) +
                        "?fifo_size=1000000&overrun_nonfatal=1";
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 500;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = "udp://127.0.0.1:" + std::to_string(outputPort) + "?pkt_size=1316";
    request.output.packetSize = 1'316;
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.queues.metadata = 1;
    request.parameters.queues.packet = 256;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = MediaRunningTime::fromNanoseconds(40'000'000);
    request.parameters.queues.frame = 128;
    request.parameters.queues.mux = 256;
    request.parameters.video.codecName = "h264";
    request.parameters.video.bitrateKbps = 8'406;
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.audio.channels = 2;
    return request;
}

std::optional<std::uint64_t> timestamp33(std::int64_t value)
{
    if (value == AV_NOPTS_VALUE) return std::nullopt;
    constexpr std::int64_t modulus = std::int64_t{1} << 33;
    const auto normalized = value % modulus;
    return static_cast<std::uint64_t>(normalized < 0 ? normalized + modulus : normalized);
}

void testProductionUdpSessionPublishesMappedProgramClock(TestContext& ctx,
                                                         std::uint16_t inputPort,
                                                         std::uint16_t outputPort)
{
    auto sender = FfmpegMpegTsUdpSender::start(inputPort);
    EXPECT_TRUE(ctx, sender);
    if (!sender) return;
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(
        integrationRequest(inputPort, outputPort));
    if (!preflight) std::cerr << "[mpeg-ts-integration] preflight error: "
                              << preflight.error().message << '\n';
    EXPECT_TRUE(ctx, preflight);
    if (!preflight) return;
    EXPECT_TRUE(ctx, preflight.value().plan.input.mpegTs.has_value());
    EXPECT_TRUE(ctx, preflight.value().prepared.has_value());
    if (!preflight.value().plan.input.mpegTs || !preflight.value().prepared) return;
    const auto plan = *preflight.value().plan.input.mpegTs;

    auto socketRuntime = MediaSocketRuntime::create();
    EXPECT_TRUE(ctx, socketRuntime);
    if (socketRuntime) {
        auto duplicate = MediaUdpSocket::bind(socketRuntime.value(), MediaUdpSocketConfig{
            MediaIpAddressFamily::Ipv4, "127.0.0.1", inputPort, 262'144});
        EXPECT_FALSE(ctx, duplicate);
    }

    auto released = preflight.value().prepared->releaseBuffer();
    EXPECT_TRUE(ctx, released);
    if (!released) return;
    auto* prepared = dynamic_cast<MediaTsPreparedInputBuffer*>(released.value().get());
    EXPECT_TRUE(ctx, prepared != nullptr);
    if (!prepared) return;
    auto taken = prepared->takeSession();
    EXPECT_TRUE(ctx, taken);
    if (!taken) return;
    auto session = std::move(taken).value();
    const auto runtimeContract = session->runtimeContract();
    EXPECT_EQ(ctx, runtimeContract.pesProvenanceCapacity, plan.pesProvenanceCapacity);

    MediaTsProgramClockPolicy policy{
        static_cast<std::uint16_t>(plan.programNumber),
        static_cast<std::uint16_t>(plan.programMapPid),
        static_cast<std::uint16_t>(plan.pcrPid),
        static_cast<std::uint16_t>(plan.videoPid),
        static_cast<std::uint16_t>(plan.audioPid),
        plan.pcrInterval27Mhz,
        plan.maximumPcrJitter27Mhz,
        plan.maximumPcrGap27Mhz};
    auto projection = MediaTsClockProjection::create(
        policy, plan.projectionCapacity, plan.maximumPacketPositionRegressionBytes,
        plan.initialSourceGeneration, plan.initialRawTransportGeneration);
    EXPECT_TRUE(ctx, projection);
    if (!projection) return;
    auto evidence = session->evidenceSnapshotAfter(std::nullopt);
    EXPECT_TRUE(ctx, evidence);
    if (!evidence) return;
    EXPECT_TRUE(ctx, std::any_of(evidence.value().begin(), evidence.value().end(),
        [](const MediaTsEvidenceCheckpoint& item) { return item.pcrObservation.has_value(); }));
    EXPECT_TRUE(ctx, projection.value().replay(evidence.value()));

    std::map<int, std::map<std::uint64_t, MediaTsSourceClockMapper>> clocks;
    bool sawVideo = false;
    bool sawAudio = false;
    bool sawMappedVideo = false;
    bool sawMappedAudio = false;
    bool sawPositionedVideo = false;
    bool sawPositionedAudio = false;
    bool sawCarriedVideo = false;
    bool sawCarriedAudio = false;
    bool sawAcquiring = false;
    bool sawLockedAfterAcquiring = false;
    std::size_t frameCount = 0;
    std::array<std::size_t, 4> positionCounts{};
    std::uint64_t maximumObservedByteOffset = 0;
    std::map<int, std::int64_t> lastPresentationNs;
    std::map<int, std::int64_t> lastDecodeNs;
    for (int attempt = 0; attempt < 1'000; ++attempt) {
        auto read = session->readFrame();
        EXPECT_TRUE(ctx, read);
        if (!read) break;
        auto envelope = std::move(read).value();
        if (envelope.state == MediaTsReadFrameState::Waiting) continue;
        EXPECT_EQ(ctx, envelope.state, MediaTsReadFrameState::Frame);
        if (envelope.state != MediaTsReadFrameState::Frame || !envelope.packet) break;
        auto packet = std::move(envelope.packet);
        ++frameCount;

        int pid = -1;
        for (const auto& program : session->programSnapshots()) {
            if (program.programNumber != plan.programNumber) continue;
            for (const auto& binding : program.streamBindings) {
                if (binding.streamIndex == packet->stream_index) pid = binding.elementaryPid;
            }
        }
        EXPECT_TRUE(ctx, pid == plan.videoPid || pid == plan.audioPid);
        if (pid == plan.videoPid) sawVideo = true;
        if (pid == plan.audioPid) sawAudio = true;
        if (packet->pos >= 0 && pid == plan.videoPid) { sawPositionedVideo = true; ++positionCounts[0]; }
        if (packet->pos >= 0 && pid == plan.audioPid) { sawPositionedAudio = true; ++positionCounts[1]; }
        if (packet->pos < 0 && pid == plan.videoPid) { sawCarriedVideo = true; ++positionCounts[2]; }
        if (packet->pos < 0 && pid == plan.audioPid) { sawCarriedAudio = true; ++positionCounts[3]; }
        if (envelope.provenance.readiness == MediaSourceClockReadiness::Acquiring) {
            sawAcquiring = true;
            EXPECT_FALSE(ctx, envelope.provenance.originByteOffset.has_value());
            continue;
        }
        if (sawAcquiring && envelope.provenance.readiness == MediaSourceClockReadiness::Locked) {
            sawLockedAfterAcquiring = true;
        }
        if (envelope.provenance.readiness != MediaSourceClockReadiness::Locked) continue;
        EXPECT_TRUE(ctx, envelope.provenance.originByteOffset.has_value());
        if (!envelope.provenance.originByteOffset) break;
        const auto position = *envelope.provenance.originByteOffset;
        maximumObservedByteOffset = std::max(maximumObservedByteOffset, position);
        EXPECT_TRUE(ctx, session->observePacketPosition(position));
        EXPECT_TRUE(ctx, projection.value().observePacketPosition(position));
        auto incremental = session->evidenceSnapshotAfter(projection.value().lastReplayedOffset());
        EXPECT_TRUE(ctx, incremental);
        if (!incremental) break;
        for (const auto& checkpoint : incremental.value()) {
            maximumObservedByteOffset = std::max(maximumObservedByteOffset,
                                                 checkpoint.byteOffset);
        }
        EXPECT_TRUE(ctx, projection.value().replay(incremental.value()));
        auto checkpoint = projection.value().atOrBefore(position);
        EXPECT_TRUE(ctx, checkpoint);
        if (!checkpoint) break;
        EXPECT_EQ(ctx, envelope.provenance.readiness, checkpoint.value().readiness);
        if (checkpoint.value().readiness != MediaSourceClockReadiness::Locked) continue;
        auto& generations = clocks[packet->stream_index];
        auto mapper = generations.find(checkpoint.value().generation);
        if (mapper == generations.end()) {
            auto created = MediaTsSourceClockMapper::create(checkpoint.value().calibration);
            EXPECT_TRUE(ctx, created);
            if (!created) break;
            mapper = generations.emplace(checkpoint.value().generation,
                                         std::move(created).value()).first;
        }
        auto mapped = mapper->second.map(timestamp33(packet->pts), timestamp33(packet->dts));
        EXPECT_TRUE(ctx, mapped);
        if (!mapped) break;
        const bool hasMappedTimes = mapped.value().presentationTime.has_value() &&
                                    mapped.value().decodeTime.has_value();
        if (mapped.value().presentationTime) {
            const auto value = mapped.value().presentationTime->nanoseconds();
            if (const auto previous = lastPresentationNs.find(packet->stream_index);
                previous != lastPresentationNs.end()) {
                EXPECT_TRUE(ctx, value >= previous->second);
            }
            lastPresentationNs[packet->stream_index] = value;
        }
        if (mapped.value().decodeTime) {
            const auto value = mapped.value().decodeTime->nanoseconds();
            if (const auto previous = lastDecodeNs.find(packet->stream_index);
                previous != lastDecodeNs.end()) {
                EXPECT_TRUE(ctx, value >= previous->second);
            }
            lastDecodeNs[packet->stream_index] = value;
        }
        if (pid == plan.videoPid) sawMappedVideo = sawMappedVideo || hasMappedTimes;
        if (pid == plan.audioPid) sawMappedAudio = sawMappedAudio || hasMappedTimes;
        const bool complete = frameCount >= 200 && sawMappedVideo && sawMappedAudio &&
                              sawPositionedVideo && sawPositionedAudio &&
                              (sawCarriedVideo || sawCarriedAudio) &&
                              maximumObservedByteOffset / plan.packetSize >= 1'000;
        if (complete) break;
    }
    EXPECT_TRUE(ctx, sawVideo && sawAudio);
    EXPECT_TRUE(ctx, sawMappedVideo && sawMappedAudio);
    std::cerr << "[mpeg-ts-integration] frames=" << frameCount
              << " video_positioned=" << positionCounts[0]
              << " audio_positioned=" << positionCounts[1]
              << " video_carried=" << positionCounts[2]
              << " audio_carried=" << positionCounts[3]
              << " raw_packets=" << maximumObservedByteOffset / plan.packetSize
              << " acquiring=" << sawAcquiring << '\n';
    EXPECT_TRUE(ctx, sawPositionedVideo && sawPositionedAudio);
    EXPECT_TRUE(ctx, sawCarriedVideo || sawCarriedAudio);
    EXPECT_TRUE(ctx, maximumObservedByteOffset / plan.packetSize >= 1'000);
    EXPECT_TRUE(ctx, frameCount >= 200);
    if (sawAcquiring) EXPECT_TRUE(ctx, sawLockedAfterAcquiring);
    EXPECT_TRUE(ctx, session->close());
}

void testProductionPlannerBuilderRuntimeLifecycle(TestContext& ctx,
                                                  std::uint16_t inputPort,
                                                  std::uint16_t outputPort)
{
    auto sender = FfmpegMpegTsUdpSender::start(inputPort);
    EXPECT_TRUE(ctx, sender);
    if (!sender) return;
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(
        integrationRequest(inputPort, outputPort));
    EXPECT_TRUE(ctx, preflight);
    if (!preflight) return;
    auto executable = MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
        std::move(preflight).value());
    EXPECT_TRUE(ctx, executable);
    if (!executable) return;
    MediaGraphRuntime runtime;
    const auto validation = MediaGraphValidation::validate(executable.value().graph);
    for (const auto& issue : validation.issues) {
        if (issue.severity == MediaGraphValidationSeverity::Error) {
            std::cerr << "MPEG-TS executable validation: " << issue.message << '\n';
        }
    }
    EXPECT_TRUE(ctx, runtime.compile(std::move(executable).value()));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    EXPECT_TRUE(ctx, runtime.startThreaded());
    if (!runtime.threadedRunning()) return;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(ctx, runtime.synchronizeThreadedState());
    const auto metrics = runtime.threadedExecutor().metrics();
    EXPECT_TRUE(ctx, metrics.workerProgress > 0);
    EXPECT_EQ(ctx, metrics.workerErrors, std::uint64_t{0});
    EXPECT_TRUE(ctx, runtime.stop());
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Stopped);
    runtime.reset();
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Empty);

    sender.value().stop();
    auto restartSender = FfmpegMpegTsUdpSender::start(inputPort);
    EXPECT_TRUE(ctx, restartSender);
    if (!restartSender) return;
    auto restartPreflight = MediaRealtimeRtpTranscodePlanner::preflight(
        integrationRequest(inputPort, outputPort));
    EXPECT_TRUE(ctx, restartPreflight);
    if (!restartPreflight) return;
    auto restartExecutable = MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
        std::move(restartPreflight).value());
    EXPECT_TRUE(ctx, restartExecutable);
    if (!restartExecutable) return;
    EXPECT_TRUE(ctx, runtime.compile(std::move(restartExecutable).value()));
    EXPECT_TRUE(ctx, runtime.registerDefaultRuntimeNodes());
    EXPECT_TRUE(ctx, restartSender.value().pause());
    EXPECT_TRUE(ctx, runtime.startThreaded());
    if (!runtime.threadedRunning()) return;

    bool observedIdleWait = false;
    for (int attempt = 0; attempt < 20 && !observedIdleWait; ++attempt) {
        const auto before = runtime.threadedExecutor().metrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        const auto after = runtime.threadedExecutor().metrics();
        observedIdleWait = before.workerProcessCalls > 0 &&
            before.workerWaits > 0 &&
            before.workerProcessCalls == after.workerProcessCalls &&
            before.workerProgress == after.workerProgress &&
            before.workerWaits == after.workerWaits &&
            before.workerWakeups == after.workerWakeups;
    }
    EXPECT_TRUE(ctx, observedIdleWait);
    EXPECT_EQ(ctx, runtime.threadedExecutor().metrics().workerErrors, std::uint64_t{0});
    runtime.abort();
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Aborted);
    runtime.reset();
    EXPECT_EQ(ctx, runtime.state(), MediaGraphRuntimeState::Empty);
}

void testProductionInvalidProvenanceBindingFailsClosed(TestContext& ctx,
                                                       std::uint16_t inputPort,
                                                       std::uint16_t outputPort)
{
    auto sender = FfmpegMpegTsUdpSender::start(inputPort);
    EXPECT_TRUE(ctx, sender);
    if (!sender) return;
    MediaRealtimePreflightIo io;
    io.openMpegTs = [&ctx](const MediaTsInputSessionOptions& options)
        -> ::media::Result<std::unique_ptr<MediaTsInputSession>> {
        auto invalid = options;
        invalid.pesProvenanceCapacity = 0;
        auto session = MediaTsInputSession::open(invalid);
        EXPECT_FALSE(ctx, session);
        return session;
    };
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(
        integrationRequest(inputPort, outputPort), io);
    EXPECT_FALSE(ctx, preflight);
}

} // namespace

int main()
{
    TestContext ctx;
    const auto basePort = integrationBasePort();
    testProductionUdpSessionPublishesMappedProgramClock(ctx, basePort,
                                                        static_cast<std::uint16_t>(basePort + 1));
    testProductionPlannerBuilderRuntimeLifecycle(
        ctx, static_cast<std::uint16_t>(basePort + 2),
        static_cast<std::uint16_t>(basePort + 3));
    testProductionInvalidProvenanceBindingFailsClosed(
        ctx, static_cast<std::uint16_t>(basePort + 4),
        static_cast<std::uint16_t>(basePort + 5));
    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " MPEG-TS integration expectation(s) failed\n";
        return 1;
    }
    std::cout << "MPEG-TS observed AVIO integration tests passed\n";
    return 0;
}
