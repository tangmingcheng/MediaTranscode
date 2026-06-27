#include "internal/FFmpegAudioEncodePipeline.h"

#include "internal/FFmpegAudioFifo.h"
#include "internal/FFmpegError.h"
#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"
#include "internal/output/capabilities/audio/AudioOutputStreamProvider.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/version.h>
#include <libswresample/swresample.h>
}

namespace media::ffmpeg {
namespace {

Status makeTimestampError(const std::string& message)
{
    return Status::failure(ErrorInfo::internalError(message));
}

} // namespace

FFmpegAudioEncodePipeline::FFmpegAudioEncodePipeline()
    : m_fifo(std::make_unique<FFmpegAudioFifo>())
{
}

FFmpegAudioEncodePipeline::~FFmpegAudioEncodePipeline()
{
    reset();
}

void FFmpegAudioEncodePipeline::reset()
{
    m_processDiagnostics.flush(
        m_packetWriter.packetCount(),
        m_fifo && m_fifo->isInitialized() ? m_fifo->size() : 0,
        "reset"
    );
    m_processDiagnostics.reset();

    if (m_fifo) {
        m_fifo->reset();
    }

    m_decodedFrame.reset();
    m_swrCtx.reset();
    m_decoderCtx.reset();
    m_encoderCtx.reset();

    m_codec = AudioCodec::AAC;
    m_inputAudioStream = nullptr;
    m_outputStreamProvider = nullptr;
    m_outputNode = nullptr;
    m_outputAudioStream = nullptr;
    m_timeline = nullptr;

    m_packetWriter.reset();

    m_nextAudioPts = AV_NOPTS_VALUE;
    m_audioBitrateKbps = 128;
}

Status FFmpegAudioEncodePipeline::initialize(const FFmpegAudioPipelineConfig& config)
{
    reset();

    if (!config.inputAudioStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioEncodePipeline initialize failed: inputAudioStream is null"));
    }

    if (!config.outputStreamProvider) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioEncodePipeline initialize failed: outputStreamProvider is null"));
    }

    if (!config.outputNode) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioEncodePipeline initialize failed: outputNode is null"));
    }

    if (!config.timeline) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioEncodePipeline initialize failed: timeline is null"));
    }

    m_codec = config.codec;
    m_inputAudioStream = config.inputAudioStream;
    m_outputStreamProvider = config.outputStreamProvider;
    m_outputNode = config.outputNode;
    m_timeline = config.timeline;
    m_audioBitrateKbps = config.audioBitrateKbps;

    Status decoderStatus = initializeDecoder();
    if (!decoderStatus) {
        return decoderStatus;
    }

    Status encoderStatus = initializeEncoder();
    if (!encoderStatus) {
        return encoderStatus;
    }

    Status streamStatus = initializeOutputStream();
    if (!streamStatus) {
        return streamStatus;
    }

    auto allocStart = m_processDiagnostics.mark();
    m_decodedFrame = makeFrame();
    m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, allocStart);
    if (!m_decodedFrame) {
        return Status::failure(makeAllocationError(
            "av_frame_alloc decoded audio frame failed"));
    }

    return initializeResamplerAndFifo();
}

Status FFmpegAudioEncodePipeline::initializeDecoder()
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

    return Status::success();
}

Status FFmpegAudioEncodePipeline::initializeEncoder()
{
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

    if (m_outputStreamProvider->requiresGlobalHeader()) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    const int ret = avcodec_open2(m_encoderCtx.get(), encoder, nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            std::string("avcodec_open2 audio encoder failed [") +
                (encoder->name ? encoder->name : "unknown") + "]",
            ret));
    }

    return Status::success();
}

Status FFmpegAudioEncodePipeline::initializeOutputStream()
{
    auto streamResult = m_outputStreamProvider->createEncodedAudioStream(m_encoderCtx.get());
    if (!streamResult) {
        return Status::failure(streamResult.error());
    }

    m_outputAudioStream = streamResult.value();

    FFmpegAudioPacketWriter::Config writerConfig;
    writerConfig.outputNode = m_outputNode;
    writerConfig.outputStream = m_outputAudioStream;
    writerConfig.timestampErrorPrefix = "encoded audio packet";
    writerConfig.writeErrorMessage = "encoded audio packet write failed";

    return m_packetWriter.initialize(std::move(writerConfig));
}

Status FFmpegAudioEncodePipeline::initializeResamplerAndFifo()
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

Status FFmpegAudioEncodePipeline::processPacket(
    AVPacket* packet,
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!packet) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioEncodePipeline processPacket failed: packet is null"));
    }

    Status status = sendPacketToDecoder(packet, onPacketWritten);
    m_processDiagnostics.maybeLog(
        m_packetWriter.packetCount(),
        m_fifo && m_fifo->isInitialized() ? m_fifo->size() : 0
    );
    return status;
}

Status FFmpegAudioEncodePipeline::sendPacketToDecoder(
    AVPacket* packet,
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioEncodePipeline sendPacketToDecoder failed: decoderCtx is null"));
    }

    auto step = m_processDiagnostics.mark();
    const int ret = avcodec_send_packet(m_decoderCtx.get(), packet);
    m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::DecoderSendPacket, step);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_packet audio decoder failed", ret));
    }

    return drainDecoder(onPacketWritten);
}

Status FFmpegAudioEncodePipeline::drainDecoder(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderCtx || !m_decodedFrame) {
        return Status::success();
    }

    while (true) {
        auto step = m_processDiagnostics.mark();
        const int ret = avcodec_receive_frame(m_decoderCtx.get(), m_decodedFrame.get());
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::DecoderReceiveFrame, step);

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

Status FFmpegAudioEncodePipeline::pushDecodedFrameToFifo(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!m_decodedFrame ||
        !m_swrCtx ||
        !m_fifo ||
        !m_fifo->isInitialized() ||
        !m_encoderCtx ||
        !m_decoderCtx) {
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

    auto step = m_processDiagnostics.mark();
    FramePtr convertedFrame = makeFrame();
    if (convertedFrame) {
        convertedFrame->nb_samples = dstNbSamples;
        convertedFrame->format = m_encoderCtx->sample_fmt;
        convertedFrame->sample_rate = m_encoderCtx->sample_rate;

        if (!setFrameAudioLayoutFromCodecContext(convertedFrame.get(), m_encoderCtx.get())) {
            m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
            return Status::failure(ErrorInfo::internalError(
                "set converted audio frame channel layout failed"));
        }

        const int ret = av_frame_get_buffer(convertedFrame.get(), 0);
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
        if (ret < 0) {
            return Status::failure(makeFFmpegError(
                "av_frame_get_buffer converted audio frame failed", ret));
        }
    }
    else {
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
        return Status::failure(makeAllocationError(
            "av_frame_alloc converted audio frame failed"));
    }

    step = m_processDiagnostics.mark();
    const int convertedSamples = swr_convert(
        m_swrCtx.get(),
        convertedFrame->extended_data,
        dstNbSamples,
        const_cast<const uint8_t**>(m_decodedFrame->extended_data),
        m_decodedFrame->nb_samples
    );
    m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::ResampleConvert, step);

    if (convertedSamples < 0) {
        return Status::failure(makeFFmpegError("swr_convert failed", convertedSamples));
    }

    convertedFrame->nb_samples = convertedSamples;

    if (convertedSamples > 0) {
        std::string fifoError;
        step = m_processDiagnostics.mark();
        Status fifoStatus = makeLegacyStatus(
            m_fifo->writeFrame(convertedFrame.get(), &fifoError),
            fifoError
        );
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FifoWrite, step);
        if (!fifoStatus) {
            return fifoStatus;
        }
    }

    return encodeFifo(false, onPacketWritten);
}

Status FFmpegAudioEncodePipeline::ensureInitialAudioPts()
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

    const int64_t inputAudioUs = TimelineNormalizer::toUs(
        inputAudioTs,
        m_inputAudioStream->time_base
    );
    const int64_t normalizedAudioUs = m_timeline
        ? m_timeline->normalizeUs(inputAudioUs)
        : AV_NOPTS_VALUE;

    if (normalizedAudioUs == AV_NOPTS_VALUE) {
        return makeTimestampError("failed to normalize input audio timestamp");
    }

    m_nextAudioPts = TimelineNormalizer::fromUs(normalizedAudioUs, m_encoderCtx->time_base);
    if (m_nextAudioPts == AV_NOPTS_VALUE || m_nextAudioPts < 0) {
        m_nextAudioPts = 0;
    }

    return Status::success();
}

Status FFmpegAudioEncodePipeline::encodeFifo(
    bool flushAll,
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!m_fifo || !m_fifo->isInitialized() || !m_encoderCtx) {
        return Status::success();
    }

    const int frameSize = m_encoderCtx->frame_size > 0 ? m_encoderCtx->frame_size : 1024;

    while (m_fifo->size() >= frameSize || (flushAll && m_fifo->size() > 0)) {
        const int availableSamples = m_fifo->size();
        const int samplesToRead = flushAll ? std::min(frameSize, availableSamples) : frameSize;

        auto step = m_processDiagnostics.mark();
        FramePtr audioFrame = makeFrame();
        if (audioFrame) {
            audioFrame->nb_samples = samplesToRead;
            audioFrame->format = m_encoderCtx->sample_fmt;
            audioFrame->sample_rate = m_encoderCtx->sample_rate;
            audioFrame->pts = m_nextAudioPts == AV_NOPTS_VALUE ? 0 : m_nextAudioPts;

            if (!setFrameAudioLayoutFromCodecContext(audioFrame.get(), m_encoderCtx.get())) {
                m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
                return Status::failure(ErrorInfo::internalError(
                    "set encoded audio frame channel layout failed"));
            }

            const int ret = av_frame_get_buffer(audioFrame.get(), 0);
            m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
            if (ret < 0) {
                return Status::failure(makeFFmpegError(
                    "av_frame_get_buffer encoded audio frame failed", ret));
            }
        }
        else {
            m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
            return Status::failure(makeAllocationError(
                "av_frame_alloc encoded audio frame failed"));
        }

        std::string fifoError;
        step = m_processDiagnostics.mark();
        Status fifoStatus = makeLegacyStatus(
            m_fifo->readToFrame(audioFrame.get(), samplesToRead, &fifoError),
            fifoError
        );
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FifoRead, step);
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

Status FFmpegAudioEncodePipeline::flushResampler()
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

        auto step = m_processDiagnostics.mark();
        FramePtr convertedFrame = makeFrame();
        if (convertedFrame) {
            convertedFrame->nb_samples = dstNbSamples;
            convertedFrame->format = m_encoderCtx->sample_fmt;
            convertedFrame->sample_rate = m_encoderCtx->sample_rate;

            if (!setFrameAudioLayoutFromCodecContext(convertedFrame.get(), m_encoderCtx.get())) {
                m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
                return Status::failure(ErrorInfo::internalError(
                    "set swr flush audio frame channel layout failed"));
            }

            const int ret = av_frame_get_buffer(convertedFrame.get(), 0);
            m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
            if (ret < 0) {
                return Status::failure(makeFFmpegError(
                    "av_frame_get_buffer swr flush audio frame failed", ret));
            }
        }
        else {
            m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FrameAlloc, step);
            return Status::failure(makeAllocationError(
                "av_frame_alloc swr flush audio frame failed"));
        }

        step = m_processDiagnostics.mark();
        const int convertedSamples = swr_convert(
            m_swrCtx.get(),
            convertedFrame->extended_data,
            dstNbSamples,
            nullptr,
            0
        );
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FlushResamplerConvert, step);

        if (convertedSamples < 0) {
            return Status::failure(makeFFmpegError(
                "swr_convert flush failed", convertedSamples));
        }

        convertedFrame->nb_samples = convertedSamples;

        if (convertedSamples <= 0) {
            break;
        }

        std::string fifoError;
        step = m_processDiagnostics.mark();
        Status fifoStatus = makeLegacyStatus(
            m_fifo->writeFrame(convertedFrame.get(), &fifoError),
            fifoError
        );
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::FifoWrite, step);
        if (!fifoStatus) {
            return fifoStatus;
        }
    }

    return Status::success();
}

Status FFmpegAudioEncodePipeline::flush(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (m_decoderCtx) {
        auto step = m_processDiagnostics.mark();
        const int ret = avcodec_send_packet(m_decoderCtx.get(), nullptr);
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::DecoderSendPacket, step);
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

    m_processDiagnostics.maybeLog(
        m_packetWriter.packetCount(),
        m_fifo && m_fifo->isInitialized() ? m_fifo->size() : 0,
        "flush"
    );

    return Status::success();
}

Status FFmpegAudioEncodePipeline::sendFrame(AVFrame* frame)
{
    if (!m_encoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioEncodePipeline sendFrame failed: encoderCtx is null"));
    }

    auto step = m_processDiagnostics.mark();
    const int ret = avcodec_send_frame(m_encoderCtx.get(), frame);
    m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::EncoderSendFrame, step);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_frame audio encoder failed", ret));
    }

    return Status::success();
}

Result<int> FFmpegAudioEncodePipeline::receiveAndWritePackets(
    const FFmpegAudioPacketWrittenCallback& onPacketWritten)
{
    if (!m_encoderCtx || !m_outputAudioStream || !m_packetWriter.isInitialized()) {
        return Result<int>::failure(ErrorInfo::notInitialized(
            "FFmpegAudioEncodePipeline receiveAndWritePackets failed: pipeline is not initialized"));
    }

    int packetsWritten = 0;

    while (true) {
        auto step = m_processDiagnostics.mark();
        PacketPtr packet = makePacket();
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::PacketAlloc, step);
        if (!packet) {
            return Result<int>::failure(makeAllocationError(
                "av_packet_alloc audio packet failed"));
        }

        step = m_processDiagnostics.mark();
        const int receiveRet = avcodec_receive_packet(m_encoderCtx.get(), packet.get());
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::EncoderReceivePacket, step);

        if (receiveRet == AVERROR(EAGAIN) || receiveRet == AVERROR_EOF) {
            break;
        }

        if (receiveRet < 0) {
            return Result<int>::failure(makeFFmpegError(
                "avcodec_receive_packet audio encoder failed", receiveRet));
        }

        av_packet_rescale_ts(packet.get(), m_encoderCtx->time_base, m_outputAudioStream->time_base);

        step = m_processDiagnostics.mark();
        const Status writeStatus = m_packetWriter.write(packet.get(), onPacketWritten);
        m_processDiagnostics.record(FFmpegAudioProcessDiagnostics::Step::PacketWrite, step);
        if (!writeStatus) {
            return Result<int>::failure(writeStatus.error());
        }

        ++packetsWritten;
    }

    return Result<int>::success(packetsWritten);
}

bool FFmpegAudioEncodePipeline::isInitialized() const
{
    return m_decoderCtx &&
        m_encoderCtx &&
        m_outputStreamProvider &&
        m_outputNode &&
        m_outputAudioStream &&
        m_packetWriter.isInitialized();
}

AudioMode FFmpegAudioEncodePipeline::mode() const
{
    return AudioMode::EncodeSelected;
}

AVStream* FFmpegAudioEncodePipeline::outputStream() const
{
    return m_outputAudioStream;
}

FFmpegAudioPacketProgress FFmpegAudioEncodePipeline::progress() const
{
    return FFmpegAudioPacketProgress{
        m_packetWriter.packetCount(),
        m_packetWriter.lastWrittenOutTimeMs()
    };
}

} // namespace media::ffmpeg
