#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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

    AVFormatContext* formatContext() noexcept { return m_formatContext; }
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept;
    ::media::Result<std::vector<FFmpegInputStreamSnapshot>> cloneStreamSnapshots() const;
    const MediaTsProgramInventorySnapshot& programInventory() const noexcept;
    const MediaTsEvidenceTimeline& evidenceTimeline() const noexcept;
    FFmpegAvioInterruptState& interruptState() noexcept { return m_interruptState; }
    ::media::Status status() const;

private:
    class EvidenceObserver;
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
};

} // namespace media::ffmpeg::graph
