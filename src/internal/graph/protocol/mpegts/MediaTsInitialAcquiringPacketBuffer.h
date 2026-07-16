#pragma once

#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

namespace media::ffmpeg::graph {

struct MediaTsInitialPacketRetentionLimit final {
    std::size_t packetCapacity = 0;
    std::uint64_t byteCapacity = 0;
    std::uint64_t maximumPacketBytes = 0;
};

struct MediaTsInitialAcquiringPacket final {
    ::media::ffmpeg::PacketPtr packet;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
};

struct MediaTsInitialReplayPacket final {
    MediaBufferRef buffer;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
};

class MediaTsInitialAcquiringPacketBuffer final {
public:
    static ::media::Result<MediaTsInitialAcquiringPacketBuffer> create(
        MediaTsInitialPacketRetentionLimit video,
        MediaTsInitialPacketRetentionLimit audio);

    ::media::Status retain(::media::ffmpeg::PacketPtr packet,
                           MediaStreamKind streamKind);
    using Materializer = std::function<::media::Result<MediaBufferRef>(
        const AVPacket&, MediaStreamKind)>;
    ::media::Status stageReplay(const AVPacket& current,
                                MediaStreamKind streamKind,
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
        MediaTsInitialPacketRetentionLimit video,
        MediaTsInitialPacketRetentionLimit audio) noexcept;

    MediaTsInitialPacketRetentionLimit m_videoLimit;
    MediaTsInitialPacketRetentionLimit m_audioLimit;
    Usage m_videoUsage;
    Usage m_audioUsage;
    std::deque<MediaTsInitialAcquiringPacket> m_packets;
    std::deque<MediaTsInitialReplayPacket> m_replay;
};

} // namespace media::ffmpeg::graph
