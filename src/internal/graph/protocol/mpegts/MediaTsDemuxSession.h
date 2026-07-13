#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"

extern "C" {
#include <libavcodec/packet.h>
}

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaTsReadFrameState { Frame, Waiting, EndOfStream };

struct MediaTsInputRuntimeContract final {
    std::size_t packetStride;
    std::size_t evidenceCapacity;
    std::uint64_t maximumPositionRegressionBytes;
};

class MediaTsDemuxSession {
public:
    virtual ~MediaTsDemuxSession() = default;
    virtual ::media::Result<MediaTsReadFrameState> readFrame(AVPacket& packet) = 0;
    virtual ::media::Status close() noexcept = 0;
    virtual void cancel() noexcept = 0;
    virtual const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept = 0;
    virtual const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept = 0;
    virtual ::media::Result<std::vector<FFmpegInputStreamSnapshot>> cloneStreamSnapshots() const = 0;
    virtual MediaTsProgramInventorySnapshot programInventory() const = 0;
    virtual const MediaTsInputRuntimeContract& runtimeContract() const noexcept = 0;
    virtual ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> evidenceSnapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const = 0;
    virtual ::media::Status observePacketPosition(std::uint64_t packetPosition) = 0;
};

} // namespace media::ffmpeg::graph
