#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
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
    Transferred,
    Empty
};

struct FFmpegInputStreamSnapshot final {
    int index = -1;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    ::media::ffmpeg::CodecParametersPtr codecParameters;
    MediaFormatDescriptor format;
    MediaTimeDescriptor time;
};

class FFmpegFormatContextBuffer final : public MediaBuffer {
public:
    explicit FFmpegFormatContextBuffer(::media::ffmpeg::OutputFormatContextPtr context);
    explicit FFmpegFormatContextBuffer(AVFormatContext* borrowedContext);

    static ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> createInput(
        ::media::ffmpeg::InputFormatContextPtr context);

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
    explicit FFmpegFormatContextBuffer(InputTag, ::media::ffmpeg::InputFormatContextPtr context);
    ::media::Status buildInputSnapshot();

    FFmpegFormatContextOwnership m_ownership = FFmpegFormatContextOwnership::Borrowed;
    ::media::ffmpeg::InputFormatContextPtr m_inputContext;
    ::media::ffmpeg::OutputFormatContextPtr m_outputContext;
    AVFormatContext* m_borrowedContext = nullptr;
    std::vector<FFmpegInputStreamSnapshot> m_inputStreams;
    bool m_inputSnapshotComplete = false;
};

} // namespace media::ffmpeg::graph
