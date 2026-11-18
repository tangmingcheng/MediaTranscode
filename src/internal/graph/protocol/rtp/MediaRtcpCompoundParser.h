#pragma once
#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "media_transcode/Result.h"
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>

namespace media::ffmpeg::graph {

enum class MediaRtcpPacketKind { SenderReport, ReceiverReport, SourceDescription, Bye, Unknown };
struct MediaRtcpSenderReport final {
    uint32_t ssrc;
    MediaRtcpNtpTimestamp ntp;
    uint32_t rtpTimestamp;
    uint32_t senderPacketCount;
    uint32_t senderOctetCount;
};

// Views are constructed only over a completely validated compound datagram.
// Iteration never allocates and never owns the datagram. A view cannot outlive
// its caller's synchronous RTCP observation.
template<class View>
class MediaRtcpWireRange final {
public:
    class Iterator final {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = View;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = View;
        explicit Iterator(std::span<const uint8_t> bytes) noexcept : m_bytes(bytes) {}
        View operator*() const noexcept { return View::read(m_bytes); }
        Iterator& operator++() noexcept { m_bytes = m_bytes.subspan(View::read(m_bytes).wireBytes); return *this; }
        Iterator operator++(int) noexcept { auto previous = *this; ++*this; return previous; }
        friend bool operator==(const Iterator& left, const Iterator& right) noexcept
        { return (left.m_bytes.empty() && right.m_bytes.empty()) ||
            (left.m_bytes.data() == right.m_bytes.data() && left.m_bytes.size() == right.m_bytes.size()); }
    private:
        std::span<const uint8_t> m_bytes;
    };
    explicit MediaRtcpWireRange(std::span<const uint8_t> bytes = {}) noexcept : m_bytes(bytes) {}
    Iterator begin() const noexcept { return Iterator(m_bytes); }
    Iterator end() const noexcept { return Iterator({}); }
    View front() const noexcept { return View::read(m_bytes); }
private:
    std::span<const uint8_t> m_bytes;
};
struct MediaRtcpSdesItem final {
    uint8_t type;
    std::span<const uint8_t> value;
    std::size_t wireBytes;
    static MediaRtcpSdesItem read(std::span<const uint8_t> bytes) noexcept;
};
struct MediaRtcpSdesChunk final {
    uint32_t ssrc;
    MediaRtcpWireRange<MediaRtcpSdesItem> items;
    std::size_t wireBytes;
    static MediaRtcpSdesChunk read(std::span<const uint8_t> bytes) noexcept;
};
struct MediaRtcpByeSource final {
    uint32_t ssrc;
    std::size_t wireBytes;
    operator uint32_t() const noexcept { return ssrc; }
    static MediaRtcpByeSource read(std::span<const uint8_t> bytes) noexcept;
};
struct MediaRtcpPacket final {
    MediaRtcpPacketKind kind;
    uint8_t packetType;
    uint8_t count;
    uint8_t paddingSize;
    std::optional<MediaRtcpSenderReport> senderReport;
    std::optional<uint32_t> receiverReportSsrc;
    MediaRtcpWireRange<MediaRtcpSdesChunk> sdesChunks;
    MediaRtcpWireRange<MediaRtcpByeSource> byeSources;
    std::span<const uint8_t> byeReason;
    std::size_t wireBytes;
    static MediaRtcpPacket read(std::span<const uint8_t> bytes) noexcept;
};
using MediaRtcpPackets = MediaRtcpWireRange<MediaRtcpPacket>;

class MediaRtcpCompoundParser final {
public:
    static ::media::Result<MediaRtcpPackets> parse(
        std::span<const uint8_t> datagram, const MediaRtcpCompoundPolicy& policy);
private:
    MediaRtcpCompoundParser() = delete;
};

} // namespace media::ffmpeg::graph
