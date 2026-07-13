#include "internal/graph/protocol/mpegts/MediaTsClockProjection.h"

#include <algorithm>

namespace media::ffmpeg::graph {

MediaTsClockProjection::MediaTsClockProjection(
    MediaTsProgramClockPolicy policy,
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes,
    MediaTsProgramClockTracker tracker) noexcept
    : m_policy(policy)
    , m_capacity(capacity)
    , m_maximumPositionRegressionBytes(maximumPositionRegressionBytes)
    , m_tracker(std::move(tracker))
{
}

::media::Result<MediaTsClockProjection> MediaTsClockProjection::create(
    MediaTsProgramClockPolicy policy,
    std::size_t capacity,
    std::uint64_t maximumPositionRegressionBytes)
{
    if (capacity == 0) {
        return ::media::Result<MediaTsClockProjection>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS clock projection capacity must be positive"));
    }
    auto tracker = MediaTsProgramClockTracker::create(policy, 0);
    if (!tracker) {
        return ::media::Result<MediaTsClockProjection>::failure(tracker.error());
    }
    return ::media::Result<MediaTsClockProjection>::success(
        MediaTsClockProjection(policy, capacity, maximumPositionRegressionBytes,
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
    if (m_checkpoints.empty() && evidence.generation != m_tracker.generation()) {
        auto seeded = MediaTsProgramClockTracker::create(m_policy, evidence.generation);
        if (!seeded) return ::media::Status::failure(seeded.error());
        m_tracker = std::move(seeded).value();
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
            .discontinuity = raw.discontinuity});
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
        .generation = m_tracker.generation()});
    m_lastReplayedOffset = evidence.byteOffset;
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

} // namespace media::ffmpeg::graph
