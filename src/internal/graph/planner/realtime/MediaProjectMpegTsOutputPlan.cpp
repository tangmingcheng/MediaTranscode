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
constexpr MediaTsContinuitySeeds ProjectContinuitySeeds{0, 0, 0, 0};

::media::Status validateProjectProtocolContract(
    int audioSampleRate,
    const MediaTsMuxPlanParameters& parameters)
{
    const bool aacFrequencyMatchesSampleRate =
        parameters.aac.samplingFrequencyIndex < MediaAacSampleRates.size() &&
        MediaAacSampleRates[parameters.aac.samplingFrequencyIndex] ==
            audioSampleRate;
    const bool h264InputContractMatches =
        (parameters.h264InputLayout == MediaTsH264InputLayout::AnnexB &&
         parameters.h264NalLengthBytes == 4) ||
        (parameters.h264InputLayout ==
             MediaTsH264InputLayout::LengthPrefixed &&
         parameters.h264NalLengthBytes >= 1 &&
         parameters.h264NalLengthBytes <= 4);
    if (audioSampleRate != ProjectAudioSampleRate ||
        parameters.transportStreamId != ProjectTransportStreamId ||
        parameters.programNumber != ProjectProgramNumber ||
        parameters.patPid != ProjectPatPid ||
        parameters.programMapPid != ProjectProgramMapPid ||
        parameters.videoPid != ProjectVideoPid ||
        parameters.audioPid != ProjectAudioPid ||
        parameters.pcrPid != parameters.videoPid ||
        parameters.tableVersion != ProjectTableVersion ||
        parameters.psiRepeatInterval.nanoseconds() !=
            ProjectPsiRepeatIntervalNs ||
        parameters.videoStreamType != ProjectH264StreamType ||
        parameters.audioStreamType != ProjectAacStreamType ||
        !h264InputContractMatches ||
        parameters.parameterSetPolicy !=
            MediaTsParameterSetPolicy::BeforeRandomAccess ||
        parameters.aac.mpegId != ProjectAacAdts.mpegId ||
        parameters.aac.audioObjectType != ProjectAacAdts.audioObjectType ||
        !aacFrequencyMatchesSampleRate ||
        parameters.aac.samplingFrequencyIndex !=
            ProjectAacAdts.samplingFrequencyIndex ||
        parameters.aac.channelConfiguration !=
            ProjectAacAdts.channelConfiguration ||
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
        parameters.packetSize != ProjectPacketSize ||
        parameters.continuity != ProjectContinuitySeeds ||
        parameters.maximumAudioAccessUnitSamples !=
            ProjectAudioAccessUnitSamples) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Project MPEG-TS encoded facts violate the H.264/AAC/PID/PCR/clock protocol contract"));
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

::media::Result<MediaProjectMpegTsOutputPlan> MediaProjectMpegTsOutputPlan::create(
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
        ProjectProgramMapPid, ProjectVideoPid, ProjectAudioPid,
        ProjectVideoPid, ProjectTableVersion,
        MediaRunningTime::fromNanoseconds(ProjectPsiRepeatIntervalNs),
        ProjectH264StreamType, ProjectAacStreamType,
        videoInput.value().layout, videoInput.value().lengthFieldBytes,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        ProjectAacAdts,
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(ProjectPcrIntervalNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrGapNs),
            MediaRunningTime::fromNanoseconds(ProjectMaximumPcrJitterNs),
            ProjectClockTimeBaseNumerator,
            ProjectClockTimeBaseDenominator},
        transportDecodeLead, startupEmissionPreroll,
        ProjectPacketSize, ProjectContinuitySeeds,
        maximumPacketsPerDatagram, transportKind,
        audioOutput.codecFrameSamples()});
    if (!mux) return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(mux.error());
    return fromEncodedFacts(
        audioOutput.sampleRate(), std::move(mux).value());
}

::media::Result<MediaProjectMpegTsOutputPlan>
MediaProjectMpegTsOutputPlan::fromEncodedFacts(
    int audioSampleRate,
    MediaTsMuxPlan muxPlan)
{
    auto contract = validateProjectProtocolContract(
        audioSampleRate, muxPlan.parameters());
    if (!contract) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            contract.error());
    }
    return ::media::Result<MediaProjectMpegTsOutputPlan>::success(
        MediaProjectMpegTsOutputPlan(audioSampleRate, std::move(muxPlan)));
}

MediaProjectMpegTsOutputPlan::MediaProjectMpegTsOutputPlan(
    int audioSampleRate, MediaTsMuxPlan muxPlan)
    : m_audioSampleRate(audioSampleRate), m_muxPlan(std::move(muxPlan))
{
}

int MediaProjectMpegTsOutputPlan::audioSampleRate() const noexcept { return m_audioSampleRate; }
const MediaTsMuxPlan& MediaProjectMpegTsOutputPlan::muxPlan() const noexcept { return m_muxPlan; }

} // namespace media::ffmpeg::graph
