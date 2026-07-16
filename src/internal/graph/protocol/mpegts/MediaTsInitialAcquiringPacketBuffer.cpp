#include "internal/graph/protocol/mpegts/MediaTsInitialAcquiringPacketBuffer.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <utility>

namespace media::ffmpeg::graph {

MediaTsInitialAcquiringPacketBuffer::MediaTsInitialAcquiringPacketBuffer(
    MediaTsInitialPacketRetentionLimit video,
    MediaTsInitialPacketRetentionLimit audio) noexcept
    : m_videoLimit(video), m_audioLimit(audio)
{
}

::media::Result<MediaTsInitialAcquiringPacketBuffer>
MediaTsInitialAcquiringPacketBuffer::create(
    MediaTsInitialPacketRetentionLimit video,
    MediaTsInitialPacketRetentionLimit audio)
{
    const auto valid = [](const MediaTsInitialPacketRetentionLimit& limit) {
        return limit.packetCapacity > 0 && limit.byteCapacity > 0 &&
               limit.maximumPacketBytes > 0 &&
               limit.maximumPacketBytes <= limit.byteCapacity;
    };
    if (!valid(video) || !valid(audio)) {
        return ::media::Result<MediaTsInitialAcquiringPacketBuffer>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Invalid MPEG-TS initial acquiring packet retention limits"));
    }
    return ::media::Result<MediaTsInitialAcquiringPacketBuffer>::success(
        MediaTsInitialAcquiringPacketBuffer(video, audio));
}

::media::Status MediaTsInitialAcquiringPacketBuffer::retain(
    ::media::ffmpeg::PacketPtr packet,
    MediaStreamKind streamKind)
{
    if (!packet ||
        (packet->pts == AV_NOPTS_VALUE && packet->dts == AV_NOPTS_VALUE) ||
        packet->duration <= 0 ||
        packet->time_base.num <= 0 || packet->time_base.den <= 0 ||
        packet->size < 0 ||
        (streamKind != MediaStreamKind::Video &&
         streamKind != MediaStreamKind::Audio)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS acquiring packet lacks replayable timing evidence"));
    }
    const auto bytes = static_cast<std::uint64_t>(packet->size);
    auto& usage = streamKind == MediaStreamKind::Video
        ? m_videoUsage : m_audioUsage;
    const auto& limit = streamKind == MediaStreamKind::Video
        ? m_videoLimit : m_audioLimit;
    if (usage.packets >= limit.packetCapacity ||
        bytes > limit.maximumPacketBytes ||
        bytes > limit.byteCapacity - usage.bytes) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS initial acquiring packet retention capacity exhausted"));
    }
    m_packets.push_back(
        MediaTsInitialAcquiringPacket{std::move(packet), streamKind});
    ++usage.packets;
    usage.bytes += bytes;
    return ::media::Status::success();
}

::media::Status MediaTsInitialAcquiringPacketBuffer::stageReplay(
    const AVPacket& current,
    MediaStreamKind streamKind,
    const Materializer& materializer)
{
    if (!materializer || hasReplay()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS initial packet replay staging state is invalid"));
    }
    std::deque<MediaTsInitialReplayPacket> staged;
    const auto append = [&](const AVPacket& packet,
                            MediaStreamKind kind) -> ::media::Status {
        auto materialized = materializer(packet, kind);
        if (!materialized) {
            return ::media::Status::failure(materialized.error());
        }
        staged.push_back(MediaTsInitialReplayPacket{
            std::move(materialized).value(), kind});
        return ::media::Status::success();
    };
    for (const auto& retained : m_packets) {
        if (auto status = append(*retained.packet, retained.streamKind); !status) {
            return status;
        }
    }
    if (auto status = append(current, streamKind); !status) return status;
    m_replay = std::move(staged);
    clear();
    return ::media::Status::success();
}

::media::Status MediaTsInitialAcquiringPacketBuffer::stageSingleReplay(
    MediaBufferRef buffer,
    MediaStreamKind streamKind)
{
    if (!buffer || !m_packets.empty() || hasReplay() ||
        (streamKind != MediaStreamKind::Video &&
         streamKind != MediaStreamKind::Audio)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS single packet replay staging state is invalid"));
    }
    m_replay.push_back(MediaTsInitialReplayPacket{
        std::move(buffer), streamKind});
    return ::media::Status::success();
}

bool MediaTsInitialAcquiringPacketBuffer::hasReplay() const noexcept
{
    return !m_replay.empty();
}

const MediaTsInitialReplayPacket&
MediaTsInitialAcquiringPacketBuffer::nextReplay() const
{
    return m_replay.front();
}

void MediaTsInitialAcquiringPacketBuffer::popReplay() noexcept
{
    m_replay.pop_front();
}

bool MediaTsInitialAcquiringPacketBuffer::empty() const noexcept
{
    return m_packets.empty();
}

void MediaTsInitialAcquiringPacketBuffer::clear() noexcept
{
    m_packets.clear();
    m_videoUsage = {};
    m_audioUsage = {};
}

} // namespace media::ffmpeg::graph
