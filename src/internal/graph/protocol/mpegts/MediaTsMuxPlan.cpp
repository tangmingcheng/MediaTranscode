#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint16_t MinimumAssignablePid = 0x0020;
constexpr std::uint16_t NullPid = 0x1FFF;
constexpr std::size_t RtpFixedHeaderBytes = 12;

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

bool validParameterSetPolicy(MediaTsParameterSetPolicy policy) noexcept
{
    switch (policy) {
    case MediaTsParameterSetPolicy::Never:
    case MediaTsParameterSetPolicy::BeforeRandomAccess:
        return true;
    }
    return false;
}

bool validTransportKind(MediaOutputTransportKind kind) noexcept
{
    switch (kind) {
    case MediaOutputTransportKind::UdpDatagrams:
    case MediaOutputTransportKind::RtpAvp:
        return true;
    }
    return false;
}

bool validContinuitySeeds(const MediaTsVideoContinuitySeeds& seeds) noexcept
{
    return seeds.pat <= 15 && seeds.pmt <= 15 && seeds.video <= 15;
}

bool validContinuitySeeds(
    const MediaTsAudioVideoContinuitySeeds& seeds) noexcept
{
    return seeds.pat <= 15 && seeds.pmt <= 15 && seeds.video <= 15 &&
           seeds.audio <= 15;
}

} // namespace

::media::Result<std::uint16_t>
MediaTsMuxPlan::maximumPacketsPerRtpDatagram(
    std::size_t maximumDatagramBytes)
{
    return maximumPacketsPerDatagram(
        maximumDatagramBytes, MediaOutputTransportKind::RtpAvp);
}

::media::Result<std::uint16_t>
MediaTsMuxPlan::maximumPacketsPerDatagram(
    std::size_t maximumUdpPayloadBytes,
    MediaOutputTransportKind transportKind)
{
    const auto protocolHeaderBytes =
        transportKind == MediaOutputTransportKind::RtpAvp
            ? RtpFixedHeaderBytes
            : transportKind == MediaOutputTransportKind::UdpDatagrams
                ? std::size_t{0}
                : maximumUdpPayloadBytes;
    if (maximumUdpPayloadBytes <= protocolHeaderBytes) {
        return ::media::Result<std::uint16_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS datagram cannot carry one complete TS packet"));
    }
    const auto payloadCapacity =
        maximumUdpPayloadBytes - protocolHeaderBytes;
    const auto packetCount = payloadCapacity / std::size_t{188};
    if (packetCount == 0 ||
        packetCount > (std::numeric_limits<std::uint16_t>::max)()) {
        return ::media::Result<std::uint16_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS packet geometry is outside the protocol plan range"));
    }
    return ::media::Result<std::uint16_t>::success(
        static_cast<std::uint16_t>(packetCount));
}

::media::Result<MediaTsMuxPlan> MediaTsMuxPlan::create(
    MediaTsMuxPlanParameters parameters)
{
    if (parameters.transportStreamId == 0 || parameters.programNumber == 0) {
        return invalid("requires nonzero transport-stream and program identity");
    }
    const auto* videoOnly = std::get_if<MediaTsVideoOnlyProgramPlan>(
        &parameters.program);
    const auto* audioVideo = std::get_if<MediaTsAudioVideoProgramPlan>(
        &parameters.program);
    if ((videoOnly == nullptr) == (audioVideo == nullptr)) {
        return invalid("requires exactly one typed program stream set");
    }
    const std::uint16_t videoPid = videoOnly
        ? videoOnly->videoPid : audioVideo->videoPid;
    const std::uint16_t pcrPid = videoOnly
        ? videoOnly->pcrPid : audioVideo->pcrPid;
    if (parameters.patPid != 0 || !assignablePid(parameters.programMapPid) ||
        !assignablePid(videoPid) || !assignablePid(pcrPid) ||
        (audioVideo && !assignablePid(audioVideo->audioPid))) {
        return invalid("contains a reserved or unsupported PID");
    }
    if (parameters.programMapPid == videoPid ||
        (audioVideo &&
         (parameters.programMapPid == audioVideo->audioPid ||
          videoPid == audioVideo->audioPid)) ||
        pcrPid != videoPid) {
        return invalid("requires distinct PMT/ES PIDs and an ES PCR PID");
    }
    if (parameters.tableVersion > 31 ||
        parameters.timing.psiRepeatInterval().value.nanoseconds() <= 0 ||
        parameters.timing.psiRepeatInterval().value <=
            parameters.timing.pcrInterval().value) {
        return invalid("contains an invalid PSI cadence or version");
    }
    const std::uint8_t programVideoStreamType = videoOnly
        ? videoOnly->videoStreamType : audioVideo->videoStreamType;
    if (programVideoStreamType != parameters.video.streamType() ||
        (audioVideo && audioVideo->audioStreamType != 0x0F)) {
        return invalid("contains conflicting video or AAC stream types");
    }
    if (!validParameterSetPolicy(parameters.parameterSetPolicy)) {
        return invalid("contains an invalid video elementary-stream contract");
    }
    if (audioVideo &&
        (audioVideo->aac.mpegId > 1 ||
         audioVideo->aac.audioObjectType < 1 ||
         audioVideo->aac.audioObjectType > 4 ||
         audioVideo->aac.samplingFrequencyIndex > 12 ||
         audioVideo->aac.channelConfiguration < 1 ||
         audioVideo->aac.channelConfiguration > 7 ||
         audioVideo->maximumAudioAccessUnitSamples <= 0)) {
        return invalid("contains an invalid AAC ADTS contract");
    }
    const auto clock = parameters.timing.clockPolicy();
    if (clock.pcrInterval.nanoseconds() <= 0 ||
        clock.maximumPcrGap <= clock.pcrInterval ||
        clock.maximumPcrJitter.nanoseconds() <= 0 ||
        clock.maximumPcrJitter >= clock.pcrInterval ||
        clock.timestampTimeBaseNumerator != 1 ||
        clock.timestampTimeBaseDenominator != 90'000) {
        return invalid("contains an invalid output clock policy");
    }
    if (parameters.transportDecodeLead.nanoseconds() <= 0 ||
        parameters.startupEmissionPreroll.nanoseconds() <= 0 ||
        parameters.startupEmissionPreroll >
            parameters.transportDecodeLead ||
        parameters.packetSize != 188 ||
        (videoOnly && !validContinuitySeeds(videoOnly->continuity)) ||
        (audioVideo && !validContinuitySeeds(audioVideo->continuity)) ||
        parameters.maximumPacketsPerDatagram < 1 ||
        !validTransportKind(parameters.transportKind)) {
        return invalid("contains an invalid transport contract");
    }
    return ::media::Result<MediaTsMuxPlan>::success(
        MediaTsMuxPlan(std::move(parameters)));
}

MediaTsMuxPlan::MediaTsMuxPlan(MediaTsMuxPlanParameters parameters) noexcept
    : m_parameters(std::move(parameters)),
      m_clockPolicy(m_parameters.timing.clockPolicy())
{
}

const MediaTsMuxPlanParameters& MediaTsMuxPlan::parameters() const noexcept
{
    return m_parameters;
}

const MediaTsVideoOnlyProgramPlan*
MediaTsMuxPlan::videoOnlyProgram() const noexcept
{
    return std::get_if<MediaTsVideoOnlyProgramPlan>(&m_parameters.program);
}

const MediaTsAudioVideoProgramPlan*
MediaTsMuxPlan::audioVideoProgram() const noexcept
{
    return std::get_if<MediaTsAudioVideoProgramPlan>(&m_parameters.program);
}

std::uint16_t MediaTsMuxPlan::videoPid() const noexcept
{
    return std::visit(
        [](const auto& program) { return program.videoPid; },
        m_parameters.program);
}

std::uint16_t MediaTsMuxPlan::pcrPid() const noexcept
{
    return std::visit(
        [](const auto& program) { return program.pcrPid; },
        m_parameters.program);
}

std::uint8_t MediaTsMuxPlan::videoStreamType() const noexcept
{
    return std::visit(
        [](const auto& program) { return program.videoStreamType; },
        m_parameters.program);
}

const MediaTsOutputClockPolicy& MediaTsMuxPlan::clockPolicy() const noexcept
{
    return m_clockPolicy;
}

const MediaMpegTsTimingPolicy& MediaTsMuxPlan::timingPolicy() const noexcept
{
    return m_parameters.timing;
}

MediaRunningTime MediaTsMuxPlan::transportDecodeLead() const noexcept
{
    return m_parameters.transportDecodeLead;
}

MediaRunningTime MediaTsMuxPlan::startupEmissionPreroll() const noexcept
{
    return m_parameters.startupEmissionPreroll;
}

} // namespace media::ffmpeg::graph
