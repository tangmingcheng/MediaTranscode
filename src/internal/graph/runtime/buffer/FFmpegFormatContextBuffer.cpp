#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaStreamKind streamKindFromCodecType(AVMediaType type) noexcept
{
    if (type == AVMEDIA_TYPE_VIDEO) return MediaStreamKind::Video;
    if (type == AVMEDIA_TYPE_AUDIO) return MediaStreamKind::Audio;
    return MediaStreamKind::Unknown;
}

MediaTimeDescriptor snapshotTime(const AVStream& stream)
{
    MediaTimeDescriptor time;
    time.timeBase = FFmpegDescriptorMapper::toRational(stream.time_base);
    time.frameRate = FFmpegDescriptorMapper::toRational(stream.avg_frame_rate);
    time.startTime = stream.start_time;
    time.duration = stream.duration;
    return time;
}

} // namespace

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

::media::Status FFmpegFormatContextBuffer::buildInputSnapshot()
{
    AVFormatContext* input = m_inputContext.get();
    if (!input) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("input snapshot requires owned format context"));
    }
    m_inputStreams.reserve(input->nb_streams);
    for (unsigned int index = 0; index < input->nb_streams; ++index) {
        const AVStream* stream = input->streams[index];
        if (!stream) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "input snapshot stream " + std::to_string(index) + " is null"));
        }
        if (!stream->codecpar) {
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "input snapshot stream " + std::to_string(index) + " codec parameters are null"));
        }
        auto parameters = ::media::ffmpeg::makeCodecParameters();
        if (!parameters) {
            return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
                "input snapshot stream " + std::to_string(index) + " codec parameter allocation failed"));
        }
        const int copyResult = avcodec_parameters_copy(parameters.get(), stream->codecpar);
        if (copyResult < 0) {
            return ::media::Status::failure(FFmpegGraphError::fromCode(
                copyResult, "input snapshot stream " + std::to_string(index) + " avcodec_parameters_copy"));
        }
        FFmpegInputStreamSnapshot snapshot;
        snapshot.index = static_cast<int>(index);
        snapshot.streamKind = streamKindFromCodecType(stream->codecpar->codec_type);
        snapshot.codecParameters = std::move(parameters);
        snapshot.format = FFmpegDescriptorMapper::fromStream(stream);
        snapshot.time = snapshotTime(*stream);
        m_inputStreams.push_back(std::move(snapshot));
    }
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
