#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadReservation.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaTsInitialPacketRetentionLimit final {
    std::size_t packetCapacity = 0;
    std::uint64_t byteCapacity = 0;
    std::uint64_t maximumPacketBytes = 0;
};

struct MediaTsVideoOnlyPacketRetentionPlan final {
    MediaTsInitialPacketRetentionLimit video;
};

struct MediaTsAudioVideoPacketRetentionPlan final {
    MediaTsInitialPacketRetentionLimit video;
    MediaTsInitialPacketRetentionLimit audio;
};

using MediaTsInitialPacketRetentionPlan = std::variant<
    MediaTsVideoOnlyPacketRetentionPlan,
    MediaTsAudioVideoPacketRetentionPlan>;

struct MediaTsInitialAcquiringPacket final {
    ::media::ffmpeg::PacketPtr packet;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaGraphPayloadReservation reservation;
};

struct MediaTsInitialReplayPacket final {
    MediaBufferRef buffer;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
};

class MediaTsInitialAcquiringPacketBuffer final {
public:
    static ::media::Result<MediaTsInitialAcquiringPacketBuffer> create(
        MediaTsInitialPacketRetentionPlan plan);

    ::media::Status retain(
        ::media::ffmpeg::PacketPtr packet,
        MediaStreamKind streamKind,
        MediaGraphPayloadReservation reservation);
    using Materializer = std::function<::media::Result<MediaBufferRef>(
        const AVPacket&, MediaStreamKind,
        const MediaGraphPayloadReservation&)>;
    ::media::Status stageReplay(const AVPacket& current,
                                MediaStreamKind streamKind,
                                MediaGraphPayloadReservation currentReservation,
                                const Materializer& materializer);
    ::media::Status stageSingleReplay(MediaBufferRef buffer,
                                      MediaStreamKind streamKind);
    bool hasReplay() const noexcept;
    const MediaTsInitialReplayPacket& nextReplay() const;
    void popReplay() noexcept;
    bool empty() const noexcept;
    void clear() noexcept;

private:
    struct Usage final {
        std::size_t packets = 0;
        std::uint64_t bytes = 0;
    };

    MediaTsInitialAcquiringPacketBuffer(
        MediaTsInitialPacketRetentionPlan plan) noexcept;

    MediaTsInitialPacketRetentionPlan m_plan;
    Usage m_videoUsage;
    Usage m_audioUsage;
    std::deque<MediaTsInitialAcquiringPacket> m_packets;
    std::deque<MediaTsInitialReplayPacket> m_replay;
};

} // namespace media::ffmpeg::graph
