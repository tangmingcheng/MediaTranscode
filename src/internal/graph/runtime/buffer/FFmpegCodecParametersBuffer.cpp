#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegCodecParametersBuffer::FFmpegCodecParametersBuffer(::media::ffmpeg::CodecParametersPtr parameters)
    : m_parameters(std::move(parameters))
{
    setPayloadKind(MediaPayloadKind::CodecParameters);
    if (m_parameters) {
        setStreamKind(FFmpegDescriptorMapper::toStreamKind(m_parameters->codec_type));
        setFormatDescriptor(FFmpegDescriptorMapper::fromCodecParameters(m_parameters.get()));
    }
}

MediaBufferType FFmpegCodecParametersBuffer::type() const noexcept
{
    return MediaBufferType::CodecParameters;
}

AVCodecParameters* FFmpegCodecParametersBuffer::parameters() noexcept
{
    return m_parameters.get();
}

const AVCodecParameters* FFmpegCodecParametersBuffer::parameters() const noexcept
{
    return m_parameters.get();
}

} // namespace media::ffmpeg::graph
