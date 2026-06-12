#include "internal/FFmpegAudioPipeline.h"

#include "internal/FFmpegAudioFifo.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/version.h>
}

namespace media::ffmpeg {

FFmpegAudioPipeline::FFmpegAudioPipeline()
    : m_fifo(new FFmpegAudioFifo())
{
}

FFmpegAudioPipeline::~FFmpegAudioPipeline()
{
    reset();
    delete m_fifo;
    m_fifo = nullptr;
}

FFmpegAudioPipeline::FFmpegAudioPipeline(FFmpegAudioPipeline&& other) noexcept
{
    *this = std::move(other);
}

FFmpegAudioPipeline& FFmpegAudioPipeline::operator=(FFmpegAudioPipeline&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();
    delete m_fifo;

    m_mode = other.m_mode;
    m_codec = other.m_codec;
    m_inputAudioStream = other.m_inputAudioStream;
    m_outputFmtCtx = other.m_outputFmtCtx;
    m_outputAudioStream = other.m_outputAudioStream;
    m_timeline = other.m_timeline;
    m_decoderCtx = other.m_decoderCtx;
    m_encoderCtx = other.m_encoderCtx;
    m_swrCtx = other.m_swrCtx;
    m_fifo = other.m_fifo;
    m_decodedFrame = other.m_decodedFrame;
    m_packetCount = other.m_packetCount;
    m_lastWrittenDts = other.m_lastWrittenDts;
    m_lastWrittenOutTimeMs = other.m_lastWrittenOutTimeMs;
    m_nextAudioPts = other.m_nextAudioPts;
    m_audioBitrateKbps = other.m_audioBitrateKbps;

    other.m_mode = AudioMode::None;
    other.m_codec = AudioCodec::AAC;
    other.m_inputAudioStream = nullptr;
    other.m_outputFmtCtx = nullptr;
    other.m_outputAudioStream = nullptr;
    other.m_timeline = nullptr;
    other.m_decoderCtx = nullptr;
    other.m_encoderCtx = nullptr;
    other.m_swrCtx = nullptr;
    other.m_fifo = new FFmpegAudioFifo();
    other.m_decodedFrame = nullptr;
    other.m_packetCount = 0;
    other.m_lastWrittenDts = AV_NOPTS_VALUE;
    other.m_lastWrittenOutTimeMs = 0;
    other.m_nextAudioPts = AV_NOPTS_VALUE;
    other.m_audioBitrateKbps = 128;

    return *this;
}

void FFmpegAudioPipeline::reset()
{
    if (m_fifo) {
        m_fifo->reset();
    }

    if (m_decodedFrame) {
        av_frame_free(&m_decodedFrame);
    }

    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }

    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
    }

    if (m_encoderCtx) {
        avcodec_free_context(&m_encoderCtx);
    }

    m_mode = AudioMode::None;
    m_codec = AudioCodec::AAC;
    m_inputAudioStream = nullptr;
    m_outputFmtCtx = nullptr;
    m_outputAudioStream = nullptr;
    m_timeline = nullptr;

    m_packetCount = 0;
    m_lastWrittenDts = AV_NOPTS_VALUE;
    m_lastWrittenOutTimeMs = 0;
    m_nextAudioPts = AV_NOPTS_VALUE;
    m_audioBitrateKbps = 128;
}

bool FFmpegAudioPipeline::initialize(const Config& config, std::string* error)
{
    reset();

    m_mode = config.mode;
    m_codec = config.codec;
    m_inputAudioStream = config.inputAudioStream;
    m_outputFmtCtx = config.outputFmtCtx;
    m_timeline = config.timeline;
    m_audioBitrateKbps = config.audioBitrateKbps;

    if (m_mode == AudioMode::None) {
        return true;
    }

    if (!m_inputAudioStream) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: inputAudioStream is null";
        }
        return false;
    }

    if (!m_outputFmtCtx) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: outputFmtCtx is null";
        }
        return false;
    }

    if (!m_timeline) {
        if (error) {
            *error = "FFmpegAudioPipeline initialize failed: timeline is null";
        }
        return false;
    }

    if (m_mode == AudioMode::CopySelected) {
        return initializeCopy(error);
    }

    if (m_mode == AudioMode::EncodeSelected) {
        return initializeEncode(error);
    }

    if (error) {
        *error = "FFmpegAudioPipeline initialize failed: unknown audio mode";
    }
    return false;
}

bool FFmpegAudioPipeline::initializeCopy(std::string* error)
{
    m_outputAudioStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputAudioStream) {
        if (error) {
            *error = "avformat_new_stream audio failed";
        }
        return false;
    }

    const int ret = avcodec_parameters_copy(m_outputAudioStream->codecpar, m_inputAudioStream->codecpar);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_copy audio failed: " + errorString(ret);
        }
        return false;
    }

    m_outputAudioStream->codecpar->codec_tag = 0;
    m_outputAudioStream->time_base = m_inputAudioStream->time_base;
    return true;
}

bool FFmpegAudioPipeline::initializeEncode(std::string* error)
{
    const AVCodec* decoder = avcodec_find_decoder(m_inputAudioStream->codecpar->codec_id);
    if (!decoder) {
        if (error) {
            *error = "avcodec_find_decoder audio failed: unsupported input audio codec";
        }
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(decoder);
    if (!m_decoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 audio decoder failed";
        }
        return false;
    }

    int ret = avcodec_parameters_to_context(m_decoderCtx, m_inputAudioStream->codecpar);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_to_context audio decoder failed: " + errorString(ret);
        }
        return false;
    }

    m_decoderCtx->pkt_timebase = m_inputAudioStream->time_base;

    ret = avcodec_open2(m_decoderCtx, decoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_open2 audio decoder failed: " + errorString(ret);
        }
        return false;
    }

    if (!ensureAudioDecoderChannelLayout(m_decoderCtx)) {
        if (error) {
            *error = "invalid input audio channel layout";
        }
        return false;
    }

    const char* encoderName = preferredAudioEncoderName(m_codec);
    const AVCodec* encoder = encoderName ? avcodec_find_encoder_by_name(encoderName) : nullptr;
    if (!encoder) {
        encoder = avcodec_find_encoder(fallbackAudioCodecId(m_codec));
    }

    if (!encoder) {
        if (error) {
            *error = "avcodec_find_encoder audio failed: requested audio encoder not found";
        }
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(encoder);
    if (!m_encoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 audio encoder failed";
        }
        return false;
    }

    if (!copyAudioChannelLayoutToEncoder(m_encoderCtx, m_decoderCtx)) {
        if (error) {
            *error = "copy audio channel layout to encoder failed";
        }
        return false;
    }

    m_encoderCtx->sample_rate = chooseAudioSampleRate(encoder, m_decoderCtx->sample_rate);
    m_encoderCtx->sample_fmt = chooseAudioSampleFormat(encoder);
    m_encoderCtx->time_base = AVRational{ 1, m_encoderCtx->sample_rate };
    m_encoderCtx->bit_rate = static_cast<int64_t>(std::max(32, m_audioBitrateKbps)) * 1000;

    if (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_encoderCtx, encoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = std::string("avcodec_open2 audio encoder failed [") +
                (encoder->name ? encoder->name : "unknown") + "]: " + errorString(ret);
        }
        return false;
    }

    m_outputAudioStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputAudioStream) {
        if (error) {
            *error = "avformat_new_stream encoded audio failed";
        }
        return false;
    }

    m_outputAudioStream->time_base = m_encoderCtx->time_base;

    ret = avcodec_parameters_from_context(m_outputAudioStream->codecpar, m_encoderCtx);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_from_context audio failed: " + errorString(ret);
        }
        return false;
    }

    m_outputAudioStream->codecpar->codec_tag = 0;

    m_decodedFrame = av_frame_alloc();
    if (!m_decodedFrame) {
        if (error) {
            *error = "av_frame_alloc decoded audio frame failed";
        }
        return false;
    }

    return initializeResamplerAndFifo(error);
}

bool FFmpegAudioPipeline::initializeResamplerAndFifo(std::string* error)
{
    int ret = 0;

#if LIBAVUTIL_VERSION_MAJOR >= 57
    ret = swr_alloc_set_opts2(
        &m_swrCtx,
        &m_encoderCtx->ch_layout,
        m_encoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        &m_decoderCtx->ch_layout,
        m_decoderCtx->sample_fmt,
        m_decoderCtx->sample_rate,
        0,
        nullptr
    );

    if (ret < 0 || !m_swrCtx) {
        if (error) {
            *error = "swr_alloc_set_opts2 failed: " + errorString(ret);
        }
        return false;
    }
#else
    m_swrCtx = swr_alloc_set_opts(
        nullptr,
        oldAudioChannelLayout(m_encoderCtx),
        m_encoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        oldAudioChannelLayout(m_decoderCtx),
        m_decoderCtx->sample_fmt,
        m_decoderCtx->sample_rate,
        0,
        nullptr
    );

    if (!m_swrCtx) {
        if (error) {
            *error = "swr_alloc_set_opts failed";
        }
        return false;
    }
#endif

    ret = swr_init(m_swrCtx);
    if (ret < 0) {
        if (error) {
            *error = "swr_init failed: " + errorString(ret);
        }
        return false;
    }

    const int outputChannels = audioChannelCount(m_encoderCtx);
    if (outputChannels <= 0) {
        if (error) {
            *error = "invalid output audio channel count";
        }
        return false;
    }

    if (!m_fifo) {
        if (error) {
            *error = "audio fifo is null";
        }
        return false;
    }

    return m_fifo->initialize(
        m_encoderCtx->sample_fmt,
        outputChannels,
        m_encoderCtx->frame_size > 0 ? m_encoderCtx->frame_size : 1024,
        error
    );
}

bool FFmpegAudioPipeline::processPacket(
    AVPacket* packet,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (m_mode == AudioMode::None) {
        return true;
    }

    if (!packet) {
        if (error) {
            *error = "FFmpegAudioPipeline processPacket failed: packet is null";
        }
        return false;
    }

    if (m_mode == AudioMode::CopySelected) {
        return writeCopyPacket(packet, error, onPacketWritten);
    }

    if (m_mode == AudioMode::EncodeSelected) {
        return sendPacketToDecoder(packet, error, onPacketWritten);
    }

    if (error) {
        *error = "FFmpegAudioPipeline processPacket failed: unknown audio mode";
    }
    return false;
}

bool FFmpegAudioPipeline::writeCopyPacket(
    AVPacket* packet,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_outputAudioStream || !m_inputAudioStream || !m_outputFmtCtx) {
        return true;
    }

    packet->stream_index = m_outputAudioStream->index;

    if (!normalizeCopyPacketTimestamp(packet, error)) {
        return false;
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        if (m_lastWrittenDts != AV_NOPTS_VALUE && packet->dts <= m_lastWrittenDts) {
            std::ostringstream oss;
            oss << "audio packet dts is not strictly increasing: current="
                << packet->dts << ", last=" << m_lastWrittenDts;
            if (error) {
                *error = oss.str();
            }
            return false;
        }

        m_lastWrittenDts = packet->dts;
    }

    if (packet->pts != AV_NOPTS_VALUE &&
        packet->dts != AV_NOPTS_VALUE &&
        packet->pts < packet->dts) {
        std::ostringstream oss;
        oss << "audio packet pts is smaller than dts: pts="
            << packet->pts << ", dts=" << packet->dts;
        if (error) {
            *error = oss.str();
        }
        return false;
    }

    updateProgressFromPacket(packet);

    const int ret = av_interleaved_write_frame(m_outputFmtCtx, packet);
    if (ret < 0) {
        if (error) {
            *error = "av_interleaved_write_frame audio failed: " + errorString(ret);
        }
        return false;
    }

    ++m_packetCount;

    if (onPacketWritten) {
        onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
    }

    return true;
}

bool FFmpegAudioPipeline::normalizeCopyPacketTimestamp(AVPacket* packet, std::string* error) const
{
    if (!packet || !m_inputAudioStream || !m_outputAudioStream || !m_timeline) {
        return true;
    }

    const AVRational inputTimeBase = m_inputAudioStream->time_base;
    const AVRational outputTimeBase = m_outputAudioStream->time_base;

    if (packet->pts != AV_NOPTS_VALUE) {
        const int64_t ptsUs = TimelineNormalizer::toUs(packet->pts, inputTimeBase);
        const int64_t normalizedPtsUs = m_timeline->normalizeUs(ptsUs);

        if (normalizedPtsUs == AV_NOPTS_VALUE) {
            if (error) {
                *error = "failed to normalize audio packet pts";
            }
            return false;
        }

        packet->pts = TimelineNormalizer::fromUs(normalizedPtsUs, outputTimeBase);
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        const int64_t dtsUs = TimelineNormalizer::toUs(packet->dts, inputTimeBase);
        const int64_t normalizedDtsUs = m_timeline->normalizeUs(dtsUs);

        if (normalizedDtsUs == AV_NOPTS_VALUE) {
            if (error) {
                *error = "failed to normalize audio packet dts";
            }
            return false;
        }

        packet->dts = TimelineNormalizer::fromUs(normalizedDtsUs, outputTimeBase);
    }

    if (packet->duration > 0) {
        packet->duration = av_rescale_q(packet->duration, inputTimeBase, outputTimeBase);
    }

    return true;
}

bool FFmpegAudioPipeline::sendPacketToDecoder(
    AVPacket* packet,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx) {
        return true;
    }

    const int ret = avcodec_send_packet(m_decoderCtx, packet);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_packet audio decoder failed: " + errorString(ret);
        }
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegAudioPipeline::drainDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx || !m_decodedFrame) {
        return true;
    }

    while (true) {
        const int ret = avcodec_receive_frame(m_decoderCtx, m_decodedFrame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        }

        if (ret < 0) {
            if (error) {
                *error = "avcodec_receive_frame audio decoder failed: " + errorString(ret);
            }
            return false;
        }

        const bool ok = pushDecodedFrameToFifo(error, onPacketWritten);
        av_frame_unref(m_decodedFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegAudioPipeline::pushDecodedFrameToFifo(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decodedFrame || !m_swrCtx || !m_fifo || !m_fifo->isInitialized() || !m_encoderCtx || !m_decoderCtx) {
        return true;
    }

    if (!ensureInitialAudioPts(error)) {
        return false;
    }

    const int64_t delay = swr_get_delay(m_swrCtx, m_decoderCtx->sample_rate);
    const int dstNbSamples = static_cast<int>(av_rescale_rnd(
        delay + m_decodedFrame->nb_samples,
        m_encoderCtx->sample_rate,
        m_decoderCtx->sample_rate,
        AV_ROUND_UP
    ));

    if (dstNbSamples <= 0) {
        return true;
    }

    AVFrame* convertedFrame = av_frame_alloc();
    if (!convertedFrame) {
        if (error) {
            *error = "av_frame_alloc converted audio frame failed";
        }
        return false;
    }

    convertedFrame->nb_samples = dstNbSamples;
    convertedFrame->format = m_encoderCtx->sample_fmt;
    convertedFrame->sample_rate = m_encoderCtx->sample_rate;

    if (!setFrameAudioLayoutFromCodecContext(convertedFrame, m_encoderCtx)) {
        av_frame_free(&convertedFrame);
        if (error) {
            *error = "set converted audio frame channel layout failed";
        }
        return false;
    }

    int ret = av_frame_get_buffer(convertedFrame, 0);
    if (ret < 0) {
        av_frame_free(&convertedFrame);
        if (error) {
            *error = "av_frame_get_buffer converted audio frame failed: " + errorString(ret);
        }
        return false;
    }

    const int convertedSamples = swr_convert(
        m_swrCtx,
        convertedFrame->extended_data,
        dstNbSamples,
        const_cast<const uint8_t**>(m_decodedFrame->extended_data),
        m_decodedFrame->nb_samples
    );

    if (convertedSamples < 0) {
        av_frame_free(&convertedFrame);
        if (error) {
            *error = "swr_convert failed: " + errorString(convertedSamples);
        }
        return false;
    }

    convertedFrame->nb_samples = convertedSamples;

    if (convertedSamples > 0 && !m_fifo->writeFrame(convertedFrame, error)) {
        av_frame_free(&convertedFrame);
        return false;
    }

    av_frame_free(&convertedFrame);

    return encodeFifo(false, error, onPacketWritten);
}

bool FFmpegAudioPipeline::ensureInitialAudioPts(std::string* error)
{
    if (!m_encoderCtx) {
        return true;
    }

    if (m_nextAudioPts != AV_NOPTS_VALUE) {
        return true;
    }

    int64_t inputAudioTs = AV_NOPTS_VALUE;
    if (m_decodedFrame) {
        if (m_decodedFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
            inputAudioTs = m_decodedFrame->best_effort_timestamp;
        }
        else if (m_decodedFrame->pts != AV_NOPTS_VALUE) {
            inputAudioTs = m_decodedFrame->pts;
        }
        else if (m_decodedFrame->pkt_dts != AV_NOPTS_VALUE) {
            inputAudioTs = m_decodedFrame->pkt_dts;
        }
    }

    if (inputAudioTs == AV_NOPTS_VALUE) {
        m_nextAudioPts = 0;
        return true;
    }

    const int64_t inputAudioUs = TimelineNormalizer::toUs(inputAudioTs, m_inputAudioStream->time_base);
    const int64_t normalizedAudioUs = m_timeline ? m_timeline->normalizeUs(inputAudioUs) : AV_NOPTS_VALUE;

    if (normalizedAudioUs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "failed to normalize input audio timestamp";
        }
        return false;
    }

    m_nextAudioPts = TimelineNormalizer::fromUs(normalizedAudioUs, m_encoderCtx->time_base);
    if (m_nextAudioPts == AV_NOPTS_VALUE || m_nextAudioPts < 0) {
        m_nextAudioPts = 0;
    }

    return true;
}

bool FFmpegAudioPipeline::encodeFifo(
    bool flushAll,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_fifo || !m_fifo->isInitialized() || !m_encoderCtx) {
        return true;
    }

    const int frameSize = m_encoderCtx->frame_size > 0 ? m_encoderCtx->frame_size : 1024;

    while (m_fifo->size() >= frameSize || (flushAll && m_fifo->size() > 0)) {
        const int availableSamples = m_fifo->size();
        const int samplesToRead = flushAll ? std::min(frameSize, availableSamples) : frameSize;

        AVFrame* audioFrame = av_frame_alloc();
        if (!audioFrame) {
            if (error) {
                *error = "av_frame_alloc encoded audio frame failed";
            }
            return false;
        }

        audioFrame->nb_samples = samplesToRead;
        audioFrame->format = m_encoderCtx->sample_fmt;
        audioFrame->sample_rate = m_encoderCtx->sample_rate;
        audioFrame->pts = m_nextAudioPts == AV_NOPTS_VALUE ? 0 : m_nextAudioPts;

        if (!setFrameAudioLayoutFromCodecContext(audioFrame, m_encoderCtx)) {
            av_frame_free(&audioFrame);
            if (error) {
                *error = "set encoded audio frame channel layout failed";
            }
            return false;
        }

        int ret = av_frame_get_buffer(audioFrame, 0);
        if (ret < 0) {
            av_frame_free(&audioFrame);
            if (error) {
                *error = "av_frame_get_buffer encoded audio frame failed: " + errorString(ret);
            }
            return false;
        }

        if (!m_fifo->readToFrame(audioFrame, samplesToRead, error)) {
            av_frame_free(&audioFrame);
            return false;
        }

        const bool ok = sendFrame(audioFrame, error) && receiveAndWritePackets(error, onPacketWritten) >= 0;
        m_nextAudioPts = audioFrame->pts + audioFrame->nb_samples;
        av_frame_free(&audioFrame);

        if (!ok) {
            return false;
        }
    }

    return true;
}

bool FFmpegAudioPipeline::flushResampler(std::string* error)
{
    if (!m_swrCtx || !m_decoderCtx || !m_encoderCtx || !m_fifo) {
        return true;
    }

    while (true) {
        const int64_t delay = swr_get_delay(m_swrCtx, m_decoderCtx->sample_rate);
        if (delay <= 0) {
            break;
        }

        const int dstNbSamples = static_cast<int>(av_rescale_rnd(
            delay,
            m_encoderCtx->sample_rate,
            m_decoderCtx->sample_rate,
            AV_ROUND_UP
        ));

        if (dstNbSamples <= 0) {
            break;
        }

        AVFrame* convertedFrame = av_frame_alloc();
        if (!convertedFrame) {
            if (error) {
                *error = "av_frame_alloc swr flush audio frame failed";
            }
            return false;
        }

        convertedFrame->nb_samples = dstNbSamples;
        convertedFrame->format = m_encoderCtx->sample_fmt;
        convertedFrame->sample_rate = m_encoderCtx->sample_rate;

        if (!setFrameAudioLayoutFromCodecContext(convertedFrame, m_encoderCtx)) {
            av_frame_free(&convertedFrame);
            if (error) {
                *error = "set swr flush audio frame channel layout failed";
            }
            return false;
        }

        int ret = av_frame_get_buffer(convertedFrame, 0);
        if (ret < 0) {
            av_frame_free(&convertedFrame);
            if (error) {
                *error = "av_frame_get_buffer swr flush audio frame failed: " + errorString(ret);
            }
            return false;
        }

        const int convertedSamples = swr_convert(
            m_swrCtx,
            convertedFrame->extended_data,
            dstNbSamples,
            nullptr,
            0
        );

        if (convertedSamples < 0) {
            av_frame_free(&convertedFrame);
            if (error) {
                *error = "swr_convert flush failed: " + errorString(convertedSamples);
            }
            return false;
        }

        convertedFrame->nb_samples = convertedSamples;

        if (convertedSamples <= 0) {
            av_frame_free(&convertedFrame);
            break;
        }

        if (!m_fifo->writeFrame(convertedFrame, error)) {
            av_frame_free(&convertedFrame);
            return false;
        }

        av_frame_free(&convertedFrame);
    }

    return true;
}

bool FFmpegAudioPipeline::flush(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (m_mode != AudioMode::EncodeSelected) {
        return true;
    }

    if (m_decoderCtx) {
        const int ret = avcodec_send_packet(m_decoderCtx, nullptr);
        if (ret < 0) {
            if (error) {
                *error = "avcodec_send_packet audio decoder flush failed: " + errorString(ret);
            }
            return false;
        }

        if (!drainDecoder(error, onPacketWritten)) {
            return false;
        }
    }

    if (!flushResampler(error)) {
        return false;
    }

    if (!encodeFifo(true, error, onPacketWritten)) {
        return false;
    }

    return sendFrame(nullptr, error) && receiveAndWritePackets(error, onPacketWritten) >= 0;
}

bool FFmpegAudioPipeline::sendFrame(AVFrame* frame, std::string* error)
{
    if (!m_encoderCtx) {
        if (error) {
            *error = "FFmpegAudioPipeline sendFrame failed: encoderCtx is null";
        }
        return false;
    }

    const int ret = avcodec_send_frame(m_encoderCtx, frame);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_frame audio encoder failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

int FFmpegAudioPipeline::receiveAndWritePackets(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_encoderCtx || !m_outputFmtCtx || !m_outputAudioStream) {
        if (error) {
            *error = "FFmpegAudioPipeline receiveAndWritePackets failed: pipeline is not initialized";
        }
        return -1;
    }

    int packetsWritten = 0;

    while (true) {
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            if (error) {
                *error = "av_packet_alloc audio packet failed";
            }
            return -1;
        }

        const int receiveRet = avcodec_receive_packet(m_encoderCtx, packet);

        if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
            av_packet_free(&packet);
            break;
        }

        if (receiveRet < 0) {
            if (error) {
                *error = "avcodec_receive_packet audio encoder failed: " + errorString(receiveRet);
            }
            av_packet_free(&packet);
            return -1;
        }

        packet->stream_index = m_outputAudioStream->index;

        av_packet_rescale_ts(packet, m_encoderCtx->time_base, m_outputAudioStream->time_base);

        if (packet->dts != AV_NOPTS_VALUE) {
            if (m_lastWrittenDts != AV_NOPTS_VALUE && packet->dts <= m_lastWrittenDts) {
                std::ostringstream oss;
                oss << "encoded audio packet dts is not strictly increasing: current="
                    << packet->dts << ", last=" << m_lastWrittenDts;

                if (error) {
                    *error = oss.str();
                }
                av_packet_free(&packet);
                return -1;
            }

            m_lastWrittenDts = packet->dts;
        }

        if (packet->pts != AV_NOPTS_VALUE &&
            packet->dts != AV_NOPTS_VALUE &&
            packet->pts < packet->dts) {
            std::ostringstream oss;
            oss << "encoded audio packet pts is smaller than dts: pts="
                << packet->pts << ", dts=" << packet->dts;

            if (error) {
                *error = oss.str();
            }
            av_packet_free(&packet);
            return -1;
        }

        updateProgressFromPacket(packet);

        const int writeRet = av_interleaved_write_frame(m_outputFmtCtx, packet);
        av_packet_free(&packet);

        if (writeRet < 0) {
            if (error) {
                *error = "av_interleaved_write_frame encoded audio failed: " + errorString(writeRet);
            }
            return -1;
        }

        ++m_packetCount;
        ++packetsWritten;

        if (onPacketWritten) {
            onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
        }
    }

    return packetsWritten;
}

bool FFmpegAudioPipeline::updateProgressFromPacket(const AVPacket* packet)
{
    if (!packet || !m_outputAudioStream) {
        return false;
    }

    const int64_t timestamp = packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts;
    if (timestamp == AV_NOPTS_VALUE) {
        return false;
    }

    const int64_t outTimeMs = av_rescale_q(
        timestamp,
        m_outputAudioStream->time_base,
        AVRational{ 1, 1000 }
    );

    m_lastWrittenOutTimeMs = std::max<int64_t>(m_lastWrittenOutTimeMs, outTimeMs);
    return true;
}

bool FFmpegAudioPipeline::isInitialized() const
{
    return m_mode == AudioMode::None || m_outputAudioStream || m_encoderCtx || m_decoderCtx;
}

AudioMode FFmpegAudioPipeline::mode() const
{
    return m_mode;
}

AVStream* FFmpegAudioPipeline::outputStream() const
{
    return m_outputAudioStream;
}

int64_t FFmpegAudioPipeline::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegAudioPipeline::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

} // namespace media::ffmpeg
