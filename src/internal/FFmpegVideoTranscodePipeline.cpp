#include "internal/FFmpegVideoTranscodePipeline.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

FFmpegVideoTranscodePipeline::~FFmpegVideoTranscodePipeline()
{
    reset();
}

void FFmpegVideoTranscodePipeline::reset()
{
    m_filterStage.reset();
    m_packetWriter.reset();
    m_hardwareTransferStage.reset();
    m_frameRoutingStrategy.reset();
    m_encoderStage.reset();
    m_decoderStage.reset();
    m_timestampStage.reset();

    if (m_softwareTransferFrame) {
        av_frame_free(&m_softwareTransferFrame);
    }

    if (m_filteredFrame) {
        av_frame_free(&m_filteredFrame);
    }

    if (m_decodedFrame) {
        av_frame_free(&m_decodedFrame);
    }

    m_config = TranscodeConfig{};
    m_inputVideoStream = nullptr;
    m_outputFmtCtx = nullptr;
    m_inputMetadata = FFmpegVideoInputMetadata{};
    m_hardwarePlan = HardwarePipelinePlan{};
    m_hasHardwarePlan = false;
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

    m_config = *config.transcodeConfig;
    m_inputVideoStream = config.inputVideoStream;
    m_outputFmtCtx = config.outputFmtCtx;

    if (config.hardwarePlan &&
        config.hardwarePlan->valid &&
        config.hardwarePlan->executionMode != VideoExecutionMode::Cpu) {
        m_hardwarePlan = *config.hardwarePlan;
        m_hasHardwarePlan = true;
    }

    return openDecoder(error) &&
        collectVideoInputMetadata(error) &&
        initializeTimestampStage(config.timeline, error) &&
        openEncoder(error) &&
        initializeFrameRoutingStrategy(error) &&
        initializeHardwareTransferStage(error) &&
        initializeFilterStage(error) &&
        initializePacketWriter(error) &&
        allocateFrames(error);
}

bool FFmpegVideoTranscodePipeline::initializeTimestampStage(
    TimelineNormalizer* timeline,
    std::string* error)
{
    FFmpegVideoTimestampStage::Config config;
    config.inputMetadata = m_inputMetadata;
    config.timeline = timeline;
    return m_timestampStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::openDecoder(std::string* error)
{
    FFmpegVideoDecoderStage::Config config;
    config.inputStream = m_inputVideoStream;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    return m_decoderStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::collectVideoInputMetadata(std::string* error)
{
    AVCodecContext* decoderCtx = m_decoderStage.context();
    if (!decoderCtx) {
        if (error) {
            *error = "collect video input metadata failed: decoder stage is not initialized";
        }
        return false;
    }

    m_inputMetadata = FFmpegVideoInputMetadata::fromDecoderContextAndStream(
        decoderCtx,
        m_inputVideoStream
    );

    if (!m_inputMetadata.hasValidSize()) {
        if (error) {
            *error = "collect video input metadata failed: invalid input video size";
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTranscodePipeline::openEncoder(std::string* error)
{
    FFmpegVideoEncoderStage::Config config;
    config.transcodeConfig = &m_config;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    config.inputMetadata = m_inputMetadata;
    config.outputFmtCtx = m_outputFmtCtx;
    config.hardwareDeviceContext = &m_decoderStage.hardwareDeviceContext();
    config.decoderUsesHardwareFrames = m_decoderStage.usesHardwareFrames();
    config.decoderHardwareDeviceAttached = m_decoderStage.hardwareDeviceAttached();

    return m_encoderStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializeFrameRoutingStrategy(std::string* error)
{
    FFmpegVideoFrameRoutingStrategy::Config config;
    config.executionMode = m_hasHardwarePlan
        ? m_hardwarePlan.executionMode
        : VideoExecutionMode::Cpu;
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();
    config.decoderUsesHardwareFrames = m_decoderStage.usesHardwareFrames();
    config.hardwareDecoderConfig = m_decoderStage.hardwareDecoderConfig();

    return m_frameRoutingStrategy.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializeHardwareTransferStage(std::string* error)
{
    FFmpegVideoHardwareTransferStage::Config config;
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();
    return m_hardwareTransferStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializeFilterStage(std::string* error)
{
    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        if (error) {
            *error = "initialize filter stage failed: encoder stage is not initialized";
        }
        return false;
    }

    FFmpegVideoFilterStage::Config config;
    config.encoderCtx = encoderCtx;
    config.inputMetadata = m_inputMetadata;
    config.outputFps = m_encoderStage.outputFps();
    config.enableConstantFps = m_encoderStage.enableConstantFps();
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();
    config.hardwareBackend = m_encoderStage.hardwareBackend();

    return m_filterStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializePacketWriter(std::string* error)
{
    FFmpegVideoPacketWriterStage::Config config;
    config.encoderCtx = encoderContext();
    config.outputFmtCtx = m_outputFmtCtx;
    config.outputVideoStream = m_encoderStage.outputStream();

    return m_packetWriter.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::allocateFrames(std::string* error)
{
    m_decodedFrame = av_frame_alloc();
    m_filteredFrame = av_frame_alloc();
    m_softwareTransferFrame = av_frame_alloc();

    if (!m_decodedFrame || !m_filteredFrame || !m_softwareTransferFrame) {
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
    if (!m_decoderStage.sendPacket(packet, error)) {
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderStage.isInitialized()) {
        return true;
    }

    if (!m_decoderStage.sendFlush(error)) {
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushFilterAndEncoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_filterStage.flush(error)) {
        return false;
    }

    if (!drainFilterStage(error, onPacketWritten)) {
        return false;
    }

    return writeEncodedPackets(nullptr, error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::drainDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        const int receiveRet = m_decoderStage.receiveFrame(m_decodedFrame, error);
        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
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
    const FFmpegVideoFrameRoutingStrategy::Decision decision =
        m_frameRoutingStrategy.decide(m_decodedFrame);

    switch (decision.route) {
    case FFmpegVideoFrameRoutingStrategy::Route::HardwareZeroCopy:
        return processHardwareFrameZeroCopy(error, onPacketWritten);

    case FFmpegVideoFrameRoutingStrategy::Route::HardwareTransferThenSoftwareFilter:
        if (!m_hardwareTransferStage.transferToSoftware(
                m_decodedFrame,
                m_softwareTransferFrame,
                error)) {
            return false;
        }
        {
            const bool ok = processFrameThroughSoftwareFilter(
                m_softwareTransferFrame,
                error,
                onPacketWritten
            );
            av_frame_unref(m_softwareTransferFrame);
            return ok;
        }

    case FFmpegVideoFrameRoutingStrategy::Route::SoftwareFilter:
        return processFrameThroughSoftwareFilter(
            m_decodedFrame,
            error,
            onPacketWritten
        );

    case FFmpegVideoFrameRoutingStrategy::Route::Invalid:
    default:
        if (error) {
            *error = decision.error.empty()
                ? "frame routing strategy returned invalid route"
                : decision.error;
        }
        return false;
    }
}

bool FFmpegVideoTranscodePipeline::processHardwareFrameZeroCopy(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_timestampStage.normalizeFramePts(m_decodedFrame, error)) {
        return false;
    }

    if (!m_filterStage.sendHardwareFrame(m_decodedFrame, error)) {
        return false;
    }

    return drainFilterStage(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::processFrameThroughSoftwareFilter(
    AVFrame* frame,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_timestampStage.normalizeFramePts(frame, error)) {
        return false;
    }

    if (!m_filterStage.sendSoftwareFrame(frame, error)) {
        return false;
    }

    return drainFilterStage(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::drainFilterStage(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        const int receiveRet = m_filterStage.receiveFrame(m_filteredFrame, error);

        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
            return false;
        }

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

    return m_packetWriter.receiveAndWritePackets(error, onPacketWritten) >= 0;
}

AVCodecContext* FFmpegVideoTranscodePipeline::encoderContext() const
{
    return m_encoderStage.context();
}

bool FFmpegVideoTranscodePipeline::isInitialized() const
{
    return m_timestampStage.isInitialized() &&
        m_decoderStage.isInitialized() &&
        m_encoderStage.isInitialized() &&
        m_frameRoutingStrategy.isInitialized() &&
        m_hardwareTransferStage.isInitialized() &&
        m_filterStage.isInitialized();
}

AVStream* FFmpegVideoTranscodePipeline::outputStream() const
{
    return m_encoderStage.outputStream();
}

int64_t FFmpegVideoTranscodePipeline::packetCount() const
{
    return m_packetWriter.packetCount();
}

int64_t FFmpegVideoTranscodePipeline::lastWrittenOutTimeMs() const
{
    return m_packetWriter.lastWrittenOutTimeMs();
}

int64_t FFmpegVideoTranscodePipeline::estimatedOutTimeMs() const
{
    if (m_packetWriter.lastWrittenOutTimeMs() > 0) {
        return m_packetWriter.lastWrittenOutTimeMs();
    }

    AVCodecContext* encoderCtx = encoderContext();
    if (encoderCtx && encoderCtx->framerate.num > 0) {
        return static_cast<int64_t>(
            m_packetWriter.packetCount() * 1000.0 *
            encoderCtx->framerate.den /
            encoderCtx->framerate.num
        );
    }

    return 0;
}

} // namespace media::ffmpeg
