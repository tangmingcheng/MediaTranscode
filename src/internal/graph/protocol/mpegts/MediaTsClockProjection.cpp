#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {

MediaTsClockProjection::MediaTsClockProjection(
    MediaTsProgramClockPolicy policy,
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes,
    std::uint64_t initialSourceGeneration,
    std::uint64_t expectedInitialRawTransportGeneration,
    MediaTsProgramClockTracker tracker) noexcept
    : m_policy(policy)
    , m_capacity(capacity)
    , m_maximumPositionRegressionBytes(maximumPositionRegressionBytes)
    , m_initialSourceGeneration(initialSourceGeneration)
    , m_expectedInitialRawTransportGeneration(expectedInitialRawTransportGeneration)
    , m_tracker(std::move(tracker))
{
}

::media::Result<MediaTsClockProjection> MediaTsClockProjection::create(
    MediaTsProgramClockPolicy policy,
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes,
    std::uint64_t initialSourceGeneration,
    std::uint64_t expectedInitialRawTransportGeneration)
{
    if (capacity == 0) {
        return ::media::Result<MediaTsClockProjection>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS clock projection capacity must be positive"));
    }
    auto tracker = MediaTsProgramClockTracker::create(policy, initialSourceGeneration);
    if (!tracker) {
        return ::media::Result<MediaTsClockProjection>::failure(tracker.error());
    }
    return ::media::Result<MediaTsClockProjection>::success(
        MediaTsClockProjection(policy, capacity, maximumPositionRegressionBytes,
                               initialSourceGeneration,
                               expectedInitialRawTransportGeneration,
                               std::move(tracker).value()));
}

::media::Status MediaTsClockProjection::replay(
    const std::vector<MediaTsEvidenceCheckpoint>& evidence)
{
    for (std::size_t index = 1; index < evidence.size(); ++index) {
        if (evidence[index].byteOffset <= evidence[index - 1].byteOffset) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS projection evidence must be strictly ordered"));
        }
    }
    auto candidate = *this;
    for (const auto& item : evidence) {
        if (candidate.m_lastReplayedOffset &&
            item.byteOffset <= *candidate.m_lastReplayedOffset) {
            continue;
        }
        if (auto status = candidate.replayOne(item); !status) return status;
    }
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsClockProjection::replayOne(
    const MediaTsEvidenceCheckpoint& evidence)
{
    if (evidence.pcrObservation &&
        evidence.pcrObservation->byteOffset != evidence.byteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS projection PCR offset mismatch"));
    }
    if (evidence.continuityEvent &&
        evidence.continuityEvent->byteOffset != evidence.byteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS projection continuity event offset mismatch"));
    }
    if (evidence.pcrObservation) {
        const bool matchingDiscontinuityEvent = evidence.continuityEvent &&
            evidence.continuityEvent->pid == evidence.pcrObservation->pid &&
            evidence.continuityEvent->reason ==
                MediaTsContinuityEventReason::DiscontinuityIndicator;
        if (evidence.pcrObservation->discontinuity != matchingDiscontinuityEvent) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS PCR discontinuity must match its raw continuity event"));
        }
    }
    if (!m_lastRawTransportGeneration) {
        const bool hasEvent = evidence.continuityEvent.has_value();
        const bool exactOrigin = !hasEvent &&
            evidence.generation == m_expectedInitialRawTransportGeneration;
        const bool firstTransition = hasEvent &&
            m_expectedInitialRawTransportGeneration !=
                std::numeric_limits<std::uint64_t>::max() &&
            evidence.generation == m_expectedInitialRawTransportGeneration + 1;
        if (!exactOrigin && !firstTransition) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "MPEG-TS clock projection missing bootstrap/history"));
        }
    } else {
        if (evidence.generation < *m_lastRawTransportGeneration ||
            evidence.generation - *m_lastRawTransportGeneration > 1) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS raw transport generation is non-contiguous"));
        }
        const bool advanced = evidence.generation != *m_lastRawTransportGeneration;
        if (advanced != evidence.continuityEvent.has_value()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS raw transport generation/event mismatch"));
        }
    }
    if (m_packetPositionHighWatermark &&
        *m_packetPositionHighWatermark > m_maximumPositionRegressionBytes) {
        const auto earliest = *m_packetPositionHighWatermark - m_maximumPositionRegressionBytes;
        while (m_checkpoints.size() > 1 && m_checkpoints[1].byteOffset <= earliest) {
            m_checkpoints.erase(m_checkpoints.begin());
        }
    }
    if (m_checkpoints.size() == m_capacity) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS clock projection capacity exhausted"));
    }
    if (auto status = validateInventory(evidence.inventory); !status) return status;
    if (evidence.continuityEvent) {
        const auto pid = evidence.continuityEvent->pid;
        ::media::Status continuity = pid == m_policy.pcrPid
            ? m_tracker.observePcrContinuityLoss(pid)
            : m_tracker.observeElementaryContinuityLoss(pid);
        if (!continuity) return continuity;
    }
    if (evidence.pcrObservation && evidence.pcrObservation->pid == m_policy.pcrPid) {
        const auto& raw = *evidence.pcrObservation;
        auto status = m_tracker.observe(MediaTsPcrObservation{
            .byteOffset = raw.byteOffset,
            .programNumber = m_policy.programNumber,
            .pmtPid = m_policy.pmtPid,
            .pcrPid = raw.pid,
            .videoPid = m_policy.videoPid,
            .audioPid = m_policy.audioPid,
            .pcr27Mhz = raw.pcr27Mhz,
            .discontinuity = false});
        if (!status) return status;
    }
    std::optional<MediaTsPcrCalibration> calibration;
    if (m_tracker.ready()) {
        auto current = m_tracker.calibration();
        if (!current) return ::media::Status::failure(current.error());
        calibration = current.value();
    }
    m_checkpoints.push_back(MediaTsClockProjectionCheckpoint{
        .byteOffset = evidence.byteOffset,
        .calibration = calibration,
        .readiness = calibration
            ? MediaSourceClockReadiness::Locked
            : (m_tracker.generation() == m_initialSourceGeneration
                ? MediaSourceClockReadiness::Acquiring
                : MediaSourceClockReadiness::ReacquireRequired),
        .generation = m_tracker.generation()});
    m_lastReplayedOffset = evidence.byteOffset;
    m_lastRawTransportGeneration = evidence.generation;
    return ::media::Status::success();
}

::media::Status MediaTsClockProjection::observePacketPosition(std::uint64_t packetPosition)
{
    if (m_packetPositionHighWatermark && *m_packetPositionHighWatermark > packetPosition &&
        *m_packetPositionHighWatermark - packetPosition > m_maximumPositionRegressionBytes) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS projection packet position exceeds planned regression"));
    }
    if (!m_packetPositionHighWatermark || packetPosition > *m_packetPositionHighWatermark) {
        m_packetPositionHighWatermark = packetPosition;
    }
    return ::media::Status::success();
}

::media::Status MediaTsClockProjection::validateInventory(
    const MediaTsProgramInventorySnapshot& inventory) const
{
    const auto program = std::find_if(
        inventory.programs.begin(), inventory.programs.end(),
        [this](const MediaTsProgramInfo& item) {
            return item.programNumber == m_policy.programNumber;
        });
    if (program == inventory.programs.end() || program->pmtPid != m_policy.pmtPid ||
        program->pcrPid != m_policy.pcrPid) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS projection selected program identity mismatch"));
    }
    const auto hasPid = [program](std::uint16_t pid) {
        return std::any_of(
            program->elementaryStreams.begin(), program->elementaryStreams.end(),
            [pid](const MediaTsElementaryStreamInfo& stream) { return stream.pid == pid; });
    };
    if (!hasPid(m_policy.videoPid) || !hasPid(m_policy.audioPid)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS projection selected stream PID mismatch"));
    }
    return ::media::Status::success();
}

::media::Result<MediaTsClockProjectionCheckpoint> MediaTsClockProjection::atOrBefore(
    std::uint64_t packetPosition) const
{
    const auto upper = std::upper_bound(
        m_checkpoints.begin(), m_checkpoints.end(), packetPosition,
        [](std::uint64_t offset, const MediaTsClockProjectionCheckpoint& item) {
            return offset < item.byteOffset;
        });
    if (upper == m_checkpoints.begin()) {
        return ::media::Result<MediaTsClockProjectionCheckpoint>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS clock projection is unavailable for packet position"));
    }
    return ::media::Result<MediaTsClockProjectionCheckpoint>::success(*std::prev(upper));
}

std::optional<std::uint64_t> MediaTsClockProjection::lastReplayedOffset() const noexcept
{
    return m_lastReplayedOffset;
}

std::uint64_t MediaTsClockProjection::oldestRetainedGeneration() const noexcept
{
    return m_checkpoints.empty() ? m_initialSourceGeneration
                                 : m_checkpoints.front().generation;
}

} // namespace media::ffmpeg::graph
