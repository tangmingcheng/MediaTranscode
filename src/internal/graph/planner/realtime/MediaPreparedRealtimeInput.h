#pragma once

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaPreparedGenericInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsPreparedInputBuffer.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"
#include "internal/graph/protocol/mpegts/MediaTsInputSession.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

using MediaRealtimeInputOpener = std::function<::media::Result<::media::ffmpeg::InputFormatContextPtr>(
    const std::string&, AVDictionary**)>;
using MediaTsInputSessionOpener = std::function<
    ::media::Result<std::unique_ptr<MediaTsInputSession>>(
        const MediaTsInputSessionOptions&)>;

struct MediaRealtimePreflightIo final {
    MediaRealtimeInputOpener openGeneric;
    MediaTsInputSessionOpener openMpegTs;
};

enum class MediaPreparedRealtimeInputKind { Generic, MpegTs, RawRtp };

class MediaPreparedRealtimeInput final {
public:
    MediaPreparedRealtimeInput() = default;
    ~MediaPreparedRealtimeInput() = default;

    MediaPreparedRealtimeInput(const MediaPreparedRealtimeInput&) = delete;
    MediaPreparedRealtimeInput& operator=(const MediaPreparedRealtimeInput&) = delete;
    MediaPreparedRealtimeInput(MediaPreparedRealtimeInput&&) noexcept = default;
    MediaPreparedRealtimeInput& operator=(MediaPreparedRealtimeInput&&) noexcept = default;

    static ::media::Result<MediaPreparedRealtimeInput> create(
        ::media::ffmpeg::InputFormatContextPtr context);
    static ::media::Result<MediaPreparedRealtimeInput> create(
        MediaPreparedGenericInput input);
    static ::media::Result<MediaPreparedRealtimeInput> createMpegTs(
        std::unique_ptr<MediaTsInputSession> preflightSession,
        MediaTsRuntimeSessionFactory runtimeSessionFactory);
    static ::media::Result<MediaPreparedRealtimeInput> createRawRtp(
        MediaPreparedRawRtpInput prepared);

    bool valid() const noexcept;
    std::optional<MediaPreparedRealtimeInputKind> kind() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(int streamIndex) const noexcept;
    const MediaPreparedGenericInputPlan* genericPlan() const noexcept;
    const MediaPreparedGenericInputEvidence* genericEvidence() const noexcept;
    const MediaAvSyncStartupPolicy* genericStartup() const noexcept;
    ::media::Status startRawRtpPreflightCapture();
    ::media::Status rawRtpCaptureStatus();
    ::media::Result<MediaRtpIngressObservation> rawRtpIngressObservation();
    ::media::Result<std::size_t> rawRtpPreparedByteCapacity() const;
    ::media::Result<std::size_t>
    rawRtpEffectiveSocketReceivePayloadBytes() const;
    ::media::Status configureRawRtpRuntimeIngress(
        const MediaRtpIngressPlan& plan);
    ::media::Status sealRawRtpPreflight();
    ::media::Result<MediaBufferRef> releaseBuffer();

private:
    explicit MediaPreparedRealtimeInput(std::unique_ptr<FFmpegFormatContextBuffer> buffer);
    explicit MediaPreparedRealtimeInput(std::unique_ptr<MediaPreparedGenericInputBuffer> buffer);
    explicit MediaPreparedRealtimeInput(std::unique_ptr<MediaTsPreparedInputBuffer> buffer);
    explicit MediaPreparedRealtimeInput(
        std::unique_ptr<MediaRawRtpPreparedInputBuffer> buffer);

    std::unique_ptr<FFmpegFormatContextBuffer> m_genericBuffer;
    std::unique_ptr<MediaPreparedGenericInputBuffer> m_preparedGenericBuffer;
    std::unique_ptr<MediaTsPreparedInputBuffer> m_tsBuffer;
    std::unique_ptr<MediaRawRtpPreparedInputBuffer> m_rawRtpBuffer;
};

} // namespace media::ffmpeg::graph
