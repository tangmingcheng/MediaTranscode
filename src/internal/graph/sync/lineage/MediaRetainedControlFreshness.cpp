#include "internal/graph/sync/lineage/MediaRetainedControlFreshness.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

namespace media::ffmpeg::graph {
namespace {

bool isExactTerminalControl(
    const MediaBufferRef& buffer,
    MediaStreamKind expectedStream) noexcept
{
    const auto* control =
        dynamic_cast<const MediaControlBuffer*>(buffer.get());
    if (!control || buffer->type() != MediaBufferType::Control ||
        buffer->payloadKind() != MediaPayloadKind::ControlSignal ||
        (buffer->streamKind() != expectedStream &&
         buffer->streamKind() != MediaStreamKind::Control)) {
        return false;
    }
    if (control->controlKind() == MediaControlBufferKind::Eof) {
        return buffer->isEof() && !buffer->isFlush();
    }
    if (control->controlKind() == MediaControlBufferKind::Flush) {
        return buffer->isFlush() && !buffer->isEof();
    }
    return false;
}

} // namespace

bool MediaRetainedControlFreshness::isCandidate(
    const MediaBufferRef& buffer) noexcept
{
    return buffer &&
        (buffer->type() == MediaBufferType::Control || buffer->isEof() ||
         buffer->isFlush() ||
         buffer->payloadKind() == MediaPayloadKind::ControlSignal);
}

::media::Status MediaRetainedControlFreshness::capture(
    const MediaBufferRef& buffer,
    std::uint64_t generation,
    MediaStreamKind expectedStream)
{
    if (generation == 0 || expectedStream == MediaStreamKind::Control ||
        expectedStream == MediaStreamKind::Unknown ||
        expectedStream == MediaStreamKind::Any ||
        !isExactTerminalControl(buffer, expectedStream)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Retained terminal requires exact stream control and observed generation"));
    }
    m_buffer = buffer;
    m_generation = generation;
    return ::media::Status::success();
}

bool MediaRetainedControlFreshness::matches(
    const MediaBufferRef& buffer,
    std::uint64_t generation,
    MediaStreamKind expectedStream) const noexcept
{
    const auto retained = m_buffer.lock();
    return generation != 0 && generation == m_generation && retained &&
           retained == buffer &&
           isExactTerminalControl(buffer, expectedStream);
}

void MediaRetainedControlFreshness::clear() noexcept
{
    m_buffer.reset();
    m_generation = 0;
}

} // namespace media::ffmpeg::graph
