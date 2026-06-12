#include "internal/FFmpegVideoTranscodePipeline.h"

#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg {

FFmpegVideoTranscodePipeline::~FFmpegVideoTranscodePipeline()
{
    reset();
}

FFmpegVideoTranscodePipeline::FFmpegVideoTranscodePipeline(FFmpegVideoTranscodePipeline&& other) noexcept
{
    *this = std::move(other);
}

FFmpegVideoTranscodePipeline& FFmpegVideoTranscodePipeline::operator=(FFmpegVideoTranscodePipeline&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_config = other.m_config;
    m_inputVideoStream = other.m_inputVideoStream;
    m_outputFmtCtx = other.m_outputFmtCtx;
    m_outputVideoStream = other.m_outputVideoStream;
    m_timeline = other.m_timeline;
    m_decoderCtx = other.m_decoderCtx;
    m_encoderCtx = other.m_encoderCtx;
    m_filterGraph = std::move(other.m_filterGraph);
    m_packetWriter = std::move(other.m_packetWriter);
    m_decodedFrame = other.m_decodedFrame;
    m_filteredFrame = other.m_filteredFrame;
    m_lastSubmittedPts = other.m_lastSubmittedPts;
    m_packetCount = other.m_packetCount;
    m_lastWrittenOutTimeMs = other.m_lastWrittenOutTimeMs;
    m_outputFps = other.m_outputFps;
    m_enableConstantFps = other.m_enableConstantFps;

    other.m_inputVideoStream = nullptr;
    other.m_outputFmtCtx = nullptr;
    other.m_outputVideoStream = nullptr;
    other.m_timeline = nullptr;
    other.m_decoderCtx = nullptr;
    other.m_encoderCtx = nullptr;
    other.m_decodedFrame = nullptr;
    other.m_filteredFrame = nullptr;
    other.m_lastSubmittedPts = AV_NOPTS_VALUE;
    other.m_packetCount = 0;
    other.m_lastWrittenOutTimeMs = 0;
    other.m_outputFps = 0;
    other.m_enableConstantFps = false;

    return *this;
}

void FFmpegVideoTranscodePipeline::reset()
{
    m_filterGraph.reset();
    m_packetWriter.reset();

    if (m_filteredFrame) {
        av_frame_free(&m_filteredFrame);
    }

    if (m_decodedFrame) {
        av_frame_free(&m_decodedFrame);
    }

    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
    }

    if (m_encoderCtx) {
        avcodec_free_context(&m_encoderCtx);
    }

    m_config = TranscodeConfig{};
    m_inputVideoStream = nullptr;
    m_outputFmtCtx = nullptr;
    m_outputVideoStream = nullptr;
    m_timeline = nullptr;
    m_lastSubmittedPts = AV_NOPTS_VALUE;
    m_packetCount = 0;
    m_lastWrittenOutTimeMs = 0;
    m_outputFps = 0;
    m_enableConstantFps = false;
}

bool FFmpegVideoTranscodePipeline::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.transcodeConfig) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: transcodeConfig is null";
        }
        return false;
    }

    if (!config.inputVideoStream) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: inputVideoStream is null";
        }
        return false;
    }

    if (!config.outputFmtCtx) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: outputFmtCtx is null";
        }
        return false;
    }

    if (!config.timeline) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: timeline is null";
        }
        return false;
    }

    m_config = *config.transcodeConfig;
    m_inputVideoStream = config.inputVideoStream;
    m_outputFmtCtx = config.outputFmtCtx;
    m_timeline = config.timeline;

    return openDecoder(error) &&
        openEncoderAndCreateOutputStream(error) &&
        initializeFilterGraph(error) &&
        initializePacketWriter(error) &&
        allocateFrames(error);
}

bool FFmpegVideoTranscodePipeline::openDecoder(std::string* error)
{
    const AVCodec* decoder = avcodec_find_decoder(m_inputVideoStream->codecpar->codec_id);
    if (!decoder) {
        if (error) {
            *error = "avcodec_find_decoder failed: unsupported input video codec";
        }
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(decoder);
    if (!m_decoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 decoder failed";
        }
        return false;
    }

    int ret = avcodec_parameters_to_context(m_decoderCtx, m_inputVideoStream->codecpar);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_to_context decoder failed: " + errorString(ret);
        }
        return false;
    }

    ret = avcodec_open2(m_decoderCtx, decoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_open2 decoder failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTranscodePipeline::openEncoderAndCreateOutputStream(std::string* error)
{
    const char* encoderName = preferredVideoEncoderName(m_config.videoCodec);
    const AVCodec* encoder = nullptr;

    if (encoderName) {
        encoder = avcodec_find_encoder_by_name(encoderName);
    }

    if (!encoder) {
        const AVCodecID codecId = fallbackVideoCodecId(m_config.videoCodec);
        encoder = avcodec_find_encoder(codecId);
    }

    if (!encoder) {
        if (error) {
            *error = "avcodec_find_encoder failed: no suitable video encoder";
        }
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(encoder);
    if (!m_encoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 encoder failed";
        }
        return false;
    }

    m_outputFps = chooseOutputFps(m_config, m_inputVideoStream);
    m_enableConstantFps = m_config.fps > 0;

    int outputWidth = m_config.width > 0 ? m_config.width : m_decoderCtx->width;
    int outputHeight = m_config.height > 0 ? m_config.height : m_decoderCtx->height;

    outputWidth = normalizeEvenSize(outputWidth);
    outputHeight = normalizeEvenSize(outputHeight);

    if (outputWidth <= 0 || outputHeight <= 0) {
        if (error) {
            *error = "invalid output video size";
        }
        return false;
    }

    m_encoderCtx->width = outputWidth;
    m_encoderCtx->height = outputHeight;

    AVRational encoderTimeBase = AVRational{ 1, m_outputFps };
    if (!m_enableConstantFps) {
        encoderTimeBase = m_inputVideoStream->time_base;
        if (encoderTimeBase.num <= 0 || encoderTimeBase.den <= 0) {
            encoderTimeBase = AVRational{ 1, m_outputFps };
        }
    }

    m_encoderCtx->time_base = encoderTimeBase;
    m_encoderCtx->framerate = AVRational{ m_outputFps, 1 };
    m_encoderCtx->pix_fmt = chooseVideoEncoderPixelFormat(encoder);
    m_encoderCtx->bit_rate = static_cast<int64_t>(std::max(1, m_config.videoBitrateKbps)) * 1000;
    m_encoderCtx->gop_size = std::max(10, m_outputFps * 2);
    m_encoderCtx->max_b_frames = 0;

    if (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    setVideoEncoderOptions(m_encoderCtx, encoder);

    int ret = avcodec_open2(m_encoderCtx, encoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = std::string("avcodec_open2 encoder failed [") +
                (encoder->name ? encoder->name : "unknown") + "]: " + errorString(ret);
        }
        return false;
    }

    m_outputVideoStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputVideoStream) {
        if (error) {
            *error = "avformat_new_stream video failed";
        }
        return false;
    }

    m_outputVideoStream->time_base = m_encoderCtx->time_base;

    ret = avcodec_parameters_from_context(m_outputVideoStream->codecpar, m_encoderCtx);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_from_context video failed: " + errorString(ret);
        }
        return false;
    }

    m_outputVideoStream->codecpar->codec_tag = 0;
    return true;
}

bool FFmpegVideoTranscodePipeline::initializeFilterGraph(std::string* error)
{
    VideoFilterGraph::Config config;
    config.decoderCtx = m_decoderCtx;
    config.encoderCtx = m_encoderCtx;
    config.inputStream = m_inputVideoStream;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;

    return m_filterGraph.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializePacketWriter(std::string* error)
{
    FFmpegVideoPipeline::Config config;
    config.encoderCtx = m_encoderCtx;
    config.outputFmtCtx = m_outputFmtCtx;
    config.outputVideoStream = m_outputVideoStream;

    return m_packetWriter.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::allocateFrames(std::string* error)
{
    m_decodedFrame = av_frame_alloc();
    m_filteredFrame = av_frame_alloc();

    if (!m_decodedFrame || !m_filteredFrame) {
        if (error) {
            *error = "av_frame_alloc video frame failed";
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTranscodePipeline::processPacket(
    AVPacket* packet,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline processPacket failed: decoder is not initialized";
        }
        return false;
    }

    const int ret = avcodec_send_packet(m_decoderCtx, packet);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_packet decoder failed: " + errorString(ret);
        }
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx) {
        return true;
    }

    const int ret = avcodec_send_packet(m_decoderCtx, nullptr);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_packet decoder flush failed: " + errorString(ret);
        }
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushFilterAndEncoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_filterGraph.flush(error)) {
        return false;
    }

    if (!drainFilterGraph(error, onPacketWritten)) {
        return false;
    }

    return writeEncodedPackets(nullptr, error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::drainDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        const int ret = avcodec_receive_frame(m_decoderCtx, m_decodedFrame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        }

        if (ret < 0) {
            if (error) {
                *error = "avcodec_receive_frame decoder failed: " + errorString(ret);
            }
            return false;
        }

        const bool ok = processDecodedFrame(error, onPacketWritten);
        av_frame_unref(m_decodedFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegVideoTranscodePipeline::processDecodedFrame(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    const int64_t inputVideoTs = decodedFrameTimestamp();
    if (inputVideoTs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "input video frame has no valid timestamp; refuse to synthesize PTS in normalized transcoder";
        }
        return false;
    }

    const int64_t inputVideoUs = TimelineNormalizer::toUs(inputVideoTs, m_inputVideoStream->time_base);
    const int64_t normalizedVideoUs = m_timeline->normalizeUs(inputVideoUs);

    if (normalizedVideoUs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "failed to normalize input video timestamp";
        }
        return false;
    }

    m_decodedFrame->pts = TimelineNormalizer::fromUs(normalizedVideoUs, m_inputVideoStream->time_base);
    if (m_decodedFrame->pts == AV_NOPTS_VALUE) {
        if (error) {
            *error = "decoded video frame pts is invalid after normalization";
        }
        return false;
    }

    if (!m_filterGraph.sendFrame(m_decodedFrame, error)) {
        return false;
    }

    return drainFilterGraph(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::drainFilterGraph(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        const int receiveRet = m_filterGraph.receiveFrame(m_filteredFrame, error);

        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
            return false;
        }

        const AVRational filterTimeBase = m_filterGraph.sinkTimeBase();

        if (m_filteredFrame->pts == AV_NOPTS_VALUE) {
            av_frame_unref(m_filteredFrame);
            if (error) {
                *error = "filtered video frame has invalid pts";
            }
            return false;
        }

        m_filteredFrame->pts = av_rescale_q(
            m_filteredFrame->pts,
            filterTimeBase,
            m_encoderCtx->time_base
        );

        if (m_lastSubmittedPts != AV_NOPTS_VALUE &&
            m_filteredFrame->pts <= m_lastSubmittedPts) {
            std::ostringstream oss;
            oss << "filtered video timestamp is not strictly increasing: current="
                << m_filteredFrame->pts
                << ", last="
                << m_lastSubmittedPts;

            av_frame_unref(m_filteredFrame);
            if (error) {
                *error = oss.str();
            }
            return false;
        }

        m_lastSubmittedPts = m_filteredFrame->pts;

        const bool ok = writeEncodedPackets(m_filteredFrame, error, onPacketWritten);
        av_frame_unref(m_filteredFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegVideoTranscodePipeline::writeEncodedPackets(
    AVFrame* frame,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_packetWriter.sendFrame(frame, error)) {
        return false;
    }

    const int writtenPackets = m_packetWriter.receiveAndWritePackets(
        error,
        [&](int64_t packetCount, int64_t outTimeMs) {
            m_packetCount = packetCount;
            m_lastWrittenOutTimeMs = outTimeMs;

            if (onPacketWritten) {
                onPacketWritten(packetCount, outTimeMs);
            }
        }
    );

    return writtenPackets >= 0;
}

int64_t FFmpegVideoTranscodePipeline::decodedFrameTimestamp() const
{
    if (!m_decodedFrame) {
        return AV_NOPTS_VALUE;
    }

    if (m_decodedFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
        return m_decodedFrame->best_effort_timestamp;
    }

    if (m_decodedFrame->pts != AV_NOPTS_VALUE) {
        return m_decodedFrame->pts;
    }

    if (m_decodedFrame->pkt_dts != AV_NOPTS_VALUE) {
        return m_decodedFrame->pkt_dts;
    }

    return AV_NOPTS_VALUE;
}

bool FFmpegVideoTranscodePipeline::isInitialized() const
{
    return m_decoderCtx && m_encoderCtx && m_outputVideoStream;
}

AVStream* FFmpegVideoTranscodePipeline::outputStream() const
{
    return m_outputVideoStream;
}

int64_t FFmpegVideoTranscodePipeline::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegVideoTranscodePipeline::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

int64_t FFmpegVideoTranscodePipeline::estimatedOutTimeMs() const
{
    if (m_lastWrittenOutTimeMs > 0) {
        return m_lastWrittenOutTimeMs;
    }

    if (m_encoderCtx && m_encoderCtx->framerate.num > 0) {
        return static_cast<int64_t>(
            m_packetCount * 1000.0 *
            m_encoderCtx->framerate.den /
            m_encoderCtx->framerate.num
        );
    }

    return 0;
}

} // namespace media::ffmpeg
