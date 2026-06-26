#include "internal/FFmpegAudioPipeline.h"

#include "internal/FFmpegAudioCopyPipeline.h"
#include "internal/FFmpegAudioEncodePipeline.h"
#include "internal/FFmpegAudioPipelineStrategy.h"

namespace media::ffmpeg {

const char* audioPipelineModeName(FFmpegAudioPipelineMode mode)
{
    switch (mode) {
    case FFmpegAudioPipelineMode::None:
        return "none";
    case FFmpegAudioPipelineMode::Copy:
        return "copy";
    case FFmpegAudioPipelineMode::Encode:
        return "encode";
    default:
        return "unknown";
    }
}

struct FFmpegAudioPipeline::Impl {
    FFmpegAudioPipelineMode mode = FFmpegAudioPipelineMode::None;
    std::unique_ptr<IFFmpegAudioPipelineStrategy> strategy;

    FFmpegAudioPacketProgress progress() const
    {
        return strategy ? strategy->progress() : FFmpegAudioPacketProgress{};
    }
};

FFmpegAudioPipeline::FFmpegAudioPipeline()
    : m_impl(std::make_unique<Impl>())
{
}

FFmpegAudioPipeline::~FFmpegAudioPipeline() = default;

FFmpegAudioPipeline::FFmpegAudioPipeline(FFmpegAudioPipeline&& other) noexcept = default;

FFmpegAudioPipeline& FFmpegAudioPipeline::operator=(FFmpegAudioPipeline&& other) noexcept = default;

void FFmpegAudioPipeline::reset()
{
    if (!m_impl) {
        m_impl = std::make_unique<Impl>();
        return;
    }

    if (m_impl->strategy) {
        m_impl->strategy->reset();
    }

    m_impl->strategy.reset();
    m_impl->mode = FFmpegAudioPipelineMode::None;
}

Status FFmpegAudioPipeline::initialize(const Config& config)
{
    reset();

    if (config.mode == FFmpegAudioPipelineMode::None) {
        m_impl->mode = FFmpegAudioPipelineMode::None;
        return Status::success();
    }

    if (!config.inputAudioStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: inputAudioStream is null"));
    }

    if (!config.outputStreamProvider) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: outputStreamProvider is null"));
    }

    if (!config.outputNode) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: outputNode is null"));
    }

    if (!config.timeline) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: timeline is null"));
    }

    switch (config.mode) {
    case FFmpegAudioPipelineMode::Copy:
        m_impl->strategy = std::make_unique<FFmpegAudioCopyPipeline>();
        break;

    case FFmpegAudioPipelineMode::Encode:
        m_impl->strategy = std::make_unique<FFmpegAudioEncodePipeline>();
        break;

    case FFmpegAudioPipelineMode::None:
        break;

    default:
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegAudioPipeline initialize failed: unknown audio pipeline mode"));
    }

    if (!m_impl->strategy) {
        return Status::failure(ErrorInfo::internalError(
            "FFmpegAudioPipeline initialize failed: no strategy selected"));
    }

    Status status = m_impl->strategy->initialize(config);
    if (!status) {
        reset();
        return status;
    }

    m_impl->mode = config.mode;
    return Status::success();
}

Status FFmpegAudioPipeline::processPacket(
    AVPacket* packet,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_impl || m_impl->mode == FFmpegAudioPipelineMode::None) {
        return Status::success();
    }

    if (!m_impl->strategy) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline processPacket failed: strategy is not initialized"));
    }

    return m_impl->strategy->processPacket(packet, onPacketWritten);
}

Status FFmpegAudioPipeline::flush(const PacketWrittenCallback& onPacketWritten)
{
    if (!m_impl || m_impl->mode == FFmpegAudioPipelineMode::None) {
        return Status::success();
    }

    if (!m_impl->strategy) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline flush failed: strategy is not initialized"));
    }

    return m_impl->strategy->flush(onPacketWritten);
}

Status FFmpegAudioPipeline::sendFrame(AVFrame* frame)
{
    if (!m_impl || !m_impl->strategy) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline sendFrame failed: strategy is not initialized"));
    }

    return m_impl->strategy->sendFrame(frame);
}

Result<int> FFmpegAudioPipeline::receiveAndWritePackets(
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_impl || !m_impl->strategy) {
        return Result<int>::failure(ErrorInfo::notInitialized(
            "FFmpegAudioPipeline receiveAndWritePackets failed: strategy is not initialized"));
    }

    return m_impl->strategy->receiveAndWritePackets(onPacketWritten);
}

bool FFmpegAudioPipeline::isInitialized() const
{
    if (!m_impl) {
        return false;
    }

    return m_impl->mode == FFmpegAudioPipelineMode::None ||
        (m_impl->strategy && m_impl->strategy->isInitialized());
}

FFmpegAudioPipelineMode FFmpegAudioPipeline::mode() const
{
    return m_impl ? m_impl->mode : FFmpegAudioPipelineMode::None;
}

AVStream* FFmpegAudioPipeline::outputStream() const
{
    return (m_impl && m_impl->strategy) ? m_impl->strategy->outputStream() : nullptr;
}

int64_t FFmpegAudioPipeline::packetCount() const
{
    return m_impl ? m_impl->progress().packetCount : 0;
}

int64_t FFmpegAudioPipeline::lastWrittenOutTimeMs() const
{
    return m_impl ? m_impl->progress().lastWrittenOutTimeMs : 0;
}

} // namespace media::ffmpeg
