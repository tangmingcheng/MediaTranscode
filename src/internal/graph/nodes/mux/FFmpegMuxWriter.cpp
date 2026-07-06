#include "internal/graph/nodes/mux/FFmpegMuxWriter.h"

#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}

#include <utility>

namespace media::ffmpeg::graph {
namespace {

AVRational toAVRational(MediaRational value) noexcept
{
    return AVRational{ value.num, value.den };
}

bool known(AVRational value) noexcept
{
    return value.num != 0 && value.den != 0;
}

int streamIndexFor(MediaStreamKind kind, int videoIndex, int audioIndex) noexcept
{
    if (kind == MediaStreamKind::Video) {
        return videoIndex;
    }
    if (kind == MediaStreamKind::Audio) {
        return audioIndex;
    }
    return invalidMediaStreamIndex;
}

void setStreamIndex(MediaStreamKind kind, int index, int& videoIndex, int& audioIndex) noexcept
{
    if (kind == MediaStreamKind::Video) {
        videoIndex = index;
    } else if (kind == MediaStreamKind::Audio) {
        audioIndex = index;
    }
}

AVRational packetTimeBase(const MediaBufferRef& buffer) noexcept
{
    if (!buffer) {
        return AVRational{ 0, 1 };
    }
    if (buffer->timeDescriptor().timeBase.isKnown()) {
        return toAVRational(buffer->timeDescriptor().timeBase);
    }
    if (buffer->formatDescriptor().time.timeBase.isKnown()) {
        return toAVRational(buffer->formatDescriptor().time.timeBase);
    }
    return AVRational{ 0, 1 };
}

} // namespace

::media::Status FFmpegMuxWriter::configureExpectations(bool expectVideo, bool expectAudio) noexcept
{
    m_expectVideo = expectVideo;
    m_expectAudio = expectAudio;
    m_expectationsBound = true;
    return ::media::Status::success();
}

bool FFmpegMuxWriter::bindOutputContext(const MediaBufferRef& buffer) noexcept
{
    auto* contextBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!contextBuffer || !contextBuffer->context()) {
        return false;
    }
    m_outputContextBuffer = buffer;
    if (contextBuffer->ownership() == FFmpegFormatContextOwnership::Output) {
        m_outputContextOwner = contextBuffer->takeOutputContext();
        m_outputContext = m_outputContextOwner.get();
        m_outputContextBuffer.reset();
    } else {
        m_outputContextOwner.reset();
        m_outputContext = contextBuffer->context();
    }
    m_headerWritten = false;
    m_trailerWritten = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_audioStreamIndex = invalidMediaStreamIndex;
    return m_outputContext != nullptr;
}

::media::Status FFmpegMuxWriter::tryBindStreamConfig(const MediaBufferRef& buffer)
{
    const bool accepted = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get()) ||
        dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get());
    if (!accepted) {
        return ::media::Status::success();
    }
    if (m_headerWritten) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter received late stream config"));
    }
    if (!m_outputContext) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    auto registered = registerStreamFromConfig(buffer);
    return registered ? writePendingPacketsIfReady() : registered;
}

::media::Status FFmpegMuxWriter::registerPendingStreamConfigs()
{
    if (!m_outputContext || m_pendingStreamConfigs.empty()) {
        return ::media::Status::success();
    }
    auto pending = std::move(m_pendingStreamConfigs);
    m_pendingStreamConfigs.clear();
    for (const auto& buffer : pending) {
        auto status = registerStreamFromConfig(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status FFmpegMuxWriter::registerStreamFromConfig(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    if (dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get())) {
        return registerStreamFromCodecContext(buffer);
    }
    if (dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get())) {
        return registerStreamFromCodecParameters(buffer);
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter expected stream config"));
}

::media::Status FFmpegMuxWriter::registerStreamFromCodecContext(const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codec = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codec || !known(codec->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter requires upstream codec context and time_base"));
    }
    const MediaStreamKind kind = buffer->streamKind();
    if (streamIndexFor(kind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        return ::media::Status::success();
    }
    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream"));
    }
    const int ret = avcodec_parameters_from_context(stream->codecpar, codec);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_from_context");
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = codec->time_base;
    if (kind == MediaStreamKind::Video) {
        stream->avg_frame_rate = codec->framerate;
        stream->r_frame_rate = codec->framerate;
    }
    setStreamIndex(kind, stream->index, m_videoStreamIndex, m_audioStreamIndex);
    return ::media::Status::success();
}

::media::Status FFmpegMuxWriter::registerStreamFromCodecParameters(const MediaBufferRef& buffer)
{
    auto* paramsBuffer = dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get());
    const AVCodecParameters* params = paramsBuffer ? paramsBuffer->parameters() : nullptr;
    if (!params || !buffer->timeDescriptor().timeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter requires upstream codec parameters and time_base"));
    }
    const MediaStreamKind kind = buffer->streamKind();
    if (streamIndexFor(kind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        return ::media::Status::success();
    }
    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream"));
    }
    const int ret = avcodec_parameters_copy(stream->codecpar, params);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_copy");
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = toAVRational(buffer->timeDescriptor().timeBase);
    setStreamIndex(kind, stream->index, m_videoStreamIndex, m_audioStreamIndex);
    return ::media::Status::success();
}

::media::Status FFmpegMuxWriter::writeHeaderIfNeeded()
{
    if (!m_outputContext || m_headerWritten) {
        return ::media::Status::success();
    }
    if (m_outputContext->nb_streams == 0 || !expectedStreamsRegistered()) {
        return ::media::Status::success();
    }
    const int ret = avformat_write_header(m_outputContext, nullptr);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avformat_write_header");
    }
    m_headerWritten = true;
    return ::media::Status::success();
}

::media::Status FFmpegMuxWriter::writePendingPacketsIfReady()
{
    auto header = writeHeaderIfNeeded();
    if (!header || !m_headerWritten || m_pendingPackets.empty()) {
        return header;
    }
    auto pending = std::move(m_pendingPackets);
    m_pendingPackets.clear();
    for (const auto& buffer : pending) {
        auto status = writePacketNow(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status FFmpegMuxWriter::writePacket(const MediaBufferRef& buffer)
{
    auto header = writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_headerWritten) {
        m_pendingPackets.push_back(buffer);
        return ::media::Status::success();
    }
    auto pending = writePendingPacketsIfReady();
    return pending ? writePacketNow(buffer) : pending;
}

::media::Status FFmpegMuxWriter::writePacketNow(const MediaBufferRef& buffer)
{
    const AVPacket* source = FFmpegPacketView::packet(buffer);
    if (!m_outputContext || !source) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter requires output context and packet"));
    }
    const int targetIndex = streamIndexFor(buffer->streamKind(), m_videoStreamIndex, m_audioStreamIndex);
    if (targetIndex == invalidMediaStreamIndex || targetIndex >= static_cast<int>(m_outputContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter packet stream is not registered"));
    }
    AVStream* muxStream = m_outputContext->streams[targetIndex];
    const AVRational srcTb = packetTimeBase(buffer);
    const AVRational muxTb = muxStream ? muxStream->time_base : AVRational{ 0, 1 };
    if (!known(srcTb) || !known(muxTb)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegMuxWriter requires packet time_base"));
    }
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("av_packet_alloc"));
    }
    const int refRet = av_packet_ref(packet.get(), source);
    if (refRet < 0) {
        return FFmpegGraphError::statusFromCode(refRet, "av_packet_ref");
    }
    av_packet_rescale_ts(packet.get(), srcTb, muxTb);
    packet->stream_index = targetIndex;
    const int writeRet = av_interleaved_write_frame(m_outputContext, packet.get());
    return writeRet < 0 ? FFmpegGraphError::statusFromCode(writeRet, "av_interleaved_write_frame")
                        : ::media::Status::success();
}

::media::Status FFmpegMuxWriter::writeTrailerIfNeeded()
{
    if (!m_outputContext || !m_headerWritten || m_trailerWritten) {
        return ::media::Status::success();
    }
    const int ret = av_write_trailer(m_outputContext);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_write_trailer");
    }
    m_trailerWritten = true;
    return ::media::Status::success();
}

void FFmpegMuxWriter::reset() noexcept
{
    m_pendingStreamConfigs.clear();
    m_pendingPackets.clear();
    m_outputContext = nullptr;
    m_outputContextOwner.reset();
    m_outputContextBuffer.reset();
    m_headerWritten = false;
    m_trailerWritten = false;
    m_expectationsBound = false;
    m_expectVideo = false;
    m_expectAudio = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_audioStreamIndex = invalidMediaStreamIndex;
}

AVFormatContext* FFmpegMuxWriter::context() noexcept
{
    return m_outputContext;
}

bool FFmpegMuxWriter::headerWritten() const noexcept
{
    return m_headerWritten;
}

bool FFmpegMuxWriter::readyForSdp() const noexcept
{
    return m_outputContext && expectedStreamsRegistered();
}

bool FFmpegMuxWriter::expectedStreamsRegistered() const noexcept
{
    if (!m_expectationsBound) {
        return false;
    }
    return (!m_expectVideo || m_videoStreamIndex != invalidMediaStreamIndex) &&
           (!m_expectAudio || m_audioStreamIndex != invalidMediaStreamIndex);
}

} // namespace media::ffmpeg::graph
