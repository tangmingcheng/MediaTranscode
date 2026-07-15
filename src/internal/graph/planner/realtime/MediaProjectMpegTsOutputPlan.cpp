#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaProjectMpegTsOutputPlan> MediaProjectMpegTsOutputPlan::create(
    const std::string& videoCodecName,
    const MediaResolvedAudioOutputPlan& audioOutput,
    MediaRunningTime transportDecodeLead)
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
    auto mux = MediaTsMuxPlan::create(MediaTsMuxPlanParameters{
        1, 1, 0x0000, 0x0100, 0x0101, 0x0102, 0x0101, 0,
        MediaRunningTime::fromNanoseconds(100'000'000), 0x1B, 0x0F,
        MediaTsH264InputLayout::LengthPrefixed, 4,
        MediaTsParameterSetPolicy::BeforeRandomAccess,
        MediaTsAacAdtsPlan{0, 2, 3, 2},
        MediaTsOutputClockPolicy{
            MediaRunningTime::fromNanoseconds(20'000'000),
            MediaRunningTime::fromNanoseconds(100'000'000),
            MediaRunningTime::fromNanoseconds(5'000'000), 1, 90'000},
        transportDecodeLead, 188,
        MediaTsContinuitySeeds{0, 0, 0, 0}, 7,
        MediaTsOutputTransportKind::Udp, audioOutput.codecFrameSamples()});
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
