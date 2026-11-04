#pragma once

namespace media::ffmpeg::graph {

// Internal protocol handoff roles map to the existing DAG output ports.
enum class MediaPreparedRtpOutputPort { Packet, Clock, Event };

constexpr const char* mediaPreparedRtpOutputPortName(MediaPreparedRtpOutputPort port) noexcept
{
    switch (port) {
    case MediaPreparedRtpOutputPort::Packet: return "packet";
    case MediaPreparedRtpOutputPort::Clock: return "clock";
    case MediaPreparedRtpOutputPort::Event: return "event";
    }
    return nullptr;
}

} // namespace media::ffmpeg::graph
