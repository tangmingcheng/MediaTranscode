#include "internal/graph/planner/realtime/MediaPreparedRtpEmissionPlanner.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/protocol/rtp/MediaRtcpWireGeometry.h"
#include <new>

namespace media::ffmpeg::graph {
::media::Result<MediaRtpAccessUnitEmissionContract> MediaPreparedRtpEmissionPlanner::video(
    const MediaPreparedEncoderEmissionEnvelope& emission,
    const FFmpegCodecParametersSnapshot& parameters,
    std::size_t maximumDatagramBytes)
{
    using Result = ::media::Result<MediaRtpAccessUnitEmissionContract>;
    if (!parameters.complete() || !emission.encodedPacketLayout ||
        emission.encodedPacketLayout->kind() != MediaEncodedPacketLayoutKind::StartCodeDelimited ||
        emission.authority.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "RTP emission requires opened video codec and start-code layout facts"));
    }
    MediaScheduledRtpPacketizationMode mode;
    switch (parameters.parameters()->codec_id) {
    case AV_CODEC_ID_H264: mode = MediaScheduledRtpPacketizationMode::H264AnnexB; break;
    case AV_CODEC_ID_HEVC: mode = MediaScheduledRtpPacketizationMode::HevcAnnexB; break;
    default: return Result::failure(::media::ErrorInfo::unsupported(
        "RTP emission requires an opened H.264 or HEVC encoder"));
    }
    return MediaRtpAccessUnitEmissionContract::createVideo(mode,
        *emission.encodedPacketLayout, emission.maximumAccessUnitPayloadBytes,
        maximumDatagramBytes, emission.authority + "+deterministic-rfc-packetizer");
}

::media::Result<MediaRtpAccessUnitEmissionContract> MediaPreparedRtpEmissionPlanner::audio(
    const MediaPreparedAudioEncoderEmissionEnvelope& emission,
    std::size_t maximumDatagramBytes)
{
    if (emission.codecId != AV_CODEC_ID_AAC) {
        return ::media::Result<MediaRtpAccessUnitEmissionContract>::failure(
            ::media::ErrorInfo::unsupported(
                "AAC LATM packetization requires opened AAC encoder readback"));
    }
    if (emission.authority.empty()) {
        return ::media::Result<MediaRtpAccessUnitEmissionContract>::failure(
            ::media::ErrorInfo::notInitialized("RTP audio emission requires prepared authority"));
    }
    return MediaRtpAccessUnitEmissionContract::createAacLatm(
        emission.maximumAccessUnitPayloadBytes, maximumDatagramBytes,
        emission.authority + "+ffmpeg-rtpenc-latm-contract");
}
::media::Result<std::vector<MediaRawRtpProducerPlan>> MediaPreparedRtpEmissionPlanner::producers(
    MediaDatagramProtocolLayout layout, const MediaPreparedRealtimeEmissionSet& emission,
    std::size_t maximumDatagramBytes, std::string_view cname)
{
    using Result = ::media::Result<std::vector<MediaRawRtpProducerPlan>>;
    auto topology = mediaDatagramProtocolTopology(layout);
    if (!topology) return Result::failure(topology.error());
    if (layout == MediaDatagramProtocolLayout::MpegTsRtp ||
        layout == MediaDatagramProtocolLayout::MpegTsUdp) return Result::success({});
    if (!emission.videoCodecParameters ||
        emission.audio.has_value() != (layout == MediaDatagramProtocolLayout::SeparateAvRtp))
        return Result::failure(::media::ErrorInfo::notInitialized(
            "Raw RTP producer planning requires matching prepared streams"));
    auto videoContract = video(emission.video, *emission.videoCodecParameters, maximumDatagramBytes);
    auto compound = MediaRtcpWireGeometry::compoundPayloadBytes(cname.size());
    if (!videoContract || !compound)
        return Result::failure(!videoContract ? videoContract.error() : compound.error());
    try {
        std::vector<MediaRawRtpProducerPlan> result;
        result.reserve(topology.value().rtpInstances.size());
        result.push_back({MediaDatagramProtocolEndpointRole::VideoRtp,
            std::move(videoContract).value(), compound.value()});
        if (emission.audio) {
            auto audioContract = audio(*emission.audio, maximumDatagramBytes);
            if (!audioContract) return Result::failure(audioContract.error());
            result.push_back({MediaDatagramProtocolEndpointRole::AudioRtp,
                std::move(audioContract).value(), compound.value()});
        }
        auto valid = validateRawRtpProducerPlans(layout, result);
        if (!valid) return Result::failure(valid.error());
        return Result::success(std::move(result));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("Raw RTP producer facts"));
    }
}
} // namespace media::ffmpeg::graph
