#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"

#include "internal/graph/planner/realtime/MediaTsReceiverTimingPlanner.h"

#include "internal/graph/protocol/codec/MediaAacAudioSpecificConfigParser.h"
#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

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
constexpr std::uint8_t ProjectHevcStreamType = 0x24;
constexpr std::uint8_t ProjectAacStreamType = 0x0F;
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
    if (parameters.transportStreamId != ProjectTransportStreamId ||
        parameters.programNumber != ProjectProgramNumber ||
        parameters.patPid != ProjectPatPid ||
        parameters.programMapPid != ProjectProgramMapPid ||
        parameters.tableVersion != ProjectTableVersion ||
        parameters.psiRepeatInterval.nanoseconds() !=
            ProjectPsiRepeatIntervalNs ||
        parameters.parameterSetPolicy !=
            MediaTsParameterSetPolicy::BeforeRandomAccess ||
        parameters.clock.pcrInterval != MediaTsReceiverTimingPlanner::pcrInterval() ||
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
                "Project MPEG-TS encoded facts violate the common video/PID/PCR/clock protocol contract"));
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
        program->videoStreamType !=
            muxPlan.parameters().video.streamType() ||
        program->continuity != ProjectVideoContinuitySeeds) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS VideoOnly facts violate the typed PMT/PES/PCR contract"));
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
    const bool supportedAac = program &&
        program->aac.mpegId == 0 &&
        program->aac.audioObjectType == MediaAacLcAudioObjectType &&
        program->aac.samplingFrequencyIndex < MediaAacSampleRates.size() &&
        program->aac.channelConfiguration >= 1 &&
        program->aac.channelConfiguration <= 7;
    if (!program ||
        program->videoPid != ProjectVideoPid ||
        program->audioPid != ProjectAudioPid ||
        program->pcrPid != ProjectVideoPid ||
        program->videoStreamType !=
            muxPlan.parameters().video.streamType() ||
        program->audioStreamType != ProjectAacStreamType ||
        !supportedAac ||
        program->continuity != ProjectAvContinuitySeeds ||
        program->maximumAudioAccessUnitSamples !=
            ProjectAudioAccessUnitSamples) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS AudioVideo facts violate the video/AAC PMT/PES/PCR contract"));
    }
    return ::media::Status::success();
}

::media::Result<MediaTsAacAdtsPlan> aacAdtsPlan(
    const MediaResolvedAudioOutputPlan& audioOutput)
{
    auto configuration = makeMediaAacLcLongFrameAudioSpecificConfig(
        audioOutput.sampleRate(), audioOutput.channels());
    if (!configuration) {
        return ::media::Result<MediaTsAacAdtsPlan>::failure(
            configuration.error());
    }
    auto parsed = parseMediaAacAudioSpecificConfig(configuration.value());
    if (!parsed || parsed.value().frameSamples != ProjectAudioAccessUnitSamples) {
        return ::media::Result<MediaTsAacAdtsPlan>::failure(
            parsed ? ::media::ErrorInfo::unsupported(
                         "Project MPEG-TS requires AAC-LC long-frame output")
                   : parsed.error());
    }
    return ::media::Result<MediaTsAacAdtsPlan>::success(MediaTsAacAdtsPlan{
        0, parsed.value().audioObjectType,
        parsed.value().samplingFrequencyIndex,
        parsed.value().channelConfiguration});
}

::media::Result<MediaTsVideoCodec> videoCodec(const std::string& codecName)
{
    const auto canonical = canonicalCodecName(codecName);
    if (canonical == "h264") {
        return ::media::Result<MediaTsVideoCodec>::success(
            MediaTsVideoCodec::H264);
    }
    if (canonical == "hevc") {
        return ::media::Result<MediaTsVideoCodec>::success(
            MediaTsVideoCodec::Hevc);
    }
    return ::media::Result<MediaTsVideoCodec>::failure(
        ::media::ErrorInfo::unsupported(
            "Project MPEG-TS output supports only resolved H.264 or HEVC video"));
}

std::uint8_t videoStreamType(MediaTsVideoCodec codec) noexcept
{
    return codec == MediaTsVideoCodec::H264
        ? ProjectH264StreamType : ProjectHevcStreamType;
}

::media::Result<MediaTsVideoElementaryStreamContract> videoInputContract(
    MediaTsVideoCodec codec,
    const MediaEncodedPacketLayout& packetLayout)
{
    if (packetLayout.kind() ==
        MediaEncodedPacketLayoutKind::StartCodeDelimited) {
        return MediaTsVideoElementaryStreamContract::create(
            codec, MediaTsNalLayout::AnnexB, 0,
            videoStreamType(codec));
    }
    if (packetLayout.kind() !=
        MediaEncodedPacketLayoutKind::LengthPrefixed) {
        return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS output does not support the resolved encoded packet layout"));
    }
    const auto lengthFieldBytes = packetLayout.lengthFieldBytes();
    if (!lengthFieldBytes || *lengthFieldBytes > 4) {
        return ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS video output supports length-field widths from one through four bytes"));
    }
    return MediaTsVideoElementaryStreamContract::create(
        codec, MediaTsNalLayout::LengthPrefixed, *lengthFieldBytes,
        videoStreamType(codec));
}

} // namespace

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::createVideoOnly(
    const std::string& videoCodecName,
    const MediaEncodedPacketLayout& videoPacketLayout,
    MediaRunningTime transportDecodeLead,
    MediaRunningTime startupEmissionPreroll,
    MediaOutputTransportKind transportKind,
    std::uint16_t maximumPacketsPerDatagram)
{
    auto codec = videoCodec(videoCodecName);
    auto videoInput = codec
        ? videoInputContract(codec.value(), videoPacketLayout)
        : ::media::Result<MediaTsVideoElementaryStreamContract>::failure(
              codec.error());
    if (!codec || !videoInput) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            codec ? videoInput.error() : codec.error());
    }
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        ProjectTransportStreamId, ProjectProgramNumber, ProjectPatPid,
        ProjectProgramMapPid, ProjectTableVersion,
        MediaRunningTime::fromNanoseconds(ProjectPsiRepeatIntervalNs),
        MediaTsVideoOnlyProgramPlan{
            ProjectVideoPid, ProjectVideoPid,
            videoInput.value().streamType(),
            ProjectVideoContinuitySeeds},
        videoInput.value(),
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsOutputClockPolicy{
            MediaTsReceiverTimingPlanner::pcrInterval(),
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
    std::uint16_t maximumPacketsPerDatagram)
{
    auto codec = videoCodec(videoCodecName);
    if (!codec || audioOutput.codecName() != "aac" ||
        audioOutput.profile().knowledge() != MediaAudioProfileKnowledge::Known ||
        audioOutput.profile().canonicalName() != "aac_low" ||
        audioOutput.codecFrameSamples() != ProjectAudioAccessUnitSamples) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS output requires resolved H.264/HEVC and representable AAC-LC long-frame output"));
    }
    auto aac = aacAdtsPlan(audioOutput);
    if (!aac) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            aac.error());
    }
    auto videoInput = videoInputContract(codec.value(), videoPacketLayout);
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
            videoInput.value().streamType(), ProjectAacStreamType, aac.value(),
            ProjectAvContinuitySeeds, audioOutput.codecFrameSamples()},
        videoInput.value(),
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsOutputClockPolicy{
            MediaTsReceiverTimingPlanner::pcrInterval(),
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
