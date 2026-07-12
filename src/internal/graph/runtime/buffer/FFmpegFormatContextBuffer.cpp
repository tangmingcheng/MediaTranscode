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

AVRational resolvedSourceFrameRate(AVFormatContext& context, AVStream& stream) noexcept
{
    AVRational frameRate = av_guess_frame_rate(&context, &stream, nullptr);
    if (frameRate.num > 0 && frameRate.den > 0) return frameRate;
    if (stream.avg_frame_rate.num > 0 && stream.avg_frame_rate.den > 0) return stream.avg_frame_rate;
    if (stream.r_frame_rate.num > 0 && stream.r_frame_rate.den > 0) return stream.r_frame_rate;
    return AVRational{0, 1};
}

MediaTimeDescriptor snapshotTime(AVFormatContext& context, AVStream& stream)
{
    MediaTimeDescriptor time;
    time.timeBase = FFmpegDescriptorMapper::toRational(stream.time_base);
    time.frameRate = FFmpegDescriptorMapper::toRational(resolvedSourceFrameRate(context, stream));
    time.startTime = stream.start_time;
    time.duration = stream.duration;
    return time;
}

} // namespace

bool FFmpegInputStreamSnapshot::codecParametersComplete() const noexcept
{
    return codec.complete();
}

::media::Result<::media::ffmpeg::CodecParametersPtr>
FFmpegInputStreamSnapshot::cloneCodecParameters() const
{
    return codec.cloneCodecParameters();
}

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
    m_inputStreams.reserve(input->nb_streams);
    for (unsigned int index = 0; index < input->nb_streams; ++index) {
        AVStream* stream = input->streams[index];
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
        auto ownedSnapshot = FFmpegCodecParametersSnapshot::takeOwnership(std::move(parameters));
        if (!ownedSnapshot) return ::media::Status::failure(ownedSnapshot.error());
        snapshot.codec = std::move(ownedSnapshot).value();
        snapshot.format = FFmpegDescriptorMapper::fromStream(stream);
        snapshot.time = snapshotTime(*input, *stream);
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
