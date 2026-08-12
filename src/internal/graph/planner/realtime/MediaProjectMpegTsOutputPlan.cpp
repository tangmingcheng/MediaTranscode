#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"

#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct H264MuxInputContract final {
    MediaTsH264InputLayout layout;
    std::uint8_t lengthFieldBytes;
};

constexpr int ProjectAudioSampleRate = 48'000;
constexpr std::uint8_t ProjectAudioChannels = 2;
constexpr int ProjectAudioAccessUnitSamples = MediaAacLongFrameSamples;
constexpr std::uint16_t ProjectTransportStreamId = 1;
constexpr std::uint16_t ProjectProgramNumber = 1;
constexpr std::uint16_t ProjectPatPid = 0x0000;
constexpr std::uint16_t ProjectProgramMapPid = 0x0100;
constexpr std::uint16_t ProjectVideoPid = 0x0101;
constexpr std::uint16_t ProjectAudioPid = 0x0102;
constexpr std::uint8_t ProjectTableVersion = 0;
constexpr std::int64_t ProjectPsiRepeatIntervalNs = 100'000'000;
constexpr std::uint8_t ProjectH264StreamType = 0x1B;
constexpr std::uint8_t ProjectAacStreamType = 0x0F;
constexpr MediaTsAacAdtsPlan ProjectAacAdts{
    0, MediaAacLcAudioObjectType, 3, ProjectAudioChannels};
constexpr std::int64_t ProjectPcrIntervalNs = 20'000'000;
constexpr std::int64_t ProjectMaximumPcrGapNs = 100'000'000;
constexpr std::int64_t ProjectMaximumPcrJitterNs = 5'000'000;
constexpr int ProjectClockTimeBaseNumerator = 1;
constexpr int ProjectClockTimeBaseDenominator = 90'000;
constexpr std::uint16_t ProjectPacketSize = 188;
constexpr MediaTsVideoContinuitySeeds ProjectVideoContinuitySeeds{0, 0, 0};
constexpr MediaTsAudioVideoContinuitySeeds ProjectAvContinuitySeeds{
    0, 0, 0, 0};

::media::Status validateCommonProjectProtocolContract(
    const MediaTsMuxPlanParameters& parameters)
{
    const bool h264InputContractMatches =
        (parameters.h264InputLayout == MediaTsH264InputLayout::AnnexB &&
         parameters.h264NalLengthBytes == 4) ||
        (parameters.h264InputLayout ==
             MediaTsH264InputLayout::LengthPrefixed &&
         parameters.h264NalLengthBytes >= 1 &&
         parameters.h264NalLengthBytes <= 4);
    if (parameters.transportStreamId != ProjectTransportStreamId ||
        parameters.programNumber != ProjectProgramNumber ||
        parameters.patPid != ProjectPatPid ||
        parameters.programMapPid != ProjectProgramMapPid ||
        parameters.tableVersion != ProjectTableVersion ||
        parameters.psiRepeatInterval.nanoseconds() !=
            ProjectPsiRepeatIntervalNs ||
        !h264InputContractMatches ||
        parameters.parameterSetPolicy !=
            MediaTsParameterSetPolicy::BeforeRandomAccess ||
        parameters.clock.pcrInterval.nanoseconds() != ProjectPcrIntervalNs ||
        parameters.clock.maximumPcrGap.nanoseconds() !=
            ProjectMaximumPcrGapNs ||
        parameters.clock.maximumPcrJitter.nanoseconds() !=
            ProjectMaximumPcrJitterNs ||
        parameters.clock.timestampTimeBaseNumerator !=
            ProjectClockTimeBaseNumerator ||
        parameters.clock.timestampTimeBaseDenominator !=
            ProjectClockTimeBaseDenominator ||
        parameters.startupEmissionPreroll.nanoseconds() <= 0 ||
        parameters.startupEmissionPreroll >
            parameters.transportDecodeLead ||
        parameters.packetSize != ProjectPacketSize) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS encoded facts violate the common H.264/PID/PCR/clock protocol contract"));
    }
    return ::media::Status::success();
}

::media::Status validateVideoOnlyProjectProtocolContract(
    const MediaTsMuxPlan& muxPlan)
{
    if (auto common = validateCommonProjectProtocolContract(
            muxPlan.parameters()); !common) {
        return common;
    }
    const auto* program = muxPlan.videoOnlyProgram();
    if (!program || program->videoPid != ProjectVideoPid ||
        program->pcrPid != ProjectVideoPid ||
        program->videoStreamType != ProjectH264StreamType ||
        program->continuity != ProjectVideoContinuitySeeds) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS VideoOnly facts violate the H.264-only PMT/PES/PCR contract"));
    }
    return ::media::Status::success();
}

::media::Status validateAudioVideoProjectProtocolContract(
    const MediaTsMuxPlan& muxPlan)
{
    if (auto common = validateCommonProjectProtocolContract(
            muxPlan.parameters()); !common) {
        return common;
    }
    const auto* program = muxPlan.audioVideoProgram();
    const bool frequencyMatches = program &&
        program->aac.samplingFrequencyIndex < MediaAacSampleRates.size() &&
        MediaAacSampleRates[program->aac.samplingFrequencyIndex] ==
            ProjectAudioSampleRate;
    if (!program ||
        program->videoPid != ProjectVideoPid ||
        program->audioPid != ProjectAudioPid ||
        program->pcrPid != ProjectVideoPid ||
        program->videoStreamType != ProjectH264StreamType ||
        program->audioStreamType != ProjectAacStreamType ||
        program->aac != ProjectAacAdts || !frequencyMatches ||
        program->continuity != ProjectAvContinuitySeeds ||
        program->maximumAudioAccessUnitSamples !=
            ProjectAudioAccessUnitSamples) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS AudioVideo facts violate the H.264/AAC PMT/PES/PCR contract"));
    }
    return ::media::Status::success();
}

::media::Result<H264MuxInputContract> h264MuxInputContract(
    const MediaEncodedPacketLayout& packetLayout)
{
    if (packetLayout.kind() ==
        MediaEncodedPacketLayoutKind::StartCodeDelimited) {
        return ::media::Result<H264MuxInputContract>::success(
            H264MuxInputContract{MediaTsH264InputLayout::AnnexB, 4});
    }
    if (packetLayout.kind() !=
        MediaEncodedPacketLayoutKind::LengthPrefixed) {
        return ::media::Result<H264MuxInputContract>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS output does not support the resolved encoded packet layout"));
    }
    const auto lengthFieldBytes = packetLayout.lengthFieldBytes();
    if (!lengthFieldBytes || *lengthFieldBytes > 4) {
        return ::media::Result<H264MuxInputContract>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS H.264 output supports length-field widths from one through four bytes"));
    }
    return ::media::Result<H264MuxInputContract>::success(
        H264MuxInputContract{
            MediaTsH264InputLayout::LengthPrefixed, *lengthFieldBytes});
}

} // namespace

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::createVideoOnly(
    const std::string& videoCodecName,
    const MediaEncodedPacketLayout& videoPacketLayout,
    MediaRunningTime transportDecodeLead,
    MediaRunningTime startupEmissionPreroll,
    MediaOutputTransportKind transportKind,
    std::uint8_t maximumPacketsPerDatagram)
{
    if (canonicalCodecName(videoCodecName) != "h264") {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS VideoOnly output requires resolved H.264 output"));
    }
    auto videoInput = h264MuxInputContract(videoPacketLayout);
    if (!videoInput) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            videoInput.error());
    }
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        ProjectTransportStreamId, ProjectProgramNumber, ProjectPatPid,
        ProjectProgramMapPid, ProjectTableVersion,
        MediaRunningTime::fromNanoseconds(ProjectPsiRepeatIntervalNs),
        MediaTsVideoOnlyProgramPlan{
            ProjectVideoPid, ProjectVideoPid, ProjectH264StreamType,
            ProjectVideoContinuitySeeds},
        videoInput.value().layout, videoInput.value().lengthFieldBytes,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(ProjectPcrIntervalNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrGapNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrJitterNs),
            ProjectClockTimeBaseNumerator,
            ProjectClockTimeBaseDenominator},
        transportDecodeLead, startupEmissionPreroll, ProjectPacketSize,
        maximumPacketsPerDatagram, transportKind});
    if (!mux) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            mux.error());
    }
    return fromVideoOnlyEncodedFacts(std::move(mux).value());
}

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::createAudioVideo(
    const std::string& videoCodecName,
    const MediaEncodedPacketLayout& videoPacketLayout,
    const MediaResolvedAudioOutputPlan& audioOutput,
    MediaRunningTime transportDecodeLead,
    MediaRunningTime startupEmissionPreroll,
    MediaOutputTransportKind transportKind,
    std::uint8_t maximumPacketsPerDatagram)
{
    if (canonicalCodecName(videoCodecName) != "h264" ||
        audioOutput.codecName() != "aac" ||
        audioOutput.profile().knowledge() != MediaAudioProfileKnowledge::Known ||
        audioOutput.profile().canonicalName() != "aac_low" ||
        audioOutput.sampleRate() != ProjectAudioSampleRate ||
        audioOutput.channels() != ProjectAudioChannels ||
        audioOutput.codecFrameSamples() != ProjectAudioAccessUnitSamples) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS output requires resolved H.264 and known AAC-LC 48 kHz stereo long-frame output"));
    }
    auto videoInput = h264MuxInputContract(videoPacketLayout);
    if (!videoInput) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            videoInput.error());
    }
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        ProjectTransportStreamId, ProjectProgramNumber, ProjectPatPid,
        ProjectProgramMapPid, ProjectTableVersion,
        MediaRunningTime::fromNanoseconds(ProjectPsiRepeatIntervalNs),
        MediaTsAudioVideoProgramPlan{
            ProjectVideoPid, ProjectAudioPid, ProjectVideoPid,
            ProjectH264StreamType, ProjectAacStreamType, ProjectAacAdts,
            ProjectAvContinuitySeeds, audioOutput.codecFrameSamples()},
        videoInput.value().layout, videoInput.value().lengthFieldBytes,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(ProjectPcrIntervalNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrGapNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrJitterNs),
            ProjectClockTimeBaseNumerator,
            ProjectClockTimeBaseDenominator},
        transportDecodeLead, startupEmissionPreroll, ProjectPacketSize,
        maximumPacketsPerDatagram, transportKind});
    if (!mux) return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(mux.error());
    return fromAudioVideoEncodedFacts(std::move(mux).value());
}

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::fromVideoOnlyEncodedFacts(
    MediaTsMuxPlan muxPlan)
{
    auto contract = validateVideoOnlyProjectProtocolContract(muxPlan);
    if (!contract) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            contract.error());
    }
    return ::media::Result<MediaProjectMpegTsOutputPlan>::success(
        MediaProjectMpegTsOutputPlan(std::move(muxPlan)));
}

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::fromAudioVideoEncodedFacts(
    MediaTsMuxPlan muxPlan)
{
    auto contract = validateAudioVideoProjectProtocolContract(
        muxPlan);
    if (!contract) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            contract.error());
    }
    return ::media::Result<MediaProjectMpegTsOutputPlan>::success(
        MediaProjectMpegTsOutputPlan(std::move(muxPlan)));
}

MediaProjectMpegTsOutputPlan::MediaProjectMpegTsOutputPlan(
    MediaTsMuxPlan muxPlan)
    : m_muxPlan(std::move(muxPlan))
{
}

const MediaTsMuxPlan& MediaProjectMpegTsOutputPlan::muxPlan() const noexcept { return m_muxPlan; }

} // namespace media::ffmpeg::graph
