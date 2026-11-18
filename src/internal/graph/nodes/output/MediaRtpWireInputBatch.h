#pragma once
#include "internal/graph/nodes/output/MediaMpegTsDatagramView.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include <functional>
#include <variant>

namespace media::ffmpeg::graph {
struct MediaPacketizedRtpDatagramView final {
    std::span<const std::uint8_t> bytes;
    std::size_t payloadOctets;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime canonicalRelease;
    MediaWireMediaBoundary mediaBoundary;
};

struct MediaRtpWireInputTiming final {
    MediaRunningTime presentationOnMaster;
    MediaRunningTime canonicalRelease;
    MediaWireMediaBoundary mediaBoundary;
};

struct MediaRtpWireInputDatagram final {
    std::variant<MediaPacketizedRtpDatagramView, MediaMpegTsRtpPacketPlan> payload;
    MediaRtpWireInputTiming timing;
    std::size_t payloadOctets() const noexcept;
};

// Synchronous immutable borrow. Neither indexed access nor preparation advances
// protocol state; the same original spans are used throughout both sizing passes.
class MediaRtpWireInputBatch final {
public:
    explicit MediaRtpWireInputBatch(std::span<const MediaPacketizedRtpDatagramView> datagrams) noexcept;
    MediaRtpWireInputBatch(std::span<const MediaMpegTsDatagramView> datagrams,
        std::reference_wrapper<const MediaMpegTsRtpPacketizer> packetizer) noexcept;
    MediaRtpWireInputBatch(std::span<const MediaMpegTsProtocolDatagram> datagrams,
        std::reference_wrapper<const MediaMpegTsRtpPacketizer> packetizer) noexcept;
    std::size_t size() const noexcept;
    MediaRtpWireInputTiming timing(std::size_t index) const noexcept;
    ::media::Result<MediaRtpWireInputDatagram> at(std::size_t index) const;
private:
    template<class Datagram> struct TsBatch {
        std::span<const Datagram> datagrams;
        std::reference_wrapper<const MediaMpegTsRtpPacketizer> packetizer;
    };
    std::variant<std::span<const MediaPacketizedRtpDatagramView>,
        TsBatch<MediaMpegTsDatagramView>, TsBatch<MediaMpegTsProtocolDatagram>> m_input;
};
} // namespace media::ffmpeg::graph
