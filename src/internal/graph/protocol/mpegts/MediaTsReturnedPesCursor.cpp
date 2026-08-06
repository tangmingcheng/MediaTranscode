#include "internal/graph/protocol/mpegts/MediaTsReturnedPesCursor.h"

#include <type_traits>

namespace media::ffmpeg::graph {

::media::Result<MediaTsReturnedPesCursor> MediaTsReturnedPesCursor::create(
    const MediaTsRuntimeBinding& binding)
{
    const std::size_t capacity = std::visit(
        [](const auto& selected) {
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsVideoOnlyRuntimeBinding>) {
                return selected.videoPesProvenanceCapacity;
            } else {
                return selected.pesProvenanceCapacity;
            }
        },
        binding);
    if (!MediaTsRuntimeBindingCodec::validate(binding, capacity)) {
        return ::media::Result<MediaTsReturnedPesCursor>::failure(
            ::media::ErrorInfo::invalidArgument(
                "invalid MPEG-TS returned PES cursor binding"));
    }
    MediaTsReturnedPesCursor result;
    std::visit(
        [&result](const auto& selected) {
            result.m_streams.emplace(
                selected.video.streamIndex,
                StreamCursor{selected.video.pid, std::nullopt});
            using Binding = std::decay_t<decltype(selected)>;
            if constexpr (std::is_same_v<
                              Binding,
                              MediaTsAudioVideoRuntimeBinding>) {
                result.m_streams.emplace(
                    selected.audio.streamIndex,
                    StreamCursor{selected.audio.pid, std::nullopt});
            }
        },
        binding);
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
