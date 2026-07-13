#include "internal/graph/protocol/mpegts/MediaTsPesProvenanceTimeline.h"

#include <algorithm>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaTsPesProvenanceTimeline::MediaTsPesProvenanceTimeline(
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes) noexcept
    : m_capacity(capacity)
    , m_maximumPositionRegressionBytes(maximumPositionRegressionBytes)
{
}

::media::Result<MediaTsPesProvenanceTimeline> MediaTsPesProvenanceTimeline::create(
    std::size_t packetStride,
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes)
{
    if (packetStride != 188) {
        return ::media::Result<MediaTsPesProvenanceTimeline>::failure(
            ::media::ErrorInfo::unsupported("PES provenance supports only 188-byte MPEG-TS packets"));
    }
    if (capacity == 0) {
        return ::media::Result<MediaTsPesProvenanceTimeline>::failure(
            ::media::ErrorInfo::invalidArgument("PES provenance capacity must be positive"));
    }
    return ::media::Result<MediaTsPesProvenanceTimeline>::success(
        MediaTsPesProvenanceTimeline(capacity, maximumPositionRegressionBytes));
}

::media::Status MediaTsPesProvenanceTimeline::trackPid(std::uint16_t pid)
{
    if (pid == 0 || pid >= 0x1FFF) return invalid("PES provenance PID is not an elementary PID");
    if (m_selectedPids) {
        return m_trackedPids.contains(pid)
            ? ::media::Status::success()
            : invalid("PES provenance rejects a new PID after selection");
    }
    m_trackedPids.insert(pid);
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::configureSelectedPids(
    std::span<const std::uint16_t> pids)
{
    if (m_selectedPids) return invalid("PES provenance selected PIDs are already configured");
    if (pids.empty()) return invalid("PES provenance selected PIDs must not be empty");
    std::unordered_set<std::uint16_t> selected;
    for (const auto pid : pids) {
        if (!m_trackedPids.contains(pid)) {
            return invalid("PES provenance selected PID was not tracked during probe");
        }
        if (!selected.insert(pid).second) {
            return invalid("PES provenance selected PIDs must be unique");
        }
    }
    m_selectedPids = std::move(selected);
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::validateObservedOffset(std::uint64_t byteOffset)
{
    if (!m_alignmentOrigin) m_alignmentOrigin = byteOffset;
    if (byteOffset < *m_alignmentOrigin ||
        (byteOffset - *m_alignmentOrigin) % 188 != 0) {
        return invalid("PES provenance packet offset is not aligned to the MPEG-TS stride");
    }
    return ::media::Status::success();
}

MediaTsPesProvenanceTimeline::Range* MediaTsPesProvenanceTimeline::openRange(
    std::uint16_t pid)
{
    const auto it = std::find_if(m_ranges.rbegin(), m_ranges.rend(), [pid](const Range& range) {
        return range.pid == pid && !range.endByteOffset;
    });
    return it == m_ranges.rend() ? nullptr : &*it;
}

::media::Status MediaTsPesProvenanceTimeline::appendRange(Range range)
{
    auto* previous = openRange(range.pid);
    if (previous) {
        if (range.startByteOffset < previous->startByteOffset) {
            return invalid("PES provenance ranges must increase by PID");
        }
        if (range.startByteOffset == previous->startByteOffset) {
            *previous = std::move(range);
            return ::media::Status::success();
        }
    }
    evictSafeRanges();
    if (m_ranges.size() == m_capacity) {
        return invalid("PES provenance capacity exhausted before safe eviction");
    }
    previous = openRange(range.pid);
    if (previous) previous->endByteOffset = range.startByteOffset;
    m_ranges.push_back(std::move(range));
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::invalidate(
    std::uint64_t byteOffset,
    std::uint16_t pid)
{
    auto* current = openRange(pid);
    if (current && current->validity == MediaTsPesProvenanceValidity::Invalid) {
        return ::media::Status::success();
    }
    return appendRange(Range{byteOffset, std::nullopt, pid, byteOffset, std::nullopt,
                             MediaTsPesProvenanceValidity::Invalid});
}

::media::Status MediaTsPesProvenanceTimeline::onPacketPrefix(
    const MediaTsPacketPrefixView& packet)
{
    auto aligned = validateObservedOffset(packet.byteOffset);
    if (!aligned) return aligned;
    if (m_lastPacketOffset && packet.byteOffset <= *m_lastPacketOffset) {
        return invalid("PES provenance packet offsets must increase strictly");
    }
    if (!m_trackedPids.contains(packet.pid)) {
        m_lastPacketOffset = packet.byteOffset;
        return ::media::Status::success();
    }

    auto* current = openRange(packet.pid);
    if (!packet.payloadUnitStart) {
        auto status = current ? ::media::Status::success()
                              : invalidate(packet.byteOffset, packet.pid);
        if (status) m_lastPacketOffset = packet.byteOffset;
        return status;
    }
    if (!packet.pesStart) {
        auto status = invalidate(packet.byteOffset, packet.pid);
        if (status) m_lastPacketOffset = packet.byteOffset;
        return status;
    }
    auto status = appendRange(Range{packet.byteOffset, std::nullopt, packet.pid,
                                    packet.byteOffset, packet.byteOffset,
                                    MediaTsPesProvenanceValidity::Valid});
    if (status) m_lastPacketOffset = packet.byteOffset;
    return status;
}

::media::Status MediaTsPesProvenanceTimeline::onContinuityEvent(
    const MediaTsContinuityEvent& event,
    bool beginsPayloadUnit)
{
    if (!m_trackedPids.contains(event.pid)) return ::media::Status::success();
    auto aligned = validateObservedOffset(event.byteOffset);
    if (!aligned) return aligned;
    if (m_lastPacketOffset && event.byteOffset <= *m_lastPacketOffset) {
        return invalid("PES provenance continuity event regresses packet position");
    }
    if (!beginsPayloadUnit) {
        auto* current = openRange(event.pid);
        if (!current) return invalidate(event.byteOffset, event.pid);
        current->stateEvidenceByteOffset = event.byteOffset;
        current->originByteOffset = std::nullopt;
        current->validity = MediaTsPesProvenanceValidity::Invalid;
        return ::media::Status::success();
    }
    return appendRange(Range{
        event.byteOffset, std::nullopt, event.pid, event.byteOffset, std::nullopt,
        MediaTsPesProvenanceValidity::Invalid});
}

::media::Status MediaTsPesProvenanceTimeline::onSourceClockBoundary(
    std::uint64_t byteOffset)
{
    MediaTsPesProvenanceTimeline candidate = *this;
    auto status = candidate.applySourceClockBoundary(byteOffset);
    if (!status) return status;
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::replaySourceClockBoundaries(
    std::span<const std::uint64_t> byteOffsets)
{
    if (!m_selectedPids) {
        return invalid("PES provenance boundary replay requires selected PIDs");
    }
    MediaTsPesProvenanceTimeline candidate = *this;
    std::optional<std::uint64_t> previous;
    for (const auto byteOffset : byteOffsets) {
        if (previous && byteOffset <= *previous) {
            return invalid("PES provenance replay boundaries must increase strictly");
        }
        auto aligned = candidate.validateObservedOffset(byteOffset);
        if (!aligned) return aligned;
        if (!candidate.m_lastPacketOffset || byteOffset > *candidate.m_lastPacketOffset) {
            return invalid("PES provenance replay boundary exceeds observed packets");
        }
        auto status = candidate.invalidateSelectedRangesAt(
            byteOffset, BoundaryApplication::Historical);
        if (!status) return status;
        candidate.m_lastSourceClockBoundaryOffset = byteOffset;
        previous = byteOffset;
    }
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::applySourceClockBoundary(
    std::uint64_t byteOffset)
{
    if (!m_selectedPids) {
        return invalid("PES provenance source boundary requires selected PIDs");
    }
    auto aligned = validateObservedOffset(byteOffset);
    if (!aligned) return aligned;
    if ((m_lastPacketOffset && byteOffset <= *m_lastPacketOffset) ||
        (m_lastSourceClockBoundaryOffset &&
         byteOffset <= *m_lastSourceClockBoundaryOffset)) {
        return invalid("PES provenance source boundary offsets must increase strictly");
    }

    auto status = invalidateSelectedRangesAt(byteOffset, BoundaryApplication::Live);
    if (!status) return status;
    m_lastSourceClockBoundaryOffset = byteOffset;
    return ::media::Status::success();
}

::media::Status MediaTsPesProvenanceTimeline::invalidateSelectedRangesAt(
    std::uint64_t byteOffset,
    BoundaryApplication application)
{
    std::vector<std::uint16_t> selected(m_selectedPids->begin(), m_selectedPids->end());
    std::sort(selected.begin(), selected.end());
    for (const auto pid : selected) {
        auto range = std::find_if(m_ranges.begin(), m_ranges.end(),
            [pid, byteOffset](const Range& item) {
                return item.pid == pid && item.startByteOffset <= byteOffset &&
                       (!item.endByteOffset || byteOffset < *item.endByteOffset);
            });
        if (range == m_ranges.end()) {
            if (application == BoundaryApplication::Historical) continue;
            auto appended = appendRange(Range{
                byteOffset, std::nullopt, pid, byteOffset, std::nullopt,
                MediaTsPesProvenanceValidity::Invalid});
            if (!appended) return appended;
            continue;
        }
        if (application == BoundaryApplication::Historical &&
            range->startByteOffset == byteOffset) {
            continue;
        }
        range->stateEvidenceByteOffset = byteOffset;
        range->originByteOffset = std::nullopt;
        range->validity = MediaTsPesProvenanceValidity::Invalid;
    }
    return ::media::Status::success();
}

::media::Result<MediaTsPesProvenanceAnchor> MediaTsPesProvenanceTimeline::resolveAnchor(
    std::uint64_t packetPosition,
    std::uint16_t pid) const
{
    if (!m_selectedPids || !m_selectedPids->contains(pid)) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::invalidArgument("PES provenance PID is not selected"));
    }
    if (!m_alignmentOrigin || packetPosition < *m_alignmentOrigin ||
        (packetPosition - *m_alignmentOrigin) % 188 != 0) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::invalidArgument("PES provenance query is not packet aligned"));
    }
    if (!m_lastPacketOffset || packetPosition > *m_lastPacketOffset) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::notInitialized("PES provenance query exceeds observed input"));
    }
    if (m_queryHighWatermark && *m_queryHighWatermark > packetPosition &&
        *m_queryHighWatermark - packetPosition > m_maximumPositionRegressionBytes) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::invalidArgument("PES provenance query exceeds planned regression"));
    }
    const auto it = std::find_if(m_ranges.rbegin(), m_ranges.rend(),
        [packetPosition, pid](const Range& range) {
            return range.pid == pid && range.startByteOffset <= packetPosition &&
                   (!range.endByteOffset || packetPosition < *range.endByteOffset);
        });
    if (it == m_ranges.rend()) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::notInitialized("PES provenance range is unavailable"));
    }
    const MediaTsPesProvenanceAnchor anchor{
        pid, it->startByteOffset, it->stateEvidenceByteOffset,
        it->originByteOffset, it->validity};
    if (!m_queryHighWatermark || packetPosition > *m_queryHighWatermark) {
        m_queryHighWatermark = packetPosition;
        evictSafeRanges();
    }
    return ::media::Result<MediaTsPesProvenanceAnchor>::success(
        anchor);
}

::media::Result<MediaTsPesProvenanceAnchor>
MediaTsPesProvenanceTimeline::stateForAnchor(
    const MediaTsPesProvenanceAnchor& anchor) const
{
    if (!m_selectedPids || !m_selectedPids->contains(anchor.pid)) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::invalidArgument(
                "PES provenance anchor PID is not selected"));
    }
    const auto range = std::find_if(
        m_ranges.rbegin(), m_ranges.rend(), [&anchor](const Range& item) {
            return item.pid == anchor.pid &&
                   item.startByteOffset == anchor.rangeByteOffset;
        });
    if (range == m_ranges.rend()) {
        return ::media::Result<MediaTsPesProvenanceAnchor>::failure(
            ::media::ErrorInfo::notInitialized(
                "PES provenance anchor record is unavailable"));
    }
    return ::media::Result<MediaTsPesProvenanceAnchor>::success(
        MediaTsPesProvenanceAnchor{
            range->pid, range->startByteOffset, range->stateEvidenceByteOffset,
            range->originByteOffset, range->validity});
}

void MediaTsPesProvenanceTimeline::evictSafeRanges() const
{
    if (!m_queryHighWatermark ||
        *m_queryHighWatermark < m_maximumPositionRegressionBytes) return;
    const auto earliestLegalPosition =
        *m_queryHighWatermark - m_maximumPositionRegressionBytes;
    std::erase_if(m_ranges, [earliestLegalPosition](const Range& range) {
        return range.endByteOffset && *range.endByteOffset <= earliestLegalPosition;
    });
}

} // namespace media::ffmpeg::graph
