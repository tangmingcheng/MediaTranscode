#pragma once

#include "media_transcode/Result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>

namespace media::ffmpeg::graph {

struct MediaTsPacketView final {
    uint64_t byteOffset = 0;
    uint16_t pid = 0;
    bool payloadUnitStart = false;
    uint8_t continuityCounter = 0;
    bool discontinuity = false;
    std::optional<uint64_t> pcr27Mhz;
    std::span<const uint8_t> payloadSpan;
};

struct MediaTsPacketEvidenceView final {
    uint64_t byteOffset = 0;
    uint16_t pid = 0;
    bool payloadUnitStart = false;
    uint8_t continuityCounter = 0;
    bool discontinuity = false;
    std::optional<uint64_t> pcr27Mhz;
};

struct MediaTsPacketPrefixView final {
    uint64_t byteOffset = 0;
    uint16_t pid = 0;
    bool payloadUnitStart = false;
    bool pesStart = false;
};

enum class MediaTsContinuityEventReason {
    CounterLoss,
    DiscontinuityIndicator
};

struct MediaTsContinuityEvent final {
    std::uint64_t byteOffset = 0;
    std::uint16_t pid = 0;
    MediaTsContinuityEventReason reason = MediaTsContinuityEventReason::CounterLoss;

    bool operator==(const MediaTsContinuityEvent&) const = default;
};

class MediaTsPacketSink {
public:
    virtual ~MediaTsPacketSink() = default;

    // packet.payloadSpan is valid only for the duration of this synchronous call.
    virtual ::media::Status onPacket(const MediaTsPacketView& packet) = 0;
    virtual ::media::Status onContinuityEvent(const MediaTsContinuityEvent&) {
        return ::media::Status::success();
    }
};

class MediaTsIncrementalPacketSink {
public:
    virtual ~MediaTsIncrementalPacketSink() = default;
    virtual ::media::Status onPacketEvidence(const MediaTsPacketEvidenceView& packet) = 0;
    virtual ::media::Status onPacketPrefix(const MediaTsPacketPrefixView& packet) = 0;
};

class MediaTsPacketParser final {
public:
    static ::media::Result<std::unique_ptr<MediaTsPacketParser>> create(
        std::size_t packetStride,
        MediaTsPacketSink& sink,
        MediaTsIncrementalPacketSink* incrementalSink);

    ::media::Status push(std::span<const uint8_t> bytes);
    ::media::Status finish();
    std::size_t retainedByteCount() const noexcept { return m_carrySize; }
    uint64_t copiedPacketByteCount() const noexcept { return m_copiedPacketBytes; }

private:
    struct ContinuityState final {
        uint8_t counter = 0;
    };

    struct ParsedPacketEvidence final {
        MediaTsPacketEvidenceView view;
        bool hasPayload = false;
        std::size_t payloadOffset = 0;
        bool prefixPublished = false;
    };

    MediaTsPacketParser(MediaTsPacketSink& sink,
                        MediaTsIncrementalPacketSink* incrementalSink);
    ::media::Status publishIncrementalEvidence(std::span<const uint8_t> packet,
                                               uint64_t byteOffset);
    ::media::Status parsePacket(std::span<const uint8_t> packet, uint64_t byteOffset);

    MediaTsPacketSink& m_sink;
    MediaTsIncrementalPacketSink* m_incrementalSink = nullptr;
    std::array<uint8_t, 188> m_carry{};
    std::array<uint8_t, 188> m_packetScratch{};
    std::size_t m_carrySize = 0;
    uint64_t m_carryOffset = 0;
    uint64_t m_nextInputOffset = 0;
    uint64_t m_copiedPacketBytes = 0;
    bool m_strideLocked = false;
    bool m_finished = false;
    std::optional<::media::ErrorInfo> m_finishFailure;
    std::optional<ParsedPacketEvidence> m_publishedEvidence;
    std::unordered_map<uint16_t, ContinuityState> m_continuity;
};

} // namespace media::ffmpeg::graph
