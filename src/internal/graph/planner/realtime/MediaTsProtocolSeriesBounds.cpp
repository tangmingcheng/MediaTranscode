#include "internal/graph/planner/realtime/MediaTsProtocolSeriesBounds.h"
#include "internal/graph/protocol/mpegts/MediaTsVideoAccessUnitFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsAacAdtsFramer.h"
#include "internal/graph/protocol/mpegts/MediaTsPesSerializer.h"
#include "internal/graph/protocol/mpegts/MediaTsTransportPacketBuilder.h"
#include "internal/graph/protocol/mpegts/MediaTsPsiSerializer.h"
#include <limits>
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include <new>
#include "internal/graph/nodes/mux/MediaTsFfmpegStreamConfigMaterializer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersSnapshot.h"

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaTsMediaSeriesBound> series(std::size_t framedBytes,
    std::size_t packetsPerDatagram, bool randomAccess)
{
    using Result = ::media::Result<MediaTsMediaSeriesBound>;
    // A series must also fit after a generation transition and with both timestamps.
    const auto header = MediaTsPesSerializer::headerBytes(true);
    if (!framedBytes || !packetsPerDatagram ||
        framedBytes > (std::numeric_limits<std::size_t>::max)() - header)
        return Result::failure(::media::ErrorInfo::invalidArgument("TS series geometry is not representable"));
    auto packets = MediaTsTransportPacketBuilder::payloadPacketCount(
        framedBytes + header, randomAccess, true);
    if (!packets) return Result::failure(packets.error());
    if (packets.value() > (std::numeric_limits<std::size_t>::max)() /
            sizeof(std::array<std::uint8_t, 188>))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS packet backing size is not representable"));
    const auto datagrams = packets.value() / packetsPerDatagram +
        (packets.value() % packetsPerDatagram != 0);
    return Result::success({framedBytes, packets.value(), datagrams});
}
using MaintenanceResult = ::media::Result<std::array<std::size_t, 3>>;
MaintenanceResult maintenance(const MediaTsMuxPlan& mux)
{
    auto tables = MediaTsPsiSerializer::serialize(mux);
    if (!tables) return MaintenanceResult::failure(tables.error());
    // Include the first series after a generation transition.
    const std::array<std::uint8_t, 1> pointerField{0};
    const std::array<std::span<const std::uint8_t>, 2> patSegments{
        pointerField, tables.value().pat().bytes()};
    const std::array<std::span<const std::uint8_t>, 2> pmtSegments{
        pointerField, tables.value().pmt().bytes()};
    auto pat = MediaTsTransportPacketBuilder::payloadPacketCount(patSegments, false, true);
    auto pmt = MediaTsTransportPacketBuilder::payloadPacketCount(pmtSegments, false, true);
    if (!pat || !pmt) return MaintenanceResult::failure(!pat ? pat.error() : pmt.error());
    return MaintenanceResult::success({pat.value(), pmt.value(),
        MediaTsTransportPacketBuilder::PcrOnlyPacketCount});
}

}
::media::Result<MediaTsProtocolSeriesBounds> MediaTsProtocolSeriesBoundsPlanner::plan(
    const MediaTsMuxPlan& mux, const MediaPreparedRealtimeEmissionSet& emission)
try {
    using Result = ::media::Result<MediaTsProtocolSeriesBounds>;
    if (!emission.videoCodecParameters || !emission.videoCodecParameters->complete())
        return Result::failure(::media::ErrorInfo::notInitialized("TS series requires prepared codec parameters"));
    auto materialized = MediaTsFfmpegStreamConfigMaterializer::video(mux, *emission.videoCodecParameters->parameters());
    if (!materialized) return Result::failure(materialized.error());
    const auto& video = materialized.value();
    const auto& videoEmission = emission.video;
    const auto& audioEmission = emission.audio;
    const auto* audio = mux.audioVideoProgram();
    if (video.contract() != mux.parameters().video || videoEmission.authority.empty() ||
        !videoEmission.maximumAccessUnitPayloadBytes ||
        videoEmission.maximumAccessUnitPayloadBytes > (std::numeric_limits<std::size_t>::max)() ||
        (audio != nullptr) != audioEmission.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument("TS series requires matching prepared emission facts"));
    auto framed = MediaTsVideoAccessUnitFramer::maximumFramedPayloadBytes(video,
        static_cast<std::size_t>(videoEmission.maximumAccessUnitPayloadBytes),
        mux.parameters().parameterSetPolicy == MediaTsParameterSetPolicy::BeforeRandomAccess);
    if (!framed) return Result::failure(framed.error());
    auto videoBound = series(framed.value(), mux.parameters().maximumPacketsPerDatagram, true);
    if (!videoBound) return Result::failure(videoBound.error());
    std::optional<MediaTsMediaSeriesBound> audioBound;
    if (audioEmission) {
        if (audioEmission->codecId != AV_CODEC_ID_AAC || audioEmission->authority.empty() ||
            !audioEmission->maximumAccessUnitPayloadBytes ||
            audioEmission->maximumAccessUnitPayloadBytes > (std::numeric_limits<std::size_t>::max)())
            return Result::failure(::media::ErrorInfo::invalidArgument("TS audio series lacks prepared size authority"));
        auto audioFramed = MediaTsAacAdtsFramer::maximumFramedPayloadBytes(audio->aac,
            static_cast<std::size_t>(audioEmission->maximumAccessUnitPayloadBytes));
        if (!audioFramed) return Result::failure(audioFramed.error());
        auto bound = series(audioFramed.value(), mux.parameters().maximumPacketsPerDatagram, false);
        if (!bound) return Result::failure(bound.error());
        audioBound = bound.value();
    }
    auto overhead = maintenance(mux);
    if (!overhead) return Result::failure(overhead.error());
    return Result::success({videoBound.value(), audioBound, overhead.value()[0], overhead.value()[1], overhead.value()[2]});
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaTsProtocolSeriesBounds>::failure(
        ::media::ErrorInfo::allocationFailed("TS protocol series planning"));
}
::media::Status MediaTsProtocolSeriesBoundsPlanner::validate(
    const MediaTsMuxPlan& mux, const MediaTsProtocolSeriesBounds& bounds)
try {
    if ((mux.audioVideoProgram() != nullptr) != bounds.audio.has_value())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS series audio presence differs from program"));
    auto video = series(bounds.video.maximumFramedPayloadBytes, mux.parameters().maximumPacketsPerDatagram, true);
    if (!video) return ::media::Status::failure(video.error());
    if (video.value() != bounds.video)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS video series geometry differs from packetizer"));
    if (bounds.audio) {
        auto audio = series(bounds.audio->maximumFramedPayloadBytes, mux.parameters().maximumPacketsPerDatagram, false);
        if (!audio) return ::media::Status::failure(audio.error());
        if (audio.value() != *bounds.audio)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS audio series geometry differs from packetizer"));
    }
    auto overhead = maintenance(mux);
    if (!overhead) return ::media::Status::failure(overhead.error());
    if (overhead.value() != std::array<std::size_t, 3>{bounds.maximumPatPackets,
            bounds.maximumPmtPackets, bounds.maximumPcrPackets})
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("TS maintenance geometry differs from packetizer"));
    return ::media::Status::success();
} catch (const std::bad_alloc&) {
    return ::media::Status::failure(::media::ErrorInfo::allocationFailed("TS series validation"));
}
} // namespace media::ffmpeg::graph
