#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>

namespace media::ffmpeg::graph {

struct MediaTsInputSessionOptions final {
    std::string protocolUrl;
    AVDictionary* protocolOptions = nullptr;
    AVDictionary* demuxOptions = nullptr;
    std::size_t avioBufferBytes = 0;
    std::size_t packetStride = 188;
    std::size_t evidenceCapacity = 0;
    std::uint64_t maximumPositionRegressionBytes = 0;
};

enum class MediaTsReadFrameState { Frame, Waiting, EndOfStream };

class MediaTsInputSession final {
public:
    static ::media::Result<std::unique_ptr<MediaTsInputSession>> open(
        const MediaTsInputSessionOptions& options);
    static ::media::Result<std::unique_ptr<MediaTsInputSession>> open(
        const MediaTsInputSessionOptions& options,
        FFmpegProtocolAvioOpener& opener);

    ~MediaTsInputSession();
    MediaTsInputSession(const MediaTsInputSession&) = delete;
    MediaTsInputSession& operator=(const MediaTsInputSession&) = delete;
    MediaTsInputSession(MediaTsInputSession&&) = delete;
    MediaTsInputSession& operator=(MediaTsInputSession&&) = delete;

    ::media::Result<MediaTsReadFrameState> readFrame(AVPacket& packet);
    ::media::Status close() noexcept;
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept;
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept;
    ::media::Result<std::vector<FFmpegInputStreamSnapshot>> cloneStreamSnapshots() const;
    MediaTsProgramInventorySnapshot programInventory() const;
    ::media::Result<MediaTsEvidenceCheckpoint> evidenceAtOrBefore(
        std::uint64_t packetPosition) const;
    ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> evidenceSnapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const;
    FFmpegAvioInterruptState& interruptState() noexcept { return m_interruptState; }
    ::media::Status status() const;

private:
    class EvidenceObserver;
    class ReadLease;
    MediaTsInputSession() = default;
    static ::media::Result<std::unique_ptr<MediaTsInputSession>> openWithOpener(
        const MediaTsInputSessionOptions& options,
        FFmpegProtocolAvioOpener* opener);
    ::media::Status buildStreamSnapshots();

    FFmpegAvioInterruptState m_interruptState;
    std::unique_ptr<EvidenceObserver> m_evidenceObserver;
    std::unique_ptr<FFmpegObservedReadAvio> m_observedAvio;
    AVFormatContext* m_formatContext = nullptr;
    std::vector<FFmpegInputStreamSnapshot> m_streamSnapshots;
    std::vector<FFmpegInputProgramSnapshot> m_programSnapshots;
    mutable std::mutex m_sessionMutex;
    std::condition_variable m_readsDone;
    std::size_t m_activeReads = 0;
    bool m_closing = false;
    bool m_closed = false;
    std::optional<::media::ErrorInfo> m_finalError;
};

} // namespace media::ffmpeg::graph
