#include "unit/fixtures/MpegTsOutputArtifactVerifier.h"
#include "unit/fixtures/ScheduledMpegTsDecodeSamplePreparer.h"
#include "unit/fixtures/ScheduledMpegTsDecodeSampleFixture.h"
#include "unit/fixtures/ScheduledMpegTsOutputIntegrationRuntime.h"
#include "unit/fixtures/ScheduledRtpDecodePrerequisites.h"
#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/audio/MediaResolvedAudioOutputPlan.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanner.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsResolvedPipelineFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

using namespace media::ffmpeg::graph;
using media_transcode::test::MpegTsOutputArtifactVerifier;
using media_transcode::test::ScheduledMpegTsDecodeSamplePreparer;
using media_transcode::test::ScheduledMpegTsDecodeSampleFixture;
using media_transcode::test::ScheduledMpegTsOutputIntegrationRuntime;
using media_transcode::test::ScheduledRtpDecodePrerequisites;
using media_transcode::test::ScheduledRtpDecodeReceiver;

constexpr MediaRunningTime milliseconds(std::int64_t value) noexcept
{
    return MediaRunningTime::fromNanoseconds(value * 1'000'000);
}

class TemporaryOutput final {
public:
    TemporaryOutput()
        : m_path(std::filesystem::temp_directory_path() /
                 ("scheduled_project_mpeg_ts_" + std::to_string(
                     std::chrono::steady_clock::now()
                         .time_since_epoch().count()) + ".ts"))
    {
    }

    ~TemporaryOutput()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    const std::filesystem::path& path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

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
        return std::filesystem::path(std::wstring(module.data(), length))
            .parent_path() / "ffmpeg.exe";
    }
#endif
    return "ffmpeg";
}

::media::Status verifyExitClassification()
{
    auto missingExecutable = ScheduledRtpDecodeReceiver::preflightExecutable(
        ffmpegPath().string() + ".missing");
    if (missingExecutable ||
        ScheduledRtpDecodePrerequisites::externalFailureExit(
            missingExecutable.error()) != 77) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "missing external FFmpeg must map to prerequisite skip 77"));
    }
    const auto injected = ::media::ErrorInfo::unsupported(
        "injected post-preflight planner failure");
    if (ScheduledRtpDecodePrerequisites::pipelineFailureExit(injected) != 1) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "post-preflight Unsupported must map to test failure 1"));
    }
    return ::media::Status::success();
}

MediaTsSelectedProgramPlan selectedTsProgram()
{
    MediaTsSelectedProgramPlan selected{1, 0x100, 0x101, 0x102, 0x101};
    selected.videoPacketDuration = MediaTsPacketDurationEvidence{
        0, 0x101, 3'000, {1, 90'000}};
    selected.audioPacketDuration = MediaTsPacketDurationEvidence{
        1, 0x102, 1'024, {1, 48'000}};
    return selected;
}

::media::Result<MediaProjectMpegTsResolvedPipelineFacts>
resolvedPipelineFacts()
{
    const MediaResolvedAudioSource source{
        "aac", MediaAudioProfile::knownAacLow(), 48'000, 2,
        "stereo", "fltp", 128'000};
    const MediaResolvedAudioRequest request;
    auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    if (!target) {
        return ::media::Result<MediaProjectMpegTsResolvedPipelineFacts>::failure(
            target.error());
    }
    auto audio = MediaResolvedAudioOutputPlan::create(
        target.value(), std::nullopt, 1'024);
    if (!audio) {
        return ::media::Result<MediaProjectMpegTsResolvedPipelineFacts>::failure(
            audio.error());
    }
    return ::media::Result<MediaProjectMpegTsResolvedPipelineFacts>::success(
        MediaProjectMpegTsResolvedPipelineFacts{
            "h264", MediaEncodedPacketLayout::lengthPrefixed(4).value(),
            std::move(audio).value()});
}

::media::Result<MediaAvSyncPlan> synchronizationPlan()
{
    MediaRealtimeRtpTranscodeRequest request;
    request.mediaId = "scheduled-ts-decode";
    request.input.type = RealtimeInputType::MpegTsUdp;
    request.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    request.input.url = "udp://127.0.0.1:5000";
    request.output.streamLayout =
        RealtimeOutputStreamLayout::MuxedTransportStream;
    request.output.url = "udp://127.0.0.1:7000";
    request.parameters.execution.includeAudio = true;
    request.parameters.audio.sampleRate = 48'000;
    request.parameters.queues.packet = 256;
    request.avSyncStartup.maximumVideoUnitBytes = 4 * 1024 * 1024;
    request.avSyncStartup.maximumAudioUnitBytes = 1024 * 1024;
    request.avSyncStartup.maximumGap = milliseconds(40);
    const auto selected = selectedTsProgram();
    auto facts = resolvedPipelineFacts();
    if (!facts) {
        return ::media::Result<MediaAvSyncPlan>::failure(facts.error());
    }
    auto planned = MediaAvSyncPlanner::plan(
        request, &selected, &facts.value());
    if (!planned) {
        return ::media::Result<MediaAvSyncPlan>::failure(planned.error());
    }
    auto result = std::move(planned).value();
    if (!result.topology ||
        *result.topology != MediaAvSyncTopology::MpegTsToMpegTs ||
        !result.ts || !result.ts->outputMux) {
        return ::media::Result<MediaAvSyncPlan>::failure(
            ::media::ErrorInfo::internalError(
                "scheduled TS decode fixture requires complete MpegTsToMpegTs output"));
    }
    result.audioServo.commandLeadNs = milliseconds(1'500);
    result.audioServo.compensationWindowNs = milliseconds(2'000);
    result.audioServo.frequencyFilterTimeConstantNs = milliseconds(5'000);
    return ::media::Result<MediaAvSyncPlan>::success(std::move(result));
}

::media::Result<MediaRealtimeAvSyncRuntimePlan> runtimePlan(
    const std::filesystem::path& output,
    MediaAvSyncPlan synchronization)
{
    MediaGraphQueueParameters queues;
    queues.metadata = 8;
    queues.packet = 512;
    queues.frame = 128;
    queues.mux = 512;
    MediaRealtimeAvSyncAssemblyPlan assembly{
        MediaMpegTsInputClockAssemblyPlan{},
        MediaInitialGenerationPolicy::FirstLockedOnlyFailOnChange,
        MediaClockEvidencePolicy::RequireLockedFailOnDegradedOrReacquire,
        {"video", MediaPacketDurationPlan{true},
         MediaDecodeOrderMode::ReorderedRequiresDecodeTime, 4,
         milliseconds(1'000)},
        {"audio", MediaPacketDurationPlan{true},
         MediaDecodeOrderMode::PresentationOrderNoReorder, 4,
         milliseconds(1'000)},
        milliseconds(20)};
    if (!synchronization.ts || !synchronization.ts->outputMux) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            ::media::ErrorInfo::internalError(
                "scheduled TS runtime requires planner-owned output mux"));
    }
    auto outputPlan = MediaProjectMpegTsOutputPlan::accept(
        48'000, *synchronization.ts->outputMux);
    if (!outputPlan) {
        return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::failure(
            outputPlan.error());
    }
    return ::media::Result<MediaRealtimeAvSyncRuntimePlan>::success(
        MediaRealtimeAvSyncRuntimePlan{
        MediaAvSyncGroupKey("scheduled-ts-decode"), std::move(synchronization),
        std::move(assembly), MediaAvSyncOutputAdapterKind::ProjectMpegTs,
        MediaProjectMpegTsRuntimeOutputPlan{
            output.string(), MediaOutputResourceKind::ByteSink,
            MediaMuxSessionKind::ProjectMpegTs,
            std::move(outputPlan).value()},
        queues, MediaGraphBuildSupport::blockingEdgePolicySet(queues), {},
        MediaAvGenerationTransitionPlanner::plan(
            MediaAvSyncOutputAdapterKind::ProjectMpegTs,
            milliseconds(1'000), milliseconds(500)),
        {}, {}});
}

::media::Status verifySampleExtractorContract(
    const std::filesystem::path& path,
    const MediaTsMuxPlan& plan)
{
    auto annexBParameters = plan.parameters();
    annexBParameters.h264InputLayout = MediaTsH264InputLayout::AnnexB;
    auto annexBPlan = MediaTsMuxPlan::create(std::move(annexBParameters));
    if (!annexBPlan) return ::media::Status::failure(annexBPlan.error());
    auto unsupported = ScheduledMpegTsDecodeSampleFixture::load(
        path, annexBPlan.value());
    if (unsupported ||
        unsupported.error().code != ::media::ErrorCode::Unsupported) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "scheduled MPEG-TS extractor must reject a non-length-prefixed planner layout"));
    }

    auto mismatchedParameters = plan.parameters();
    mismatchedParameters.h264NalLengthBytes = 3;
    auto mismatchedPlan = MediaTsMuxPlan::create(
        std::move(mismatchedParameters));
    if (!mismatchedPlan) {
        return ::media::Status::failure(mismatchedPlan.error());
    }
    auto mismatched = ScheduledMpegTsDecodeSampleFixture::load(
        path, mismatchedPlan.value());
    if (mismatched ||
        mismatched.error().code != ::media::ErrorCode::InvalidArgument) {
        return ::media::Status::failure(::media::ErrorInfo::internalError(
            "scheduled MPEG-TS extractor must enforce the planner-declared NAL length"));
    }
    return ::media::Status::success();
}

} // namespace

int main()
{
    auto classified = verifyExitClassification();
    if (!classified) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            classified.error());
    }
    std::cerr << "[scheduled-ts] checking prerequisites\n";
    auto prerequisites = ScheduledRtpDecodePrerequisites::check(
        ffmpegPath(), samplePath());
    if (!prerequisites) {
        return ScheduledRtpDecodePrerequisites::externalFailureExit(
            prerequisites.error());
    }
    std::cerr << "[scheduled-ts] preparing 48 kHz AAC-LC sample\n";
    auto prepared = ScheduledMpegTsDecodeSamplePreparer::prepare(
        ffmpegPath(), samplePath());
    if (!prepared) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            prepared.error());
    }
    std::cerr << "[scheduled-ts] loading H264/AAC sample access units\n";
    TemporaryOutput output;
    auto synchronization = synchronizationPlan();
    if (!synchronization) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            synchronization.error());
    }
    const MediaTsMuxPlan transport =
        *synchronization.value().ts->outputMux;
    if (auto contract = verifySampleExtractorContract(
            prepared.value().path(), transport); !contract) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            contract.error());
    }
    auto sample = ScheduledMpegTsDecodeSampleFixture::load(
        prepared.value().path(), transport);
    if (!sample) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            sample.error());
    }
    auto fixture = std::move(sample).value();
    const MediaPlaybackEpoch epoch{
        milliseconds(10'000), milliseconds(2'000), 1};
    auto plan = runtimePlan(
        output.path(), std::move(synchronization).value());
    if (!plan) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            plan.error());
    }
    std::cerr << "[scheduled-ts] driving production scheduled output graph\n";
    auto written = ScheduledMpegTsOutputIntegrationRuntime::write(
        plan.value(), epoch, fixture);
    if (!written) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            written.error());
    }
    std::error_code sizeError;
    const auto outputBytes = std::filesystem::file_size(
        output.path(), sizeError);
    std::cerr << "[scheduled-ts] artifact path=" << output.path().string()
              << " bytes="
              << (sizeError ? std::string("unavailable:") + sizeError.message()
                            : std::to_string(outputBytes))
              << '\n';
    std::cerr << "[scheduled-ts] verifying MPEG-TS artifact and decoding EOF\n";
    auto verified = MpegTsOutputArtifactVerifier::verify(
        output.path(), transport, epoch, fixture);
    if (!verified) {
        return ScheduledRtpDecodePrerequisites::pipelineFailureExit(
            verified.error());
    }
    std::cout << "scheduled project MPEG-TS production path passed: "
                 "H264/AAC decode, PAT/PMT/PIDs, continuity, PCR, and epoch "
                 "PTS/DTS verified\n";
    return 0;
}
