#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"
#include <mutex>
#include <optional>
#include "internal/graph/planner/capability/MediaVideoEncoderReadback.h"
#include "internal/graph/runtime/buffer/MediaPreparedVideoDecoder.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace media::ffmpeg::graph {

struct FFmpegInputStreamSnapshot;

class CodecResolverNode final : public FFmpegNodeRuntime {
public:
    explicit CodecResolverNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;
    ::media::Status bindPreparedEncoder(MediaBufferRef encoder);
    ::media::Status bindPreparedDecoder(std::shared_ptr<MediaPreparedVideoDecoder> decoder);
    MediaBufferRef inputSnapshot() const;
    ::media::Result<MediaVideoEncoderReadback> encoderReadback() const;
    MediaBufferRef timestampSource() const;
    MediaBufferRef encoderParametersSnapshot() const;
    ::media::Result<MediaDecoderRuntimeFacts> decoderRuntimeFacts() const;

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(MediaGraphExecutionContext& context) override;

private:
    void resetRuntimeState() noexcept;
    ::media::Status prepareDecoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream);
    ::media::Status prepareEncoder(MediaGraphExecutionContext& context, const FFmpegInputStreamSnapshot& stream);

private:
    bool m_emitted = false;
    ::media::ffmpeg::BufferRefPtr m_decoderHardwareDevice;
    std::shared_ptr<MediaPreparedVideoDecoder> m_sourceDecoder;
    mutable std::mutex m_snapshotMutex;
    MediaBufferRef m_inputSnapshot;
    MediaBufferRef m_timestampSource;
    MediaBufferRef m_preparedDecoder;
    MediaBufferRef m_preparedEncoder;
    MediaBufferRef m_encoderParametersSnapshot;
    std::optional<MediaDecoderRuntimeFacts> m_decoderRuntimeFacts;
    std::optional<MediaVideoEncoderReadback> m_encoderReadback;
};

} // namespace media::ffmpeg::graph
