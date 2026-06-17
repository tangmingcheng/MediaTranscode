#include "internal/FFmpegAudioPipeline.h"

#include "internal/FFmpegAudioFifo.h"
#include "internal/FFmpegError.h"
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
namespace {

Status makeTimestampError(const std::string& message)
{
    return Status::failure(ErrorInfo::internalError(message));
}

} // namespace

FFmpegAudioPipeline::FFmpegAudioPipeline()
    : m_fifo(std::make_unique<FFmpegAudioFifo>())
{
}

FFmpegAudioPipeline::~FFmpegAudioPipeline()
{
    reset();
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

    m_mode = other.m_mode;
    m_codec = other.m_codec;
    m_inputAudioStream = other.m_inputAudioStream;
    m_outputFmtCtx = other.m_outputFmtCtx;
    m_outputAudioStream = other.m_outputAudioStream;
    m_timeline = other.m_timeline;
    m_decoderCtx = std::move(other.m_decoderCtx);
    m_encoderCtx = std::move(other.m_encoderCtx);
    m_swrCtx = std::move(other.m_swrCtx);
    m_fifo = std::move(other.m_fifo);
    m_decodedFrame = std::move(other.m_decodedFrame);
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
    other.m_fifo = std::make_unique<FFmpegAudioFifo>();
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

    m_decodedFrame.reset();
    m_swrCtx.reset();
    m_decoderCtx.reset();
    m_encoderCtx.reset();

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

Status FFmpegAudioPipeline::initialize(const Config& config)
{
    reset();

    m_mode = config.mode;
    m_codec = config.codec;
    m_inputAudioStream = config.inputAudioStream;
    m_outputFmtCtx = config.outputFmtCtx;
    m_timeline = config.timeline;
    m_audioBitrateKbps = config.audioBitrateKbps;

    if (m_mode == AudioMode::None) {
        return Status::success();
    }

    if (!m_inputAudioStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: inputAudioStream is null"));
    }

    if (!m_outputFmtCtx) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: outputFmtCtx is null"));
    }

    if (!m_timeline) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: timeline is null"));
    }

    if (m_mode == AudioMode::CopySelected) {
        return initializeCopy();
    }

    if (m_mode == AudioMode::EncodeSelected) {
        return initializeEncode();
    }

    return Status::failure(ErrorInfo::invalidArgument(
        "FFmpegAudioPipeline initialize failed: unknown audio mode"));
}

Status FFmpegAudioPipeline::initializeCopy()
{
    m_outputAudioStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputAudioStream) {
        return Status::failure(makeAllocationError("avformat_new_stream audio failed"));
    }

    const int ret = avcodec_parameters_copy(m_outputAudioStream->codecpar, m_inputAudioStream->codecpar);
    if (ret < 0) {
        return Status::failure(makeFFmpegError("avcodec_parameters_copy audio failed", ret));
    }

    m_outputAudioStream->codecpar->codec_tag = 0;
    m_outputAudioStream->time_base = m_inputAudioStream->time_base;
    return Status::success();
}

Status FFmpegAudioPipeline::initializeEncode()
{
    const AVCodec* decoder = avcodec_find_decoder(m_inputAudioStream->codecpar->codec_id);
    if (!decoder) {
        return Status::failure(ErrorInfo::unsupported(
            "avcodec_find_decoder audio failed: unsupported input audio codec"));
    }

    m_decoderCtx = makeCodecContext(decoder);
    if (!m_decoderCtx) {
        return Status::failure(makeAllocationError(
            "avcodec_alloc_context3 audio decoder failed"));
    }

    int ret = avcodec_parameters_to_context(m_decoderCtx.get(), m_inputAudioStream->codecpar);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_parameters_to_context audio decoder failed", ret));
    }

    m_decoderCtx->pkt_timebase = m_inputAudioStream->time_base;

    ret = avcodec_open2(m_decoderCtx.get(), decoder, nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_open2 audio decoder failed", ret));
    }

    if (!ensureAudioDecoderChannelLayout(m_decoderCtx.get())) {
        return Status::failure(ErrorInfo::invalidArgument(
            "invalid input audio channel layout"));
    }

    const char* encoderName = preferredAudioEncoderName(m_codec);
    const AVCodec* encoder = encoderName ? avcodec_find_encoder_by_name(encoderName) : nullptr;
    if (!encoder) {
        encoder = avcodec_find_encoder(fallbackAudioCodecId(m_codec));
    }

    if (!encoder) {
        return Status::failure(ErrorInfo::unsupported(
            "avcodec_find_encoder audio failed: requested audio encoder not found"));
    }

    m_encoderCtx = makeCodecContext(encoder);
    if (!m_encoderCtx) {
        return Status::failure(makeAllocationError(
            "avcodec_alloc_context3 audio encoder failed"));
    }

    if (!copyAudioChannelLayoutToEncoder(m_encoderCtx.get(), m_decoderCtx.get())) {
        return Status::failure(ErrorInfo::internalError(
            "copy audio channel layout to encoder failed"));
    }

    m_encoderCtx->sample_rate = chooseAudioSampleRate(encoder, m_decoderCtx->sample_rate);
    m_encoderCtx->sample_fmt = chooseAudioSampleFormat(encoder);
    m_encoderCtx->time_base = AVRational{ 1, m_encoderCtx->sample_rate };
    m_encoderCtx->bit_rate = static_cast<int64_t>(std::max(32, m_audioBitrateKbps)) * 1000;

    if (encoder->capabilities & AV_CODEC_CAP_EXPERIMENTAL) {
        m_encoderCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    }

    if (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(m_encoderCtx.get(), encoder, nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            std::string("avcodec_open2 audio encoder failed [") +
                (encoder->name ? encoder->name : "unknown") + "]",
            ret));
    }

    m_outputAudioStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputAudioStream) {
        return Status::failure(makeAllocationError(
            "avformat_new_stream encoded audio failed"));
    }

    m_outputAudioStream->time_base = m_encoderCtx->time_base;

    ret = avcodec_parameters_from_context(m_outputAudioStream->codecpar, m_encoderCtx.get());
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_parameters_from_context audio failed", ret));
    }

    m_outputAudioStream->codecpar->codec_tag = 0;

    m_decodedFrame = makeFrame();
    if (!m_decodedFrame) {
        return Status::failure(makeAllocationError(
            "av_frame_alloc decoded audio frame failed"));
    }

    return initializeResamplerAndFifo();
}

Status FFmpegAudioPipeline::initializeResamplerAndFifo()
{
    int ret = 0;

#if LIBAVUTIL_VERSION_MAJOR >= 57
    SwrContext* rawSwrCtx = nullptr;
    ret = swr_alloc_set_opts2(
        &rawSwrCtx,
        &m_encoderCtx->ch_layout,
        m_encoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        &m_decoderCtx->ch_layout,
        m_decoderCtx->sample_fmt,
        m_decoderCtx->sample_rate,
        0,
        nullptr
    );
    SwrContextPtr createdSwrCtx(rawSwrCtx);

    if (ret < 0) {
        return Status::failure(makeFFmpegError("swr_alloc_set_opts2 failed", ret));
    }

    if (!createdSwrCtx) {
        return Status::failure(makeAllocationError(
            "swr_alloc_set_opts2 failed: no swr context allocated"));
    }

    m_swrCtx = std::move(createdSwrCtx);
#else
    m_swrCtx.reset(swr_alloc_set_opts(
        nullptr,
        oldAudioChannelLayout(m_encoderCtx.get()),
        m_encoderCtx->sample_fmt,
        m_encoderCtx->sample_rate,
        oldAudioChannelLayout(m_decoderCtx.get()),
        m_decoderCtx->sample_fmt,
        m_decoderCtx->sample_rate,
        0,
        nullptr
    ));

    if (!m_swrCtx) {
        return Status::failure(makeAllocationError("swr_alloc_set_opts failed"));
    }
#endif

    ret = swr_init(m_swrCtx.get());
    if (ret < 0) {
        return Status::failure(makeFFmpegError("swr_init failed", ret));
    }

    const int outputChannels = audioChannelCount(m_encoderCtx.get());
    if (outputChannels <= 0) {
        return Status::failure(ErrorInfo::invalidArgument(
            "invalid output audio channel count"));
    }

    if (!m_fifo) {
        return Status::failure(ErrorInfo::internalError("audio fifo is null"));
    }

    std::string fifoError;
    return makeLegacyStatus(
        m_fifo->initialize(
            m_encoderCtx->sample_fmt,
            outputChannels,
            m_encoderCtx->frame_size > 0 ? m_encoderCtx->frame_size : 1024,
            &fifoError
        ),
        fifoError
    );
}

Status FFmpegAudioPipeline::processPacket(
    AVPacket* packet,
    const PacketWrittenCallback& onPacketWritten)
{
    if (m_mode == AudioMode::None) {
        return Status::success();
    }

    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline processPacket failed: packet is null"));
    }

    if (m_mode == AudioMode::CopySelected) {
        return writeCopyPacket(packet, onPacketWritten);
    }

    if (m_mode == AudioMode::EncodeSelected) {
        return sendPacketToDecoder(packet, onPacketWritten);
    }

    return Status::failure(ErrorInfo::invalidArgument(
        "FFmpegAudioPipeline processPacket failed: unknown audio mode"));
}

Status FFmpegAudioPipeline::writeCopyPacket(
    AVPacket* packet,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_outputAudioStream || !m_inputAudioStream || !m_outputFmtCtx) {
        return Status::success();
    }

    packet->stream_index = m_outputAudioStream->index;

    Status timestampStatus = normalizeCopyPacketTimestamp(packet);
    if (!timestampStatus) {
        return timestampStatus;
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        if (m_lastWrittenDts != AV_NOPTS_VALUE && packet->dts <= m_lastWrittenDts) {
            std::ostringstream oss;
            oss << "audio packet dts is not strictly increasing: current="
                << packet->dts << ", last=" << m_lastWrittenDts;
            return makeTimestampError(oss.str());
        }

        m_lastWrittenDts = packet->dts;
    }

    if (packet->pts != AV_NOPTS_VALUE &&
        packet->dts != AV_NOPTS_VALUE &&
        packet->pts < packet->dts) {
        std::ostringstream oss;
        oss << "audio packet pts is smaller than dts: pts="
            << packet->pts << ", dts=" << packet->dts;
        return makeTimestampError(oss.str());
    }

    updateProgressFromPacket(packet);

    const int ret = av_interleaved_write_frame(m_outputFmtCtx, packet);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "av_interleaved_write_frame audio failed", ret));
    }

    ++m_packetCount;

    if (onPacketWritten) {
        onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
    }

    return Status::success();
}

Status FFmpegAudioPipeline::normalizeCopyPacketTimestamp(AVPacket* packet) const
{
    if (!packet || !m_inputAudioStream || !m_outputAudioStream || !m_timeline) {
        return Status::success();
    }

    const AVRational inputTimeBase = m_inputAudioStream->time_base;
    const AVRational outputTimeBase = m_outputAudioStream->time_base;

    if (packet->pts != AV_NOPTS_VALUE) {
        const int64_t ptsUs = TimelineNormalizer::toUs(packet->pts, inputTimeBase);
        const int64_t normalizedPtsUs = m_timeline->normalizeUs(ptsUs);

        if (normalizedPtsUs == AV_NOPTS_VALUE) {
            return makeTimestampError("failed to normalize audio packet pts");
        }

        packet->pts = TimelineNormalizer::fromUs(normalizedPtsUs, outputTimeBase);
    }

    if (packet->dts != AV_NOPTS_VALUE) {
        const int64_t dtsUs = TimelineNormalizer::toUs(packet->dts, inputTimeBase);
        const int64_t normalizedDtsUs = m_timeline->normalizeUs(dtsUs);

        if (normalizedDtsUs == AV_NOPTS_VALUE) {
            return makeTimestampError("failed to normalize audio packet dts");
        }

        packet->dts = TimelineNormalizer::fromUs(normalizedDtsUs, outputTimeBase);
    }

    if (packet->duration > 0) {
        packet->duration = av_rescale_q(packet->duration, inputTimeBase, outputTimeBase);
    }

    return Status::success();
}

Status FFmpegAudioPipeline::sendPacketToDecoder(
    AVPacket* packet,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx) {
        return Status::success();
    }

    const int ret = avcodec_send_packet(m_decoderCtx.get(), packet);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_packet audio decoder failed", ret));
    }

    return drainDecoder(onPacketWritten);
}

Status FFmpegAudioPipeline::drainDecoder(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx || !m_decodedFrame) {
        return Status::success();
    }

    while (true) {
        const int ret = avcodec_receive_frame(m_decoderCtx.get(), m_decodedFrame.get());

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return Status::success();
        }

        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "avcodec_receive_frame audio decoder failed", ret));
        }

        Status status = pushDecodedFrameToFifo(onPacketWritten);
        av_frame_unref(m_decodedFrame.get());

        if (!status) {
            return status;
        }
    }
}

Status FFmpegAudioPipeline::pushDecodedFrameToFifo(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decodedFrame || !m_swrCtx || !m_fifo || !m_fifo->isInitialized() || !m_encoderCtx || !m_decoderCtx) {
        return Status::success();
    }

    Status ptsStatus = ensureInitialAudioPts();
    if (!ptsStatus) {
        return ptsStatus;
    }

    const int64_t delay = swr_get_delay(m_swrCtx.get(), m_decoderCtx->sample_rate);
    const int dstNbSamples = static_cast<int>(av_rescale_rnd(
        delay + m_decodedFrame->nb_samples,
        m_encoderCtx->sample_rate,
        m_decoderCtx->sample_rate,
        AV_ROUND_UP
    ));

    if (dstNbSamples <= 0) {
        return Status::success();
    }

    FramePtr convertedFrame = makeFrame();
    if (!convertedFrame) {
        return Status::failure(makeAllocationError(
            "av_frame_alloc converted audio frame failed"));
    }

    convertedFrame->nb_samples = dstNbSamples;
    convertedFrame->format = m_encoderCtx->sample_fmt;
    convertedFrame->sample_rate = m_encoderCtx->sample_rate;

    if (!setFrameAudioLayoutFromCodecContext(convertedFrame.get(), m_encoderCtx.get())) {
        return Status::failure(ErrorInfo::internalError(
            "set converted audio frame channel layout failed"));
    }

    int ret = av_frame_get_buffer(convertedFrame.get(), 0);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "av_frame_get_buffer converted audio frame failed", ret));
    }

    const int convertedSamples = swr_convert(
        m_swrCtx.get(),
        convertedFrame->extended_data,
        dstNbSamples,
        const_cast<const uint8_t**>(m_decodedFrame->extended_data),
        m_decodedFrame->nb_samples
    );

    if (convertedSamples < 0) {
        return Status::failure(makeFFmpegError("swr_convert failed", convertedSamples));
    }

    convertedFrame->nb_samples = convertedSamples;

    if (convertedSamples > 0) {
        std::string fifoError;
        Status fifoStatus = makeLegacyStatus(
            m_fifo->writeFrame(convertedFrame.get(), &fifoError),
            fifoError
        );
        if (!fifoStatus) {
            return fifoStatus;
        }
    }

    return encodeFifo(false, onPacketWritten);
}

Status FFmpegAudioPipeline::ensureInitialAudioPts()
{
    if (!m_encoderCtx) {
        return Status::success();
    }

    if (m_nextAudioPts != AV_NOPTS_VALUE) {
        return Status::success();
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
        return Status::success();
    }

    const int64_t inputAudioUs = TimelineNormalizer::toUs(inputAudioTs, m_inputAudioStream->time_base);
    const int64_t normalizedAudioUs = m_timeline ? m_timeline->normalizeUs(inputAudioUs) : AV_NOPTS_VALUE;

    if (normalizedAudioUs == AV_NOPTS_VALUE) {
        return makeTimestampError("failed to normalize input audio timestamp");
    }

    m_nextAudioPts = TimelineNormalizer::fromUs(normalizedAudioUs, m_encoderCtx->time_base);
    if (m_nextAudioPts == AV_NOPTS_VALUE || m_nextAudioPts < 0) {
        m_nextAudioPts = 0;
    }

    return Status::success();
}

Status FFmpegAudioPipeline::encodeFifo(
    bool flushAll,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_fifo || !m_fifo->isInitialized() || !m_encoderCtx) {
        return Status::success();
    }

    const int frameSize = m_encoderCtx->frame_size > 0 ? m_encoderCtx->frame_size : 1024;

    while (m_fifo->size() >= frameSize || (flushAll && m_fifo->size() > 0)) {
        const int availableSamples = m_fifo->size();
        const int samplesToRead = flushAll ? std::min(frameSize, availableSamples) : frameSize;

        FramePtr audioFrame = makeFrame();
        if (!audioFrame) {
            return Status::failure(makeAllocationError(
                "av_frame_alloc encoded audio frame failed"));
        }

        audioFrame->nb_samples = samplesToRead;
        audioFrame->format = m_encoderCtx->sample_fmt;
        audioFrame->sample_rate = m_encoderCtx->sample_rate;
        audioFrame->pts = m_nextAudioPts == AV_NOPTS_VALUE ? 0 : m_nextAudioPts;

        if (!setFrameAudioLayoutFromCodecContext(audioFrame.get(), m_encoderCtx.get())) {
            return Status::failure(ErrorInfo::internalError(
                "set encoded audio frame channel layout failed"));
        }

        int ret = av_frame_get_buffer(audioFrame.get(), 0);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "av_frame_get_buffer encoded audio frame failed", ret));
        }

        std::string fifoError;
        Status fifoStatus = makeLegacyStatus(
            m_fifo->readToFrame(audioFrame.get(), samplesToRead, &fifoError),
            fifoError
        );
        if (!fifoStatus) {
            return fifoStatus;
        }

        Status sendStatus = sendFrame(audioFrame.get());
        if (!sendStatus) {
            return sendStatus;
        }

        Result<int> packetsResult = receiveAndWritePackets(onPacketWritten);
        if (!packetsResult) {
            return Status::failure(packetsResult.error());
        }

        m_nextAudioPts = audioFrame->pts + audioFrame->nb_samples;
    }

    return Status::success();
}

Status FFmpegAudioPipeline::flushResampler()
{
    if (!m_swrCtx || !m_decoderCtx || !m_encoderCtx || !m_fifo) {
        return Status::success();
    }

    while (true) {
        const int64_t delay = swr_get_delay(m_swrCtx.get(), m_decoderCtx->sample_rate);
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

        FramePtr convertedFrame = makeFrame();
        if (!convertedFrame) {
            return Status::failure(makeAllocationError(
                "av_frame_alloc swr flush audio frame failed"));
        }

        convertedFrame->nb_samples = dstNbSamples;
        convertedFrame->format = m_encoderCtx->sample_fmt;
        convertedFrame->sample_rate = m_encoderCtx->sample_rate;

        if (!setFrameAudioLayoutFromCodecContext(convertedFrame.get(), m_encoderCtx.get())) {
            return Status::failure(ErrorInfo::internalError(
                "set swr flush audio frame channel layout failed"));
        }

        int ret = av_frame_get_buffer(convertedFrame.get(), 0);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "av_frame_get_buffer swr flush audio frame failed", ret));
        }

        const int convertedSamples = swr_convert(
            m_swrCtx.get(),
            convertedFrame->extended_data,
            dstNbSamples,
            nullptr,
            0
        );

        if (convertedSamples < 0) {
            return Status::failure(makeFFmpegError(
                "swr_convert flush failed", convertedSamples));
        }

        convertedFrame->nb_samples = convertedSamples;

        if (convertedSamples <= 0) {
            break;
        }

        std::string fifoError;
        Status fifoStatus = makeLegacyStatus(
            m_fifo->writeFrame(convertedFrame.get(), &fifoError),
            fifoError
        );
        if (!fifoStatus) {
            return fifoStatus;
        }
    }

    return Status::success();
}

Status FFmpegAudioPipeline::flush(
    const PacketWrittenCallback& onPacketWritten)
{
    if (m_mode != AudioMode::EncodeSelected) {
        return Status::success();
    }

    if (m_decoderCtx) {
        const int ret = avcodec_send_packet(m_decoderCtx.get(), nullptr);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "avcodec_send_packet audio decoder flush failed", ret));
        }

        Status drainStatus = drainDecoder(onPacketWritten);
        if (!drainStatus) {
            return drainStatus;
        }
    }

    Status resamplerStatus = flushResampler();
    if (!resamplerStatus) {
        return resamplerStatus;
    }

    Status fifoStatus = encodeFifo(true, onPacketWritten);
    if (!fifoStatus) {
        return fifoStatus;
    }

    Status sendStatus = sendFrame(nullptr);
    if (!sendStatus) {
        return sendStatus;
    }

    Result<int> packetsResult = receiveAndWritePackets(onPacketWritten);
    if (!packetsResult) {
        return Status::failure(packetsResult.error());
    }

    return Status::success();
}

Status FFmpegAudioPipeline::sendFrame(AVFrame* frame)
{
    if (!m_encoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline sendFrame failed: encoderCtx is null"));
    }

    const int ret = avcodec_send_frame(m_encoderCtx.get(), frame);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_frame audio encoder failed", ret));
    }

    return Status::success();
}

Result<int> FFmpegAudioPipeline::receiveAndWritePackets(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_encoderCtx || !m_outputFmtCtx || !m_outputAudioStream) {
        return Result<int>::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline receiveAndWritePackets failed: pipeline is not initialized"));
    }

    int packetsWritten = 0;

    while (true) {
        PacketPtr packet = makePacket();
        if (!packet) {
            return Result<int>::failure(makeAllocationError(
                "av_packet_alloc audio packet failed"));
        }

        const int receiveRet = avcodec_receive_packet(m_encoderCtx.get(), packet.get());

        if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
            break;
        }

        if (receiveRet < 0) {
            return Result<int>::failure(makeFFmpegError(
                "avcodec_receive_packet audio encoder failed", receiveRet));
        }

        packet->stream_index = m_outputAudioStream->index;

        av_packet_rescale_ts(packet.get(), m_encoderCtx->time_base, m_outputAudioStream->time_base);

        if (packet->dts != AV_NOPTS_VALUE) {
            if (m_lastWrittenDts != AV_NOPTS_VALUE && packet->dts <= m_lastWrittenDts) {
                std::ostringstream oss;
                oss << "encoded audio packet dts is not strictly increasing: current="
                    << packet->dts << ", last=" << m_lastWrittenDts;

                return Result<int>::failure(makeTimestampError(oss.str()).error());
            }

            m_lastWrittenDts = packet->dts;
        }

        if (packet->pts != AV_NOPTS_VALUE &&
            packet->dts != AV_NOPTS_VALUE &&
            packet->pts < packet->dts) {
            std::ostringstream oss;
            oss << "encoded audio packet pts is smaller than dts: pts="
                << packet->pts << ", dts=" << packet->dts;

            return Result<int>::failure(makeTimestampError(oss.str()).error());
        }

        updateProgressFromPacket(packet.get());

        const int writeRet = av_interleaved_write_frame(m_outputFmtCtx, packet.get());
        if (writeRet < 0) {
            return Result<int>::failure(makeFFmpegError(
                "av_interleaved_write_frame encoded audio failed", writeRet));
        }

        ++m_packetCount;
        ++packetsWritten;

        if (onPacketWritten) {
            onPacketWritten(m_packetCount, m_lastWrittenOutTimeMs);
        }
    }

    return Result<int>::success(packetsWritten);
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
