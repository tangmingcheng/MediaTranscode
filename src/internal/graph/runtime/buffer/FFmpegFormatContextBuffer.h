#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersSnapshot.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaTimeDescriptor.h"
#include "media_transcode/Result.h"

#include <vector>

namespace media::ffmpeg::graph {

enum class FFmpegFormatContextOwnership {
    Input,
    Output,
    Borrowed,
    Snapshot,
    Transferred,
    Empty
};

struct FFmpegInputStreamSnapshot final {
    int index = -1;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    FFmpegCodecParametersSnapshot codec;
    MediaFormatDescriptor format;
    MediaTimeDescriptor time;

    bool codecParametersComplete() const noexcept;
    ::media::Result<::media::ffmpeg::CodecParametersPtr> cloneCodecParameters() const;
};

class FFmpegFormatContextBuffer final : public MediaBuffer {
public:
    explicit FFmpegFormatContextBuffer(::media::ffmpeg::OutputFormatContextPtr context);
    explicit FFmpegFormatContextBuffer(AVFormatContext* borrowedContext);

    static ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> createInput(
        ::media::ffmpeg::InputFormatContextPtr context);
    static ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> createSnapshot(
        std::vector<FFmpegInputStreamSnapshot> streams);

    MediaBufferType type() const noexcept override;

    AVFormatContext* context() noexcept;
    const AVFormatContext* context() const noexcept;

    FFmpegFormatContextOwnership ownership() const noexcept;
    const FFmpegInputStreamSnapshot* inputStreamSnapshot(int streamIndex) const noexcept;
    bool inputSnapshotComplete() const noexcept;

    ::media::ffmpeg::InputFormatContextPtr takeInputContext() noexcept;
    ::media::ffmpeg::OutputFormatContextPtr takeOutputContext() noexcept;

private:
    struct InputTag {};
    struct SnapshotTag {};
    explicit FFmpegFormatContextBuffer(InputTag, ::media::ffmpeg::InputFormatContextPtr context);
    explicit FFmpegFormatContextBuffer(SnapshotTag, std::vector<FFmpegInputStreamSnapshot> streams);
    ::media::Status buildInputSnapshot();

    FFmpegFormatContextOwnership m_ownership = FFmpegFormatContextOwnership::Borrowed;
    ::media::ffmpeg::InputFormatContextPtr m_inputContext;
    ::media::ffmpeg::OutputFormatContextPtr m_outputContext;
    AVFormatContext* m_borrowedContext = nullptr;
    std::vector<FFmpegInputStreamSnapshot> m_inputStreams;
    bool m_inputSnapshotComplete = false;
};

} // namespace media::ffmpeg::graph
