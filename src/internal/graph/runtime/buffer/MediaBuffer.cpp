#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaBuffer::attachPayloadCredit(
    std::shared_ptr<MediaGraphPayloadCreditLease> credit) noexcept
{
    if (!credit || !*credit || m_payloadCredit) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "media buffer requires one active payload credit reservation"));
    }
    m_payloadCredit = std::move(credit);
    return ::media::Status::success();
}

const std::shared_ptr<MediaGraphPayloadCreditLease>&
MediaBuffer::payloadCredit() const noexcept
{
    return m_payloadCredit;
}

std::shared_ptr<MediaGraphPayloadCreditLease>
MediaBuffer::takePayloadCredit() noexcept
{
    return std::move(m_payloadCredit);
}

::media::Status MediaBuffer::sharePayloadCreditFrom(
    const MediaBuffer& source) noexcept
{
    if (m_payloadCredit || !source.m_payloadCredit) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "media buffer payload credit sharing requires one source allocation reservation"));
    }
    m_payloadCredit = source.m_payloadCredit;
    return ::media::Status::success();
}

std::optional<std::uint64_t> MediaBuffer::payloadFootprintBytes() const noexcept
{
    return std::nullopt;
}

MediaBufferFlag operator|(MediaBufferFlag lhs, MediaBufferFlag rhs) noexcept
{
    return static_cast<MediaBufferFlag>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

MediaBufferFlag operator&(MediaBufferFlag lhs, MediaBufferFlag rhs) noexcept
{
    return static_cast<MediaBufferFlag>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

MediaBufferFlag& operator|=(MediaBufferFlag& lhs, MediaBufferFlag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

bool hasFlag(MediaBufferFlag flags, MediaBufferFlag flag) noexcept
{
    return static_cast<uint32_t>(flags & flag) != 0;
}

MediaStreamKind MediaBuffer::streamKind() const noexcept
{
    return m_streamKind;
}

MediaPayloadKind MediaBuffer::payloadKind() const noexcept
{
    return m_payloadKind;
}

const MediaFormatDescriptor& MediaBuffer::formatDescriptor() const noexcept
{
    return m_format;
}

const MediaTimeDescriptor& MediaBuffer::timeDescriptor() const noexcept
{
    return m_time;
}

const MediaHardwareDescriptor& MediaBuffer::hardwareDescriptor() const noexcept
{
    return m_hardware;
}

MediaTimeValue MediaBuffer::pts() const noexcept
{
    return m_pts;
}

MediaTimeValue MediaBuffer::dts() const noexcept
{
    return m_dts;
}

MediaDuration MediaBuffer::duration() const noexcept
{
    return m_duration;
}

MediaBufferFlag MediaBuffer::flags() const noexcept
{
    return m_flags;
}

bool MediaBuffer::isEof() const noexcept
{
    return hasFlag(m_flags, MediaBufferFlag::Eof);
}

bool MediaBuffer::isFlush() const noexcept
{
    return hasFlag(m_flags, MediaBufferFlag::Flush);
}

bool MediaBuffer::isKeyFrame() const noexcept
{
    return hasFlag(m_flags, MediaBufferFlag::KeyFrame);
}

bool MediaBuffer::isHardwareBacked() const noexcept
{
    return hasFlag(m_flags, MediaBufferFlag::HardwareBacked) || m_hardware.isHardwareBacked();
}

const std::string& MediaBuffer::diagnosticName() const noexcept
{
    return m_diagnosticName;
}

void MediaBuffer::setStreamKind(MediaStreamKind streamKind) noexcept
{
    m_streamKind = streamKind;
}

void MediaBuffer::setPayloadKind(MediaPayloadKind payloadKind) noexcept
{
    m_payloadKind = payloadKind;
}

void MediaBuffer::setFormatDescriptor(MediaFormatDescriptor descriptor)
{
    m_format = std::move(descriptor);
}

void MediaBuffer::setTimeDescriptor(MediaTimeDescriptor descriptor) noexcept
{
    m_time = descriptor;
}

void MediaBuffer::setHardwareDescriptor(MediaHardwareDescriptor descriptor)
{
    m_hardware = std::move(descriptor);
}

void MediaBuffer::setTimestamps(MediaTimeValue pts, MediaTimeValue dts, MediaDuration duration) noexcept
{
    m_pts = pts;
    m_dts = dts;
    m_duration = duration;
}

void MediaBuffer::setFlags(MediaBufferFlag flags) noexcept
{
    m_flags = flags;
}

void MediaBuffer::addFlags(MediaBufferFlag flags) noexcept
{
    m_flags |= flags;
}

void MediaBuffer::setDiagnosticName(std::string name)
{
    m_diagnosticName = std::move(name);
}

} // namespace media::ffmpeg::graph
