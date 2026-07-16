#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"
#include "internal/graph/protocol/mpegts/MediaTsInitialAcquiringPacketBuffer.h"
#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"
#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MpegTsDemuxNode final : public FFmpegNodeRuntime {
public:
    explicit MpegTsDemuxNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;
    void interrupt(MediaGraphExecutionContext& context) noexcept override;

private:
    struct StreamClock {
        std::map<std::uint64_t, MediaTsSourceClockMapper> mappers;
        void discardBefore(std::uint64_t generation);
    };

    ::media::Status bind(MediaGraphExecutionContext& context);
    ::media::Status rejectDuplicateBinding(MediaGraphExecutionContext& context);
    ::media::Result<MediaPacketSourceTiming> timingFor(
        const AVPacket& packet, const MediaTsClockProjectionCheckpoint& checkpoint,
        StreamClock& clock);
    ::media::Result<MediaTsClockProjectionCheckpoint> sourceClockCheckpoint(
        std::uint64_t packetPosition);
    ::media::Status enqueueLockedPacket(
        ::media::ffmpeg::PacketPtr packet, MediaStreamKind streamKind,
        const MediaTsClockProjectionCheckpoint& checkpoint);
    ::media::Status prepareFirstLockedBatch(
        ::media::ffmpeg::PacketPtr packet, MediaStreamKind streamKind,
        const MediaTsClockProjectionCheckpoint& checkpoint);
    ::media::Result<MediaNodeProcessResult> emitReadyPacket(
        MediaGraphExecutionContext& context);
    ::media::Status emitEof(MediaGraphExecutionContext& context);
    void reset() noexcept;

    std::unique_ptr<MediaTsDemuxSession> m_session;
    std::optional<MediaTsClockProjection> m_projection;
    std::optional<MediaTsProgramClockPolicy> m_policy;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;
    std::uint64_t m_initialSourceGeneration = 0;
    StreamClock m_videoClock;
    StreamClock m_audioClock;
    std::optional<MediaTsInitialAcquiringPacketBuffer> m_acquiringPackets;
    bool m_initialClockLocked = false;
    bool m_eofSent = false;
    std::atomic_bool m_aborted{false};
};

} // namespace media::ffmpeg::graph
