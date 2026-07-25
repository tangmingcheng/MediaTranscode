#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshotFactory.h"

#include <utility>

namespace media::ffmpeg::graph {
FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(InputTag, ::media::ffmpeg::InputFormatContextPtr context)
    : m_ownership(FFmpegFormatContextOwnership::Input)
    , m_inputContext(std::move(context))
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> FFmpegFormatContextBuffer::createInput(
    ::media::ffmpeg::InputFormatContextPtr context)
{
    if (!context) {
        return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegFormatContextBuffer input context is null"));
    }
    auto buffer = std::unique_ptr<FFmpegFormatContextBuffer>(
        new FFmpegFormatContextBuffer(InputTag{}, std::move(context)));
    auto snapshot = buffer->buildInputSnapshot();
    if (!snapshot) {
        return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(snapshot.error());
    }
    return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::success(std::move(buffer));
}

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(
    SnapshotTag,
    std::vector<FFmpegInputStreamSnapshot> streams)
    : m_ownership(FFmpegFormatContextOwnership::Snapshot)
    , m_inputStreams(std::move(streams))
    , m_inputSnapshotComplete(true)
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> FFmpegFormatContextBuffer::createSnapshot(
    std::vector<FFmpegInputStreamSnapshot> streams)
{
    if (streams.empty()) return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(
        ::media::ErrorInfo::invalidArgument("FFmpeg input snapshot requires at least one stream"));
    for (std::size_t index = 0; index < streams.size(); ++index) {
        if (streams[index].index != static_cast<int>(index) || !streams[index].codecParametersComplete() ||
            !streams[index].time.timeBase.isKnown()) {
            return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::failure(
                ::media::ErrorInfo::invalidArgument("FFmpeg input snapshot stream is incomplete"));
        }
    }
    return ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>>::success(
        std::unique_ptr<FFmpegFormatContextBuffer>(new FFmpegFormatContextBuffer(SnapshotTag{}, std::move(streams))));
}

::media::Status FFmpegFormatContextBuffer::buildInputSnapshot()
{
    AVFormatContext* input = m_inputContext.get();
    if (!input) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("input snapshot requires owned format context"));
    }
    auto snapshots = FFmpegInputStreamSnapshotFactory::fromFormatContext(*input);
    if (!snapshots) return ::media::Status::failure(snapshots.error());
    m_inputStreams = std::move(snapshots.value());
    m_inputSnapshotComplete = m_inputStreams.size() == input->nb_streams;
    return ::media::Status::success();
}

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(::media::ffmpeg::OutputFormatContextPtr context)
    : m_ownership(FFmpegFormatContextOwnership::Output)
    , m_outputContext(std::move(context))
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

FFmpegFormatContextBuffer::FFmpegFormatContextBuffer(AVFormatContext* borrowedContext)
    : m_ownership(FFmpegFormatContextOwnership::Borrowed)
    , m_borrowedContext(borrowedContext)
{
    setPayloadKind(MediaPayloadKind::FormatContext);
    setStreamKind(MediaStreamKind::Metadata);
}

MediaBufferType FFmpegFormatContextBuffer::type() const noexcept
{
    return MediaBufferType::FormatContext;
}

AVFormatContext* FFmpegFormatContextBuffer::context() noexcept
{
    if (m_inputContext) {
        return m_inputContext.get();
    }

    if (m_outputContext) {
        return m_outputContext.get();
    }

    return m_borrowedContext;
}

const AVFormatContext* FFmpegFormatContextBuffer::context() const noexcept
{
    if (m_inputContext) {
        return m_inputContext.get();
    }

    if (m_outputContext) {
        return m_outputContext.get();
    }

    return m_borrowedContext;
}

FFmpegFormatContextOwnership FFmpegFormatContextBuffer::ownership() const noexcept
{
    return m_ownership;
}

const FFmpegInputStreamSnapshot* FFmpegFormatContextBuffer::inputStreamSnapshot(int streamIndex) const noexcept
{
    if (!m_inputSnapshotComplete || streamIndex < 0 || static_cast<std::size_t>(streamIndex) >= m_inputStreams.size()) return nullptr;
    return &m_inputStreams[static_cast<std::size_t>(streamIndex)];
}

bool FFmpegFormatContextBuffer::inputSnapshotComplete() const noexcept
{
    return m_inputSnapshotComplete;
}

::media::ffmpeg::InputFormatContextPtr FFmpegFormatContextBuffer::takeInputContext() noexcept
{
    m_ownership = m_inputContext ? FFmpegFormatContextOwnership::Transferred : FFmpegFormatContextOwnership::Empty;
    return std::move(m_inputContext);
}

::media::ffmpeg::OutputFormatContextPtr FFmpegFormatContextBuffer::takeOutputContext() noexcept
{
    m_ownership = m_outputContext ? FFmpegFormatContextOwnership::Transferred : FFmpegFormatContextOwnership::Empty;
    return std::move(m_outputContext);
}

} // namespace media::ffmpeg::graph
