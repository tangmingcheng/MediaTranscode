#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

struct H264MuxInputContract final {
    MediaTsH264InputLayout layout;
    std::uint8_t lengthFieldBytes;
};

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
    MediaOutputTransportKind transportKind,
    std::uint8_t maximumPacketsPerDatagram)
{
    if (canonicalCodecName(videoCodecName) != "h264" ||
        audioOutput.codecName() != "aac" ||
        audioOutput.profile().knowledge() != MediaAudioProfileKnowledge::Known ||
        audioOutput.profile().canonicalName() != "aac_low" ||
        audioOutput.sampleRate() != 48'000 || audioOutput.channels() != 2) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            ::media::ErrorInfo::unsupported(
                "Project MPEG-TS output requires resolved H.264 and known AAC-LC 48 kHz stereo output"));
    }
    auto videoInput = h264MuxInputContract(videoPacketLayout);
    if (!videoInput) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            videoInput.error());
    }
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        videoInput.value().layout, videoInput.value().lengthFieldBytes,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        transportDecodeLead, 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, maximumPacketsPerDatagram,
        transportKind, audioOutput.codecFrameSamples()});
    if (!mux) return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(mux.error());
    return ::media::Result<MediaProjectMpegTsOutputPlan>::success(
        MediaProjectMpegTsOutputPlan(audioOutput.sampleRate(), std::move(mux).value()));
}

::media::Result<MediaProjectMpegTsOutputPlan> MediaProjectMpegTsOutputPlan::accept(
    int audioSampleRate,
    MediaTsMuxPlan muxPlan)
{
    if (audioSampleRate <= 0 ||
        muxPlan.parameters().aac.samplingFrequencyIndex > 12 ||
        muxPlan.parameters().maximumPacketsPerDatagram == 0) {
        return ::media::Result<MediaProjectMpegTsOutputPlan>::failure(
            ::media::ErrorInfo::invalidArgument(
                "accepted project MPEG-TS output plan is incomplete"));
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
