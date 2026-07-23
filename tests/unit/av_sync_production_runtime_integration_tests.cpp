#include "unit/fixtures/AvSyncProductionRuntimeIntegrationSupport.h"
#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"

#include "common/AvSyncRuntimeTestSupport.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::FfmpegRealtimeFeeder;
using media_transcode::test::FixedAvSyncClockSource;
using media_transcode::test::PreparedAvFixture;
using media_transcode::test::ScheduledRtpDecodeReceiver;
using media_transcode::test::UdpDatagramReceiver;
using media_transcode::test::findAvailableLoopbackUdpPort;

constexpr int ProductionFixtureVideoWidth = 320;
constexpr int ProductionFixtureVideoHeight = 180;
constexpr std::size_t ProductionFixtureMaximumVideoUnitBytes = 256 * 1024;
constexpr std::size_t ProductionFixtureMaximumAudioUnitBytes = 32 * 1024;
constexpr std::size_t MemoryProbeLimitBytes = 512ULL * 1024ULL * 1024ULL;

class MemoryProbeTracker final {
public:
    ::media::Status sample(std::string_view phase)
    {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(
                GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                sizeof(counters))) {
            return ::media::Status::failure(::media::ErrorInfo::ioFailure(
                "production memory probe sampling failed",
                static_cast<int>(GetLastError())));
        }
        const auto workingSet =
            static_cast<std::size_t>(counters.WorkingSetSize);
        const auto privateBytes =
            static_cast<std::size_t>(counters.PrivateUsage);
        m_peakWorkingSet = (std::max)(m_peakWorkingSet, workingSet);
        m_peakPrivateBytes = (std::max)(m_peakPrivateBytes, privateBytes);
        std::cout << "production memory phase " << phase
                  << ": working set=" << workingSet
                  << " bytes, private=" << privateBytes << " bytes\n";
        return ::media::Status::success();
#else
        (void)phase;
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "production memory probe is Windows-only"));
#endif
    }

    std::size_t peakWorkingSet() const noexcept { return m_peakWorkingSet; }
    std::size_t peakPrivateBytes() const noexcept { return m_peakPrivateBytes; }

private:
    std::size_t m_peakWorkingSet = 0;
    std::size_t m_peakPrivateBytes = 0;
};

class RealtimeTestClock final : public MediaMasterClock {
public:
    RealtimeTestClock() : m_origin(std::chrono::steady_clock::now()) {}

    ::media::Result<MediaRunningTime> now() const noexcept override
    {
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime::fromNanoseconds(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - m_origin).count()));
    }

private:
    std::chrono::steady_clock::time_point m_origin;
};

class TemporaryPath final {
public:
    explicit TemporaryPath(std::string suffix)
        : m_path(std::filesystem::temp_directory_path() /
                 ("av_sync_production_" + std::to_string(
                      std::chrono::steady_clock::now()
                          .time_since_epoch().count()) + std::move(suffix)))
    {
    }

    ~TemporaryPath()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

::media::ErrorInfo integrationError(std::string message)
{
    return ::media::ErrorInfo::internalError(std::move(message));
}

::media::Status validateFixtureVideoPlan(
    const MediaRealtimeRtpTranscodePlan& plan)
{
    if (!plan.videoParameters.width || !plan.videoParameters.height ||
        *plan.videoParameters.width != ProductionFixtureVideoWidth ||
        *plan.videoParameters.height != ProductionFixtureVideoHeight ||
        plan.videoParameters.preset != "ultrafast" ||
        plan.videoParameters.tune != "zerolatency" ||
        plan.queues.frame != 4 ||
        plan.edgePolicies.videoFrame.queuePolicy.capacity != 4 ||
        plan.edgePolicies.audioFrame.queuePolicy.capacity != 4 ||
        !plan.avSyncRuntime ||
        plan.avSyncRuntime->synchronization.startup.maximumVideoUnitBytes !=
            ProductionFixtureMaximumVideoUnitBytes ||
        plan.avSyncRuntime->synchronization.startup.maximumAudioUnitBytes !=
            ProductionFixtureMaximumAudioUnitBytes) {
        return ::media::Status::failure(integrationError(
            "production fixture video request did not reach the planner"));
    }
    return ::media::Status::success();
}

std::filesystem::path sourcePath()
{
    return std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
        "tests/samples/sample_h264_aac_2560x1440.mp4";
}

std::filesystem::path ffmpegPath()
{
#if defined(_WIN32)
    std::wstring module(32'768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length > 0 && length < module.size()) {
        module.resize(length);
        return std::filesystem::path(module).parent_path() / "ffmpeg.exe";
    }
#endif
    return "ffmpeg";
}

MediaRealtimeRtpTranscodeRequest baseRequest(std::string mediaId)
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = std::move(mediaId);
    request.input.openTimeoutMs = 5'000;
    request.input.readTimeoutMs = 5'000;
    request.input.analyzeDurationUs = 500'000;
    request.input.probeSizeBytes = 512 * 1024;
    request.input.lowLatency = true;
    request.output.packetSize = 1'200;
    request.parameters.execution.includeAudio = true;
    request.parameters.execution.disableHardware = true;
    request.parameters.video.codecName = "h264";
    request.parameters.video.width = ProductionFixtureVideoWidth;
    request.parameters.video.height = ProductionFixtureVideoHeight;
    request.parameters.video.frameRate.numerator = 30;
    request.parameters.video.frameRate.denominator = 1;
    request.parameters.video.bitrateKbps = 8'000;
    request.parameters.video.preset = "ultrafast";
    request.parameters.video.tune = "zerolatency";
    request.parameters.audio.codecName = "aac";
    request.parameters.audio.bitrateKbps = 320;
    request.parameters.audio.channels = 2;
    request.parameters.queues.metadata = 8;
    request.parameters.queues.packet = 256;
    request.parameters.queues.frame = 4;
    request.parameters.queues.mux = 256;
    request.avSyncStartup.maximumVideoUnitBytes =
        ProductionFixtureMaximumVideoUnitBytes;
    request.avSyncStartup.maximumAudioUnitBytes =
        ProductionFixtureMaximumAudioUnitBytes;
    request.avSyncStartup.maximumGap =
        MediaRunningTime::fromNanoseconds(40'000'000);
    return request;
}

MediaRealtimeRtpTranscodeRequest rtpRequest(
    std::uint16_t inputBase,
    std::uint16_t outputBase,
    const std::filesystem::path& sdp,
    std::string mediaId)
{
    auto request = baseRequest(std::move(mediaId));
    request.parameters.audio.sampleRate = 44'100;
    request.input.type = RealtimeInputType::RtpPort;
    request.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    request.input.videoRtp.url =
        "rtp://127.0.0.1:" + std::to_string(inputBase);
    request.input.videoRtp.codecName = "h264";
    request.input.videoRtp.payloadType = 96;
    request.input.videoRtp.clockRate = 90'000;
    request.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets="
        "Z0LAHtoFBn58BEAAAAMAQAAADyPFi6g=,aM4PyA==;"
        "profile-level-id=42C01E";
    request.input.audioRtp.url =
        "rtp://127.0.0.1:" + std::to_string(inputBase + 2);
    request.input.audioRtp.codecName = "aac";
    request.input.audioRtp.payloadType = 97;
    request.input.audioRtp.clockRate = 44'100;
    request.input.audioRtp.channels = 2;
    request.input.audioRtp.bitrateKbps = 320;
    request.input.audioRtp.fmtp =
        "profile-level-id=1;mode=AAC-hbr;config=1210;"
        "sizelength=13;indexlength=3;indexdeltalength=3";
    request.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
    request.output.host = "127.0.0.1";
    request.output.basePort = outputBase;
    request.output.sdpPath = sdp.string();
    return request;
}

MediaRealtimeRtpTranscodeRequest tsRequest(
    std::uint16_t inputPort,
    std::uint16_t outputPort,
    std::string mediaId)
{
    auto request = baseRequest(std::move(mediaId));
    request.parameters.audio.sampleRate = 48'000;
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout =
        RealtimeInputStreamLayout::MuxedTransportStream;
    request.input.url =
        "udp://127.0.0.1:" + std::to_string(inputPort) +
        "?fifo_size=1000000&overrun_nonfatal=1";
    request.output.streamLayout =
        RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url =
        "udp://127.0.0.1:" + std::to_string(outputPort);
    request.output.host.clear();
    request.output.basePort.reset();
    request.output.sdpPath.clear();
    return request;
}

::media::Result<std::unique_ptr<MediaGraphRuntime>> startRuntime(
    MediaRealtimeTranscodePreflight preflight,
    MemoryProbeTracker* memoryProbe = nullptr)
{
    const MediaThreadingPolicy threading = preflight.plan.avSyncRuntime
        ? preflight.plan.avSyncRuntime->threadingPolicy
        : MediaThreadingPolicy{};
    auto executable = MediaRealtimeRtpTranscodeGraphBuilder::buildExecutable(
        std::move(preflight));
    if (!executable) {
        return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
            executable.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("graph built"); !sampled) {
            return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
                sampled.error());
        }
    }
    auto clock = std::make_shared<RealtimeTestClock>();
    auto runtime = std::make_unique<MediaGraphRuntime>(
        std::make_shared<FixedAvSyncClockSource>(std::move(clock)));
    runtime->setThreadingPolicy(threading);
    if (auto compiled = runtime->compile(std::move(executable).value());
        !compiled) {
        return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
            compiled.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("runtime compiled"); !sampled) {
            return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
                sampled.error());
        }
    }
    if (auto registered = runtime->registerDefaultRuntimeNodes(); !registered) {
        return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
            registered.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("nodes registered"); !sampled) {
            return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
                sampled.error());
        }
    }
    if (auto started = runtime->startThreaded(); !started) {
        return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
            started.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("threaded started"); !sampled) {
            runtime->abort();
            return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
                sampled.error());
        }
    }
    if (!runtime->threadedRunning()) {
        return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::failure(
            integrationError("production runtime did not enter threaded state"));
    }
    return ::media::Result<std::unique_ptr<MediaGraphRuntime>>::success(
        std::move(runtime));
}

::media::Status stopAndReset(MediaGraphRuntime& runtime)
{
    if (auto synchronized = runtime.synchronizeThreadedState(); !synchronized) {
        return synchronized;
    }
    if (auto stopped = runtime.stop(); !stopped) return stopped;
    if (runtime.state() != MediaGraphRuntimeState::Stopped) {
        return ::media::Status::failure(
            integrationError("production runtime did not stop"));
    }
    runtime.reset();
    return runtime.state() == MediaGraphRuntimeState::Empty
        ? ::media::Status::success()
        : ::media::Status::failure(
              integrationError("production runtime reset did not become empty"));
}

::media::Status abortAndReset(
    MediaGraphRuntime& runtime,
    MemoryProbeTracker* memoryProbe = nullptr)
{
    runtime.abort();
    if (runtime.state() != MediaGraphRuntimeState::Aborted) {
        return ::media::Status::failure(
            integrationError("production runtime did not abort"));
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("abort"); !sampled) {
            return sampled;
        }
    }
    runtime.reset();
    if (runtime.state() != MediaGraphRuntimeState::Empty) {
        return ::media::Status::failure(
            integrationError("aborted production runtime did not reset"));
    }
    return memoryProbe ? memoryProbe->sample("reset")
                       : ::media::Status::success();
}

::media::Status abortAfterFailure(
    MediaGraphRuntime& runtime,
    ::media::ErrorInfo failure)
{
    auto synchronized = runtime.synchronizeThreadedState();
    runtime.abort();
    return synchronized
        ? ::media::Status::failure(std::move(failure))
        : ::media::Status::failure(synchronized.error());
}

::media::Status runMemoryProbe(
    MediaGraphRuntime& runtime,
    MemoryProbeTracker& memoryProbe)
{
#if defined(_WIN32)
    std::size_t peakQueuedBuffers = 0;
    std::size_t peakChannelDepth = 0;
    for (int sample = 0; sample < 20; ++sample) {
        if (auto sampled = memoryProbe.sample(
                "100ms loop " + std::to_string(sample + 1)); !sampled) {
            if (auto reset = abortAndReset(runtime, &memoryProbe); !reset) {
                return reset;
            }
            return sampled;
        }
        std::size_t queuedBuffers = 0;
        for (const MediaChannel* channel :
             runtime.context().channels().channels()) {
            queuedBuffers += channel->size();
            peakChannelDepth = (std::max)(
                peakChannelDepth,
                channel->metrics().queue.peakSize.load());
        }
        peakQueuedBuffers = (std::max)(peakQueuedBuffers, queuedBuffers);
        if (memoryProbe.peakWorkingSet() > MemoryProbeLimitBytes) {
            if (auto reset = abortAndReset(runtime, &memoryProbe); !reset) {
                return reset;
            }
            return ::media::Status::failure(integrationError(
                "production memory probe exceeded 512 MiB"));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "production memory probe peak working set: "
              << memoryProbe.peakWorkingSet()
              << " bytes, peak private bytes: "
              << memoryProbe.peakPrivateBytes()
              << ", peak queued buffers: "
              << peakQueuedBuffers << ", peak channel depth: "
              << peakChannelDepth << '\n';
    auto synchronized = runtime.synchronizeThreadedState();
    auto reset = abortAndReset(runtime, &memoryProbe);
    if (!reset) return reset;
    return synchronized;
#else
    (void)runtime;
    return ::media::Status::failure(
        ::media::ErrorInfo::unsupported(
            "production memory probe is Windows-only"));
#endif
}

::media::Status runRtpLifecycle(
    const std::filesystem::path& fixturePath,
    bool abortPath,
    MemoryProbeTracker* memoryProbe = nullptr)
{
    auto outputBase = ScheduledRtpDecodeReceiver::findAvailableIpv4PortBlock();
    if (!outputBase) return ::media::Status::failure(outputBase.error());
    TemporaryPath outputSdp(abortPath ? "_rtp_abort.sdp"
                                      : "_rtp_stop.sdp");
    auto videoReceiver = UdpDatagramReceiver::bindLoopback(outputBase.value());
    auto audioReceiver = UdpDatagramReceiver::bindLoopback(
        static_cast<std::uint16_t>(outputBase.value() + 2));
    if (!videoReceiver || !audioReceiver) {
        return ::media::Status::failure(
            !videoReceiver ? videoReceiver.error() : audioReceiver.error());
    }
    auto inputBase = ScheduledRtpDecodeReceiver::findAvailableIpv4PortBlock();
    if (!inputBase) return ::media::Status::failure(inputBase.error());

    auto request = rtpRequest(
        inputBase.value(), outputBase.value(), outputSdp.path(),
        abortPath ? "production-rtp-abort" : "production-rtp-stop");
    auto productionPlan = MediaRealtimeRtpTranscodePlanner::plan(request);
    if (!productionPlan) {
        return ::media::Status::failure(productionPlan.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("planner done"); !sampled) {
            return sampled;
        }
    }
    if (auto validated = validateFixtureVideoPlan(productionPlan.value());
        !validated) {
        return validated;
    }
    MediaRealtimeTranscodePreflight production{
        std::move(productionPlan).value(), std::nullopt};
    auto runtime = startRuntime(std::move(production), memoryProbe);
    if (!runtime) return ::media::Status::failure(runtime.error());
    auto feeder = FfmpegRealtimeFeeder::startSeparateRtp(
        ffmpegPath(), fixturePath, inputBase.value(),
        static_cast<std::uint16_t>(inputBase.value() + 2),
        "av-sync-production-input");
    if (!feeder) {
        runtime.value()->abort();
        return ::media::Status::failure(feeder.error());
    }
    if (memoryProbe) {
        if (auto sampled = memoryProbe->sample("feeder started"); !sampled) {
            runtime.value()->abort();
            return sampled;
        }
        return runMemoryProbe(*runtime.value(), *memoryProbe);
    }
    auto videoBytes = videoReceiver.value().receiveBytes(
        std::chrono::seconds(15));
    auto audioBytes = audioReceiver.value().receiveBytes(
        std::chrono::seconds(15));
    if (!videoBytes || !audioBytes || videoBytes.value() == 0 ||
        audioBytes.value() == 0) {
        return abortAfterFailure(
            *runtime.value(),
            !videoBytes ? videoBytes.error()
            : !audioBytes ? audioBytes.error()
                          : integrationError("RTP adapters emitted empty payloads"));
    }
    // Keep the live graph running across at least two FFmpeg RTCP sender-report
    // refresh intervals. Lifecycle completion must surface any worker failure
    // caused by periodic clock evidence, not only prove first-packet output.
    std::this_thread::sleep_for(std::chrono::seconds(12));
    return abortPath ? abortAndReset(*runtime.value())
                     : stopAndReset(*runtime.value());
}

::media::Status runTsLifecycle(
    const std::filesystem::path& fixturePath,
    bool abortPath)
{
    auto outputPort = findAvailableLoopbackUdpPort();
    if (!outputPort) return ::media::Status::failure(outputPort.error());
    auto receiver = UdpDatagramReceiver::bindLoopback(outputPort.value());
    if (!receiver) return ::media::Status::failure(receiver.error());
    auto inputPort = findAvailableLoopbackUdpPort();
    if (!inputPort) return ::media::Status::failure(inputPort.error());
    auto request = tsRequest(
        inputPort.value(), outputPort.value(),
        abortPath ? "production-ts-abort" : "production-ts-stop");
    auto feeder = FfmpegRealtimeFeeder::startMpegTs(
        ffmpegPath(), fixturePath, inputPort.value());
    if (!feeder) return ::media::Status::failure(feeder.error());
    auto preflight = MediaRealtimeRtpTranscodePlanner::preflight(request);
    if (!preflight) return ::media::Status::failure(preflight.error());
    if (auto validated = validateFixtureVideoPlan(preflight.value().plan);
        !validated) {
        return validated;
    }
    auto runtime = startRuntime(std::move(preflight).value());
    if (!runtime) return ::media::Status::failure(runtime.error());
    auto bytes = receiver.value().receiveBytes(std::chrono::seconds(15));
    if (!bytes || bytes.value() == 0) {
        return abortAfterFailure(
            *runtime.value(),
            bytes ? integrationError("TS adapter emitted an empty datagram")
                  : bytes.error());
    }
    return abortPath ? abortAndReset(*runtime.value())
                     : stopAndReset(*runtime.value());
}

int fail(const char* phase, const ::media::ErrorInfo& error)
{
    std::cerr << phase << " failed: " << error.describe() << '\n';
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    const bool rtpStopOnly = argc == 2 &&
        std::string_view(argv[1]) == "--rtp-stop";
    const bool memoryProbeOnly = argc == 2 &&
        std::string_view(argv[1]) == "--memory-probe";
    if (argc > 1 && !rtpStopOnly && !memoryProbeOnly) {
        std::cerr << "unsupported production runtime integration mode\n";
        return 2;
    }
    MemoryProbeTracker memoryProbe;
    if (memoryProbeOnly) {
        if (auto sampled = memoryProbe.sample("process entry"); !sampled) {
            return fail("production memory probe", sampled.error());
        }
    }
    if (!std::filesystem::is_regular_file(sourcePath()) ||
        !std::filesystem::is_regular_file(ffmpegPath())) {
        std::cerr << "production runtime integration prerequisites unavailable\n";
        return 77;
    }
    if (auto status = ScheduledRtpDecodeReceiver::preflightPlatformApis();
        !status) {
        std::cerr << status.error().describe() << '\n';
        return 77;
    }
    auto fixtureRequest = baseRequest("production-fixture");
    if (!fixtureRequest.parameters.video.width ||
        !fixtureRequest.parameters.video.height) {
        std::cerr << "production fixture dimensions unavailable\n";
        return 1;
    }
    auto fixture = PreparedAvFixture::create(
        ffmpegPath(), sourcePath(), *fixtureRequest.parameters.video.width,
        *fixtureRequest.parameters.video.height);
    if (!fixture) return fail("production fixture preparation", fixture.error());
    if (memoryProbeOnly) {
        if (auto sampled = memoryProbe.sample("fixture prepared"); !sampled) {
            return fail("production memory probe", sampled.error());
        }
        if (auto status = runRtpLifecycle(
                fixture.value().path(), true, &memoryProbe); !status) {
            return fail("production memory probe", status.error());
        }
        return 0;
    }
    if (auto status = runRtpLifecycle(fixture.value().path(), false); !status) {
        return fail("separate RTP production stop/reset", status.error());
    }
    if (rtpStopOnly) {
        std::cout << "A/V sync production RTP stop/reset passed\n";
        return 0;
    }
    if (auto status = runRtpLifecycle(fixture.value().path(), true); !status) {
        return fail("separate RTP production abort/reset", status.error());
    }
    if (auto status = runTsLifecycle(fixture.value().path(), false); !status) {
        return fail("MPEG-TS production stop/reset", status.error());
    }
    if (auto status = runTsLifecycle(fixture.value().path(), true); !status) {
        return fail("MPEG-TS production abort/reset", status.error());
    }
    std::cout << "A/V sync production runtime integration passed: full "
                 "builder executable, threaded lifecycle, scheduler activation, "
                 "and RTP/TS protocol outputs\n";
    return 0;
}
