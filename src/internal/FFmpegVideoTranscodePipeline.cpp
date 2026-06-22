#include "internal/FFmpegVideoTranscodePipeline.h"

#include "internal/FFmpegError.h"
#include "internal/FFmpegVideoFlushDiagnostics.h"

#include <cstdint>
#include <string>

namespace media::ffmpeg {

FFmpegVideoTranscodePipeline::~FFmpegVideoTranscodePipeline()
{
    reset();
}

void FFmpegVideoTranscodePipeline::reset()
{
    m_filterStage.reset();
    m_frameRateStage.reset();
    m_packetWriter.reset();
    m_hardwareTransferStage.reset();
    m_frameRoutingStrategy.reset();
    m_encoderStage.reset();
    m_decoderStage.reset();
    m_timestampStage.reset();

    m_softwareTransferFrame.reset();
    m_filteredFrame.reset();
    m_frameRateFrame.reset();
    m_decodedFrame.reset();

    m_config = TranscodeConfig{};
    m_inputVideoStream = nullptr;
    m_outputFmtCtx = nullptr;
    m_inputMetadata = FFmpegVideoInputMetadata{};
    m_hardwarePlan = HardwarePipelinePlan{};
    m_hasHardwarePlan = false;
}

Status FFmpegVideoTranscodePipeline::initialize(const Config& config)
{
    reset();

    if (!config.transcodeConfig) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoTranscodePipeline initialize failed: transcodeConfig is null"));
    }

    if (!config.inputVideoStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoTranscodePipeline initialize failed: inputVideoStream is null"));
    }

    if (!config.outputFmtCtx) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoTranscodePipeline initialize failed: outputFmtCtx is null"));
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

    Status status = openDecoder();
    if (!status) {
        return status;
    }

    status = collectVideoInputMetadata();
    if (!status) {
        return status;
    }

    status = initializeTimestampStage(config.timeline);
    if (!status) {
        return status;
    }

    status = openEncoder();
    if (!status) {
        return status;
    }

    status = initializeFrameRoutingStrategy();
    if (!status) {
        return status;
    }

    status = initializeHardwareTransferStage();
    if (!status) {
        return status;
    }

    status = initializeFrameRateStage();
    if (!status) {
        return status;
    }

    status = initializeFilterStage();
    if (!status) {
        return status;
    }

    status = initializePacketWriter();
    if (!status) {
        return status;
    }

    return allocateFrames();
}

Status FFmpegVideoTranscodePipeline::initializeTimestampStage(
    TimelineNormalizer* timeline)
{
    FFmpegVideoTimestampStage::Config config;
    config.inputMetadata = m_inputMetadata;
    config.timeline = timeline;

    std::string error;
    return makeLegacyStatus(m_timestampStage.initialize(config, &error), error);
}

Status FFmpegVideoTranscodePipeline::openDecoder()
{
    FFmpegVideoDecoderStage::Config config;
    config.inputStream = m_inputVideoStream;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    return m_decoderStage.initialize(config);
}

Status FFmpegVideoTranscodePipeline::collectVideoInputMetadata()
{
    AVCodecContext* decoderCtx = m_decoderStage.context();
    if (!decoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "collect video input metadata failed: decoder stage is not initialized"));
    }

    m_inputMetadata = FFmpegVideoInputMetadata::fromDecoderContextAndStream(
        decoderCtx,
        m_inputVideoStream
    );

    if (!m_inputMetadata.hasValidSize()) {
        return Status::failure(ErrorInfo::invalidArgument(
            "collect video input metadata failed: invalid input video size"));
    }

    return Status::success();
}

Status FFmpegVideoTranscodePipeline::openEncoder()
{
    FFmpegVideoEncoderStage::Config config;
    config.transcodeConfig = &m_config;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    config.inputMetadata = m_inputMetadata;
    config.outputFmtCtx = m_outputFmtCtx;
    config.hardwareDeviceContext = &m_decoderStage.hardwareDeviceContext();
    config.decoderUsesHardwareFrames = m_decoderStage.usesHardwareFrames();
    config.decoderHardwareDeviceAttached = m_decoderStage.hardwareDeviceAttached();

    return m_encoderStage.initialize(config);
}

Status FFmpegVideoTranscodePipeline::initializeFrameRoutingStrategy()
{
    FFmpegVideoFrameRoutingStrategy::Config config;
    config.executionMode = m_hasHardwarePlan
        ? m_hardwarePlan.executionMode
        : VideoExecutionMode::Cpu;
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();
    config.decoderUsesHardwareFrames = m_decoderStage.usesHardwareFrames();
    config.hardwareDecoderConfig = m_decoderStage.hardwareDecoderConfig();

    std::string error;
    return makeLegacyStatus(m_frameRoutingStrategy.initialize(config, &error), error);
}

Status FFmpegVideoTranscodePipeline::initializeHardwareTransferStage()
{
    FFmpegVideoHardwareTransferStage::Config config;
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();

    std::string error;
    return makeLegacyStatus(m_hardwareTransferStage.initialize(config, &error), error);
}

Status FFmpegVideoTranscodePipeline::initializeFrameRateStage()
{
    FFmpegVideoFrameRateStage::Config config;
    config.inputTimeBase = m_inputMetadata.timeBase;
    config.targetFps = m_config.fps;

    std::string error;
    return makeLegacyStatus(m_frameRateStage.initialize(config, &error), error);
}

Status FFmpegVideoTranscodePipeline::initializeFilterStage()
{
    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "initialize filter stage failed: encoder stage is not initialized"));
    }

    FFmpegVideoFilterStage::Config config;
    config.encoderCtx = encoderCtx;
    config.inputMetadata = m_inputMetadata;
    config.outputFps = m_encoderStage.outputFps();
    config.enableConstantFps = false;
    config.zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();
    config.hardwareBackend = m_encoderStage.hardwareBackend();

    std::string error;
    return makeLegacyStatus(m_filterStage.initialize(config, &error), error);
}

Status FFmpegVideoTranscodePipeline::initializePacketWriter()
{
    FFmpegVideoPacketWriterStage::Config config;
    config.encoderCtx = encoderContext();
    config.outputFmtCtx = m_outputFmtCtx;
    config.outputVideoStream = m_encoderStage.outputStream();

    return m_packetWriter.initialize(config);
}

Status FFmpegVideoTranscodePipeline::allocateFrames()
{
    m_decodedFrame = makeFrame();
    m_frameRateFrame = makeFrame();
    m_filteredFrame = makeFrame();
    m_softwareTransferFrame = makeFrame();

    if (!m_decodedFrame || !m_frameRateFrame || !m_filteredFrame || !m_softwareTransferFrame) {
        m_decodedFrame.reset();
        m_frameRateFrame.reset();
        m_filteredFrame.reset();
        m_softwareTransferFrame.reset();
        return Status::failure(makeAllocationError(
            "av_frame_alloc video frame failed"));
    }

    return Status::success();
}

Status FFmpegVideoTranscodePipeline::processPacket(
    AVPacket* packet,
    const PacketWrittenCallback& onPacketWritten)
{
    Status status = m_decoderStage.sendPacket(packet);
    if (!status) {
        return status;
    }

    return drainDecoder(onPacketWritten);
}

Status FFmpegVideoTranscodePipeline::flushDecoder(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderStage.isInitialized()) {
        return Status::success();
    }

    const int64_t packetsBefore = m_packetWriter.packetCount();
    FFmpegVideoFlushDiagnostics::Session diagnostics(
        FFmpegVideoFlushDiagnostics::makeContext(
            "DECODER",
            m_hasHardwarePlan,
            m_hardwarePlan,
            m_encoderStage.zeroCopyPipeline(),
            packetsBefore
        )
    );

    auto step = diagnostics.mark();
    Status status = m_decoderStage.sendFlush();
    if (!status) {
        diagnostics.logFailure("send_flush", step);
        diagnostics.finish(m_packetWriter.packetCount(), false);
        return status;
    }
    diagnostics.logStep("send_flush", step);

    step = diagnostics.mark();
    status = drainDecoder(onPacketWritten);
    const int64_t packetsAfter = m_packetWriter.packetCount();
    diagnostics.logStepPackets("drain", step, packetsAfter - packetsBefore);
    diagnostics.finish(packetsAfter, status.ok());

    return status;
}

Status FFmpegVideoTranscodePipeline::flushFilterAndEncoder(
    const PacketWrittenCallback& onPacketWritten)
{
    const int64_t packetsBeforeAll = m_packetWriter.packetCount();
    FFmpegVideoFlushDiagnostics::Session diagnostics(
        FFmpegVideoFlushDiagnostics::makeContext(
            "FILTER_ENCODER",
            m_hasHardwarePlan,
            m_hardwarePlan,
            m_encoderStage.zeroCopyPipeline(),
            packetsBeforeAll
        )
    );

    std::string error;

    auto step = diagnostics.mark();
    if (!m_frameRateStage.flush(&error)) {
        diagnostics.logFailure("frame_rate_flush", step);
        diagnostics.finish(m_packetWriter.packetCount(), false);
        return Status::failure(makeLegacyError(error));
    }
    diagnostics.logStep("frame_rate_flush", step);

    int64_t packetsBeforeStep = m_packetWriter.packetCount();
    step = diagnostics.mark();
    Status status = drainFrameRateStage(m_encoderStage.zeroCopyPipeline(), onPacketWritten);
    diagnostics.logStepPackets(
        "frame_rate_drain",
        step,
        m_packetWriter.packetCount() - packetsBeforeStep
    );
    if (!status) {
        diagnostics.finish(m_packetWriter.packetCount(), false);
        return status;
    }

    error.clear();
    step = diagnostics.mark();
    if (!m_filterStage.flush(&error)) {
        diagnostics.logFailure("filter_flush", step);
        diagnostics.finish(m_packetWriter.packetCount(), false);
        return Status::failure(makeLegacyError(error));
    }
    diagnostics.logStep("filter_flush", step);

    packetsBeforeStep = m_packetWriter.packetCount();
    step = diagnostics.mark();
    status = drainFilterStage(onPacketWritten);
    diagnostics.logStepPackets(
        "filter_drain",
        step,
        m_packetWriter.packetCount() - packetsBeforeStep
    );
    if (!status) {
        diagnostics.finish(m_packetWriter.packetCount(), false);
        return status;
    }

    packetsBeforeStep = m_packetWriter.packetCount();
    step = diagnostics.mark();
    status = writeEncodedPackets(nullptr, onPacketWritten);
    diagnostics.logStepPackets(
        "encoder_flush",
        step,
        m_packetWriter.packetCount() - packetsBeforeStep
    );
    diagnostics.finish(m_packetWriter.packetCount(), status.ok());

    return status;
}

Status FFmpegVideoTranscodePipeline::drainDecoder(
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        auto receiveResult = m_decoderStage.receiveFrame(m_decodedFrame.get());
        if (!receiveResult) {
            return Status::failure(receiveResult.error());
        }

        if (receiveResult.value() == FFmpegVideoDecoderStage::ReceiveFrameState::NeedMoreInput) {
            return Status::success();
        }

        const Status status = processDecodedFrame(onPacketWritten);
        av_frame_unref(m_decodedFrame.get());

        if (!status) {
            return status;
        }
    }
}

Status FFmpegVideoTranscodePipeline::processDecodedFrame(
    const PacketWrittenCallback& onPacketWritten)
{
    const FFmpegVideoFrameRoutingStrategy::Decision decision =
        m_frameRoutingStrategy.decide(m_decodedFrame.get());

    switch (decision.route) {
    case FFmpegVideoFrameRoutingStrategy::Route::HardwareZeroCopy:
        return processHardwareFrameZeroCopy(onPacketWritten);

    case FFmpegVideoFrameRoutingStrategy::Route::HardwareTransferThenSoftwareFilter:
    {
        std::string error;
        if (!m_hardwareTransferStage.transferToSoftware(
                m_decodedFrame.get(),
                m_softwareTransferFrame.get(),
                &error)) {
            return Status::failure(makeLegacyError(error));
        }

        const Status status = processFrameThroughSoftwareFilter(
            m_softwareTransferFrame.get(),
            onPacketWritten
        );
        av_frame_unref(m_softwareTransferFrame.get());
        return status;
    }

    case FFmpegVideoFrameRoutingStrategy::Route::SoftwareFilter:
        return processFrameThroughSoftwareFilter(
            m_decodedFrame.get(),
            onPacketWritten
        );

    case FFmpegVideoFrameRoutingStrategy::Route::Invalid:
    default:
        return Status::failure(makeLegacyError(
            decision.error.empty()
                ? "frame routing strategy returned invalid route"
                : decision.error));
    }
}

Status FFmpegVideoTranscodePipeline::processHardwareFrameZeroCopy(
    const PacketWrittenCallback& onPacketWritten)
{
    std::string error;
    if (!m_timestampStage.normalizeFramePts(m_decodedFrame.get(), &error)) {
        return Status::failure(makeLegacyError(error));
    }

    error.clear();
    if (!m_frameRateStage.sendFrame(m_decodedFrame.get(), &error)) {
        return Status::failure(makeLegacyError(error));
    }

    return drainFrameRateStage(true, onPacketWritten);
}

Status FFmpegVideoTranscodePipeline::processFrameThroughSoftwareFilter(
    AVFrame* frame,
    const PacketWrittenCallback& onPacketWritten)
{
    std::string error;
    if (!m_timestampStage.normalizeFramePts(frame, &error)) {
        return Status::failure(makeLegacyError(error));
    }

    error.clear();
    if (!m_frameRateStage.sendFrame(frame, &error)) {
        return Status::failure(makeLegacyError(error));
    }

    return drainFrameRateStage(false, onPacketWritten);
}

Status FFmpegVideoTranscodePipeline::drainFrameRateStage(
    bool hardwareFrame,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        std::string error;
        const int receiveRet = m_frameRateStage.receiveFrame(m_frameRateFrame.get(), &error);

        if (receiveRet == 0) {
            return Status::success();
        }

        if (receiveRet < 0) {
            return Status::failure(makeLegacyError(error));
        }

        const Status status = sendFrameRateOutputToFilter(
            m_frameRateFrame.get(),
            hardwareFrame,
            onPacketWritten
        );
        av_frame_unref(m_frameRateFrame.get());

        if (!status) {
            return status;
        }
    }
}

Status FFmpegVideoTranscodePipeline::sendFrameRateOutputToFilter(
    AVFrame* frame,
    bool hardwareFrame,
    const PacketWrittenCallback& onPacketWritten)
{
    std::string error;
    const bool ok = hardwareFrame
        ? m_filterStage.sendHardwareFrame(frame, &error)
        : m_filterStage.sendSoftwareFrame(frame, &error);

    if (!ok) {
        return Status::failure(makeLegacyError(error));
    }

    return drainFilterStage(onPacketWritten);
}

Status FFmpegVideoTranscodePipeline::drainFilterStage(
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        std::string error;
        const int receiveRet = m_filterStage.receiveFrame(m_filteredFrame.get(), &error);

        if (receiveRet == 0) {
            return Status::success();
        }

        if (receiveRet < 0) {
            return Status::failure(makeLegacyError(error));
        }

        const Status status = writeEncodedPackets(m_filteredFrame.get(), onPacketWritten);
        av_frame_unref(m_filteredFrame.get());

        if (!status) {
            return status;
        }
    }
}

Status FFmpegVideoTranscodePipeline::writeEncodedPackets(
    AVFrame* frame,
    const PacketWrittenCallback& onPacketWritten)
{
    Status sendStatus = m_packetWriter.sendFrame(frame);
    if (!sendStatus) {
        return sendStatus;
    }

    auto receiveResult = m_packetWriter.receiveAndWritePackets(onPacketWritten);
    if (!receiveResult) {
        return Status::failure(receiveResult.error());
    }

    return Status::success();
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
        m_frameRateStage.isInitialized() &&
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
