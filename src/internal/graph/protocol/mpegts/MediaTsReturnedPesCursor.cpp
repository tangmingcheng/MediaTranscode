#include "internal/graph/protocol/mpegts/MediaTsReturnedPesCursor.h"

namespace media::ffmpeg::graph {

::media::Result<MediaTsReturnedPesCursor> MediaTsReturnedPesCursor::create(
    const MediaTsRuntimeBinding& binding)
{
    if (binding.originPolicy != MediaTsPacketOriginPolicy::PerStreamPesCarry ||
        binding.video.streamIndex < 0 || binding.audio.streamIndex < 0 ||
        binding.video.streamIndex == binding.audio.streamIndex ||
        binding.video.pid == 0 || binding.audio.pid == 0 ||
        binding.video.pid == binding.audio.pid ||
        binding.pcrPid == 0 || binding.pcrPid >= 0x1FFF ||
        binding.pesProvenanceCapacity == 0) {
        return ::media::Result<MediaTsReturnedPesCursor>::failure(
            ::media::ErrorInfo::invalidArgument(
                "invalid MPEG-TS returned PES cursor binding"));
    }
    MediaTsReturnedPesCursor result;
    result.m_streams.emplace(
        binding.video.streamIndex, StreamCursor{binding.video.pid, std::nullopt});
    result.m_streams.emplace(
        binding.audio.streamIndex, StreamCursor{binding.audio.pid, std::nullopt});
    return ::media::Result<MediaTsReturnedPesCursor>::success(std::move(result));
}

::media::Result<MediaTsPacketProvenance> MediaTsReturnedPesCursor::resolve(
    int streamIndex,
    std::int64_t packetPosition,
    const AnchorResolver& resolveAnchor,
    const StateResolver& resolveState)
{
    auto stream = m_streams.find(streamIndex);
    if (stream == m_streams.end()) {
        return ::media::Result<MediaTsPacketProvenance>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS packet stream is outside the planned PES binding"));
    }
    if (packetPosition >= 0) {
        auto anchor = resolveAnchor(
            static_cast<std::uint64_t>(packetPosition), stream->second.pid);
        if (!anchor) {
            return ::media::Result<MediaTsPacketProvenance>::failure(anchor.error());
        }
        stream->second.active = anchor.value();
    } else if (packetPosition != -1) {
        return ::media::Result<MediaTsPacketProvenance>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS packet position is neither an anchor nor a PES continuation"));
    }
    if (packetPosition == -1 && stream->second.active) {
        auto state = resolveState(*stream->second.active);
        if (!state) {
            return ::media::Result<MediaTsPacketProvenance>::failure(state.error());
        }
        stream->second.active = state.value();
    }
    if (!stream->second.active) {
        return ::media::Result<MediaTsPacketProvenance>::success(
            MediaTsPacketProvenance{
                std::nullopt, std::nullopt,
                MediaSourceClockReadiness::Acquiring});
    }
    const auto& active = *stream->second.active;
    if (active.validity == MediaTsPesProvenanceValidity::Invalid) {
        return ::media::Result<MediaTsPacketProvenance>::success(
            MediaTsPacketProvenance{
                active.stateEvidenceByteOffset, std::nullopt,
                MediaSourceClockReadiness::ReacquireRequired});
    }
    return ::media::Result<MediaTsPacketProvenance>::success(
        MediaTsPacketProvenance{
            active.stateEvidenceByteOffset, active.originByteOffset,
            MediaSourceClockReadiness::Locked});
}

} // namespace media::ffmpeg::graph
