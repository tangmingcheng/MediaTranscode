#pragma once

#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaBufferType {
    Unknown,
    FormatContext,
    CodecContext,
    CodecParameters,
    Packet,
    Frame,
    HardwareFrame,
    Control,
    Event,
    OutputByteSink,
    ProjectMpegTsRuntimePlan,
    TsAccessUnit,
    RawRtpPreparedInput,
    ScheduledDatagramBatch,
    WireDatagramBatch,
    ScheduledWireDatagramBatch,
    DatagramShapingPlan
};

enum class MediaBufferFlag : uint32_t {
    None = 0,
    Eof = 1u << 0,
    Flush = 1u << 1,
    KeyFrame = 1u << 2,
    Discontinuity = 1u << 3,
    Corrupt = 1u << 4,
    Dropped = 1u << 5,
    HardwareBacked = 1u << 6
};

MediaBufferFlag operator|(MediaBufferFlag lhs, MediaBufferFlag rhs) noexcept;
MediaBufferFlag operator&(MediaBufferFlag lhs, MediaBufferFlag rhs) noexcept;
MediaBufferFlag& operator|=(MediaBufferFlag& lhs, MediaBufferFlag rhs) noexcept;
bool hasFlag(MediaBufferFlag flags, MediaBufferFlag flag) noexcept;

class MediaBuffer {
public:
    virtual ~MediaBuffer() = default;

    MediaBuffer(const MediaBuffer&) = delete;
    MediaBuffer& operator=(const MediaBuffer&) = delete;

    virtual MediaBufferType type() const noexcept = 0;
    virtual std::optional<std::uint64_t> payloadFootprintBytes() const noexcept;

    MediaStreamKind streamKind() const noexcept;
    MediaPayloadKind payloadKind() const noexcept;
    const MediaFormatDescriptor& formatDescriptor() const noexcept;
    const MediaTimeDescriptor& timeDescriptor() const noexcept;
    const MediaHardwareDescriptor& hardwareDescriptor() const noexcept;

    MediaTimeValue pts() const noexcept;
    MediaTimeValue dts() const noexcept;
    MediaDuration duration() const noexcept;

    MediaBufferFlag flags() const noexcept;
    bool isEof() const noexcept;
    bool isFlush() const noexcept;
    bool isKeyFrame() const noexcept;
    bool isHardwareBacked() const noexcept;

    const std::string& diagnosticName() const noexcept;

    void setStreamKind(MediaStreamKind streamKind) noexcept;
    void setPayloadKind(MediaPayloadKind payloadKind) noexcept;
    void setFormatDescriptor(MediaFormatDescriptor descriptor);
    void setTimeDescriptor(MediaTimeDescriptor descriptor) noexcept;
    void setHardwareDescriptor(MediaHardwareDescriptor descriptor);
    void setTimestamps(MediaTimeValue pts, MediaTimeValue dts, MediaDuration duration) noexcept;
    void setFlags(MediaBufferFlag flags) noexcept;
    void addFlags(MediaBufferFlag flags) noexcept;
    void setDiagnosticName(std::string name);

protected:
    MediaBuffer() = default;

private:
    MediaStreamKind m_streamKind = MediaStreamKind::Unknown;
    MediaPayloadKind m_payloadKind = MediaPayloadKind::Unknown;
    MediaFormatDescriptor m_format;
    MediaTimeDescriptor m_time;
    MediaHardwareDescriptor m_hardware;

    MediaTimeValue m_pts = invalidMediaTimeValue;
    MediaTimeValue m_dts = invalidMediaTimeValue;
    MediaDuration m_duration = 0;
    MediaBufferFlag m_flags = MediaBufferFlag::None;
    std::string m_diagnosticName;
};

} // namespace media::ffmpeg::graph
