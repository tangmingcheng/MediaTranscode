#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

MediaStreamKind outerStreamKind(MediaScheduledStream stream) noexcept
{
    switch (stream) {
    case MediaScheduledStream::Video:
        return MediaStreamKind::Video;
    case MediaScheduledStream::Audio:
        return MediaStreamKind::Audio;
    }
    return MediaStreamKind::Unknown;
}

} // namespace

::media::Result<MediaBufferRef> MediaTsAccessUnitBuffer::create(
    MediaBufferRef outer,
    MediaScheduledStream stream,
    std::uint64_t generation,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster,
    MediaRunningTime transportDecodeLead)
{
    auto packetBuffer = std::dynamic_pointer_cast<FFmpegPacketBuffer>(outer);
    if (!packetBuffer) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit requires an FFmpeg packet buffer"));
    }
    const AVPacket* packet = packetBuffer->packet();
    if (!packet || packet->size <= 0 || !packet->data) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit requires a non-empty packet payload"));
    }
    const MediaStreamKind requiredStream = outerStreamKind(stream);
    if (requiredStream == MediaStreamKind::Unknown ||
        packetBuffer->streamKind() != requiredStream) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit stream does not match its outer packet"));
    }
    if (generation == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit requires a positive generation"));
    }
    if (transportDecodeLead.nanoseconds() <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit requires a positive transport decode lead"));
    }
    const auto actualLead = dispatchOnMaster.checkedSubtract(emitOnMaster);
    if (!actualLead || actualLead.value() != transportDecodeLead) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS access unit does not match the planned transport decode lead"));
    }

    return ::media::Result<MediaBufferRef>::success(
        std::shared_ptr<MediaTsAccessUnitBuffer>(new MediaTsAccessUnitBuffer(
            std::move(packetBuffer), stream, generation, presentationOnMaster,
            dispatchOnMaster, emitOnMaster)));
}

MediaTsAccessUnitBuffer::MediaTsAccessUnitBuffer(
    std::shared_ptr<FFmpegPacketBuffer> outer,
    MediaScheduledStream stream,
    std::uint64_t generation,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster)
    : m_outer(std::move(outer))
    , m_stream(stream)
    , m_generation(generation)
    , m_presentationOnMaster(presentationOnMaster)
    , m_dispatchOnMaster(dispatchOnMaster)
    , m_emitOnMaster(emitOnMaster)
    , m_randomAccess(m_stream == MediaScheduledStream::Video &&
                     m_outer->isKeyFrame())
{
    setStreamKind(outerStreamKind(stream));
    setPayloadKind(MediaPayloadKind::TsAccessUnit);
    if (m_randomAccess) addFlags(MediaBufferFlag::KeyFrame);
}

MediaBufferType MediaTsAccessUnitBuffer::type() const noexcept
{
    return MediaBufferType::TsAccessUnit;
}

std::optional<std::uint64_t>
MediaTsAccessUnitBuffer::payloadFootprintBytes() const noexcept
{
    return m_outer->payloadFootprintBytes();
}

::media::Result<MediaTsAccessUnitView>
MediaTsAccessUnitBuffer::view() const noexcept
{
    const AVPacket* packet = m_outer->packet();
    if (!packet || !packet->data || packet->size <= 0) {
        return ::media::Result<MediaTsAccessUnitView>::failure(
            ::media::ErrorInfo::notInitialized(
                "MPEG-TS access unit outer packet is no longer available"));
    }
    return ::media::Result<MediaTsAccessUnitView>::success(MediaTsAccessUnitView{
        std::span<const std::uint8_t>(packet->data,
                                      static_cast<std::size_t>(packet->size)),
        m_stream,
        m_generation,
        m_presentationOnMaster,
        m_dispatchOnMaster,
        m_emitOnMaster,
        m_randomAccess});
}

} // namespace media::ffmpeg::graph
