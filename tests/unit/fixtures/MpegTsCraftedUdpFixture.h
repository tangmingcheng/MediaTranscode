#pragma once

#include "internal/graph/protocol/mpegts/MediaTsEvidenceTimeline.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph::test_fixture {

struct CraftedTsProgramIdentity final {
    std::uint16_t programNumber = 1;
    std::uint16_t pmtPid = 0x1000;
    std::uint16_t pcrPid = 0x0100;
    std::uint16_t videoPid = 0x0100;
    std::uint16_t audioPid = 0x0101;
};

class CraftedTsBytes final {
public:
    static ::media::Result<CraftedTsBytes> generate();

    const CraftedTsProgramIdentity& identity() const noexcept { return m_identity; }
    std::size_t pcrCount() const noexcept;
    std::uint64_t pcrModulus() const noexcept;
    const std::vector<std::uint8_t>& bytes() const noexcept { return m_bytes; }

    ::media::Status rewritePcrSequence(std::uint64_t first,
                                       std::int64_t interval27Mhz);
    ::media::Status rewritePcr(std::size_t index, std::uint64_t pcr27Mhz);
    ::media::Status markPcrDiscontinuity(std::size_t index);
    ::media::Status changePatAndPmtVersionAfterFirst(std::uint8_t version);
    ::media::Status changePcrPidAfterFirstPmt(std::uint8_t version,
                                              std::uint16_t pcrPid);

private:
    CraftedTsBytes(std::vector<std::uint8_t> bytes,
                   std::vector<std::size_t> pcrPacketOffsets) noexcept;
    ::media::Status rewritePsiVersions(std::uint8_t version,
                                       std::optional<std::uint16_t> pcrPid);

    CraftedTsProgramIdentity m_identity;
    std::vector<std::uint8_t> m_bytes;
    std::vector<std::size_t> m_pcrPacketOffsets;
};

struct CraftedUdpObservation final {
    std::vector<MediaTsEvidenceCheckpoint> evidence;
};

::media::Result<CraftedUdpObservation> observeCraftedBytesOverProductionUdp(
    const CraftedTsBytes& stream,
    std::uint16_t port);

} // namespace media::ffmpeg::graph::test_fixture
