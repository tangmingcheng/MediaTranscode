#pragma once

#include "internal/graph/planner/realtime/MediaPreparedGenericInputPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"
#include "internal/graph/runtime/buffer/MediaDemuxInputBuffer.h"
#include "media_transcode/Result.h"

#include <vector>
#include <memory>

namespace media::ffmpeg::graph {

class MediaPreparedGenericInput final {
public:
    MediaPreparedGenericInput() = delete;
    MediaPreparedGenericInput(const MediaPreparedGenericInput&) = delete;
    MediaPreparedGenericInput& operator=(const MediaPreparedGenericInput&) = delete;
    MediaPreparedGenericInput(MediaPreparedGenericInput&&) noexcept;
    MediaPreparedGenericInput& operator=(MediaPreparedGenericInput&&) noexcept;
    ~MediaPreparedGenericInput();

    static ::media::Result<MediaPreparedGenericInput> prepare(
        ::media::ffmpeg::InputFormatContextPtr context,
        MediaPreparedGenericInputPlan plan,
        MediaAvSyncStartupPolicy startup);

    const MediaPreparedGenericInputPlan& plan() const noexcept { return m_plan; }
    const MediaPreparedGenericInputEvidence& evidence() const noexcept { return m_evidence; }
    const MediaAvSyncStartupPolicy& startup() const noexcept { return m_startup; }
    const std::vector<FFmpegInputStreamSnapshot>& snapshots() const noexcept { return m_snapshots; }
    ::media::Result<MediaDemuxInputSession> takeSession();

private:
    struct CaptureState;

    MediaPreparedGenericInput(
        std::unique_ptr<CaptureState> capture,
        std::vector<FFmpegInputStreamSnapshot> snapshots,
        MediaPreparedGenericInputPlan plan,
        MediaPreparedGenericInputEvidence evidence,
        MediaAvSyncStartupPolicy startup) noexcept;

    std::unique_ptr<CaptureState> m_capture;
    std::vector<FFmpegInputStreamSnapshot> m_snapshots;
    MediaPreparedGenericInputPlan m_plan;
    MediaPreparedGenericInputEvidence m_evidence;
    MediaAvSyncStartupPolicy m_startup;
};

} // namespace media::ffmpeg::graph
