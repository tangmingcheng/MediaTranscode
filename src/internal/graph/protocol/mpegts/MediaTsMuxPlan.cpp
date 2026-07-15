#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint16_t MinimumAssignablePid = 0x0020;
constexpr std::uint16_t NullPid = 0x1FFF;

::media::Result<MediaTsMuxPlan> invalid(const char* reason)
{
    return ::media::Result<MediaTsMuxPlan>::failure(
        ::media::ErrorInfo::invalidArgument(
            std::string("MPEG-TS mux plan ") + reason));
}

bool assignablePid(std::uint16_t pid) noexcept
{
    return pid >= MinimumAssignablePid && pid < NullPid;
}

bool validH264Layout(MediaTsH264InputLayout layout) noexcept
{
    switch (layout) {
    case MediaTsH264InputLayout::AnnexB:
    case MediaTsH264InputLayout::LengthPrefixed:
        return true;
    }
    return false;
}

bool validParameterSetPolicy(MediaTsParameterSetPolicy policy) noexcept
{
    switch (policy) {
    case MediaTsParameterSetPolicy::Never:
    case MediaTsParameterSetPolicy::BeforeRandomAccess:
        return true;
    }
    return false;
}

bool validTransportKind(MediaTsOutputTransportKind kind) noexcept
{
    switch (kind) {
    case MediaTsOutputTransportKind::Udp:
        return true;
    }
    return false;
}

bool validContinuitySeeds(const MediaTsContinuitySeeds& seeds) noexcept
{
    return seeds.pat <= 15 && seeds.pmt <= 15 && seeds.video <= 15 &&
           seeds.audio <= 15;
}

} // namespace

::media::Result<MediaTsMuxPlan> MediaTsMuxPlan::create(
    MediaTsMuxPlanParameters parameters)
{
    if (parameters.transportStreamId == 0 || parameters.programNumber == 0) {
        return invalid("requires nonzero transport-stream and program identity");
    }
    if (parameters.patPid != 0 || !assignablePid(parameters.programMapPid) ||
        !assignablePid(parameters.videoPid) ||
        !assignablePid(parameters.audioPid) ||
        !assignablePid(parameters.pcrPid)) {
        return invalid("contains a reserved or unsupported PID");
    }
    if (parameters.programMapPid == parameters.videoPid ||
        parameters.programMapPid == parameters.audioPid ||
        parameters.videoPid == parameters.audioPid ||
        (parameters.pcrPid != parameters.videoPid &&
         parameters.pcrPid != parameters.audioPid)) {
        return invalid("requires distinct PMT/ES PIDs and an ES PCR PID");
    }
    if (parameters.tableVersion > 31 ||
        parameters.psiRepeatInterval.nanoseconds() <= 0 ||
        parameters.psiRepeatInterval <= parameters.clock.pcrInterval) {
        return invalid("contains an invalid PSI cadence or version");
    }
    if (parameters.videoStreamType != 0x1B ||
        parameters.audioStreamType != 0x0F) {
        return invalid("supports only H.264 and AAC stream types");
    }
    if (!validH264Layout(parameters.h264InputLayout) ||
        parameters.h264NalLengthBytes < 1 ||
        parameters.h264NalLengthBytes > 4 ||
        !validParameterSetPolicy(parameters.parameterSetPolicy)) {
        return invalid("contains an invalid H.264 input contract");
    }
    if (parameters.aac.mpegId > 1 || parameters.aac.audioObjectType < 1 ||
        parameters.aac.audioObjectType > 4 ||
        parameters.aac.samplingFrequencyIndex > 12 ||
        parameters.aac.channelConfiguration < 1 ||
        parameters.aac.channelConfiguration > 7) {
        return invalid("contains an invalid AAC ADTS contract");
    }
    if (parameters.clock.pcrInterval.nanoseconds() <= 0 ||
        parameters.clock.maximumPcrGap <= parameters.clock.pcrInterval ||
        parameters.clock.maximumPcrJitter.nanoseconds() <= 0 ||
        parameters.clock.maximumPcrJitter >= parameters.clock.pcrInterval ||
        parameters.clock.timestampTimeBaseNumerator != 1 ||
        parameters.clock.timestampTimeBaseDenominator != 90'000) {
        return invalid("contains an invalid output clock policy");
    }
    if (parameters.transportDecodeLead.nanoseconds() <= 0 ||
        parameters.packetSize != 188 ||
        !validContinuitySeeds(parameters.continuity) ||
        parameters.maximumPacketsPerDatagram < 1 ||
        parameters.maximumPacketsPerDatagram > 7 ||
        !validTransportKind(parameters.transportKind) ||
        parameters.maximumAudioAccessUnitSamples <= 0) {
        return invalid("contains an invalid transport contract");
    }
    return ::media::Result<MediaTsMuxPlan>::success(
        MediaTsMuxPlan(std::move(parameters)));
}

MediaTsMuxPlan::MediaTsMuxPlan(MediaTsMuxPlanParameters parameters) noexcept
    : m_parameters(std::move(parameters))
{
}

const MediaTsMuxPlanParameters& MediaTsMuxPlan::parameters() const noexcept
{
    return m_parameters;
}

const MediaTsOutputClockPolicy& MediaTsMuxPlan::clockPolicy() const noexcept
{
    return m_parameters.clock;
}

MediaRunningTime MediaTsMuxPlan::transportDecodeLead() const noexcept
{
    return m_parameters.transportDecodeLead;
}

} // namespace media::ffmpeg::graph
