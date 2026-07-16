#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsDemuxSession.h"
#include "internal/graph/protocol/mpegts/MediaTsPublicProgramSnapshot.h"
#include "internal/graph/protocol/mpegts/MediaTsPesProvenanceTimeline.h"
#include "internal/graph/protocol/mpegts/MediaTsPreflightDurationProbe.h"
#include "internal/graph/protocol/mpegts/MediaTsReturnedPesCursor.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegObservedReadAvio.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"

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
    std::size_t packetStride = 0;
    std::size_t evidenceCapacity = 0;
    std::size_t pesProvenanceCapacity = 0;
    std::uint64_t maximumPositionRegressionBytes = 0;
};

class MediaTsInputSession final : public MediaTsDemuxSession {
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

    ::media::Result<MediaTsReadFrameEnvelope> readFrame() override;
    ::media::Result<MediaTsSelectedPacketDurationEvidence>
    probeSelectedPacketDurations(std::size_t frameLimit);
    ::media::Status configureRuntimeBinding(const MediaTsRuntimeBinding& binding);
    ::media::Status close() noexcept override;
    void cancel() noexcept override { m_interruptState.cancel(); }
    const std::vector<FFmpegInputStreamSnapshot>& streamSnapshots() const noexcept override;
    const std::vector<FFmpegInputProgramSnapshot>& programSnapshots() const noexcept override;
    ::media::Result<std::vector<FFmpegInputStreamSnapshot>> cloneStreamSnapshots() const override;
    MediaTsProgramInventorySnapshot programInventory() const override;
    const MediaTsInputRuntimeContract& runtimeContract() const noexcept override;
    ::media::Result<MediaTsEvidenceCheckpoint> evidenceAtOrBefore(
        std::uint64_t packetPosition) const;
    ::media::Result<std::vector<MediaTsEvidenceCheckpoint>> evidenceSnapshotAfter(
        std::optional<std::uint64_t> exclusiveOffset) const override;
    ::media::Status observePacketPosition(std::uint64_t packetPosition) override;
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
    ::media::Result<MediaTsReadFrameEnvelope> readFrameFromSource();
    ::media::Result<MediaTsPacketProvenance> provenanceFor(const AVPacket& packet);

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
    MediaTsInputRuntimeContract m_runtimeContract{};
    std::unique_ptr<MediaTsReturnedPesCursor> m_returnedPesCursor;
    std::optional<MediaTsPreflightDurationProbe> m_durationProbe;
    bool m_runtimeBindingConfigured = false;
};

} // namespace media::ffmpeg::graph
