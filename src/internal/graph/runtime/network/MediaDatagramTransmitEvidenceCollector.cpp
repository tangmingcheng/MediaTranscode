#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include <algorithm>
#include <limits>
#include <new>

namespace media::ffmpeg::graph {

MediaDatagramTransmitEvidenceCollector::MediaDatagramTransmitEvidenceCollector(
    std::uint64_t generation,
    std::optional<MediaDatagramTransmitEvidencePlan> plan,
    std::vector<MediaDatagramTransmitPort*> ports) noexcept
    : m_generation(generation),
      m_plan(std::move(plan)),
      m_ports(std::move(ports))
{
}

::media::Result<MediaDatagramTransmitEvidenceCollector>
MediaDatagramTransmitEvidenceCollector::create(
    std::uint64_t generation,
    std::optional<MediaDatagramTransmitEvidencePlan> plan,
    std::vector<MediaDatagramTransmitPort*> ports)
{
    if (generation == 0) {
        return ::media::Result<MediaDatagramTransmitEvidenceCollector>::failure(
            ::media::ErrorInfo::invalidArgument(
                "transmit evidence collector requires a generation"));
    }
    for (auto* port : ports) {
        if (!port) {
            return ::media::Result<MediaDatagramTransmitEvidenceCollector>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "transmit evidence collector requires valid ports"));
        }
    }
    try {
        MediaDatagramTransmitEvidenceCollector collector(
            generation, std::move(plan), std::move(ports));
        if (collector.m_plan) {
            collector.m_pending.reserve(static_cast<std::size_t>(
                collector.m_plan->maximumCorrelationEntries));
            collector.m_recentObserved.reserve(static_cast<std::size_t>(
                collector.m_plan->maximumCorrelationEntries));
        }
        return ::media::Result<MediaDatagramTransmitEvidenceCollector>::success(
            std::move(collector));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaDatagramTransmitEvidenceCollector>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaDatagramTransmitEvidenceCollector"));
    }
}

::media::Status MediaDatagramTransmitEvidenceCollector::recordSubmitted(
    std::uint64_t endpointId,
    std::uint64_t evidenceId,
    MediaRunningTime submittedAt) noexcept
{
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_telemetry.submitted ==
        (std::numeric_limits<std::uint64_t>::max)()) {
        m_terminalFailure = ::media::ErrorInfo::internalError(
            "transmit evidence submitted counter overflowed");
        return ::media::Status::failure(*m_terminalFailure);
    }
    ++m_telemetry.submitted;
    m_telemetry.deliveryEvidenceProven = false;
    if (!m_plan) return ::media::Status::success();
    if (evidenceId < m_plan->firstEvidenceId ||
        evidenceId > m_plan->lastEvidenceId ||
        (m_lastSubmittedEvidenceId &&
         evidenceId <= *m_lastSubmittedEvidenceId) ||
        m_pending.size() >= m_plan->maximumCorrelationEntries ||
        m_pending.contains(evidenceId) ||
        std::find(m_recentObserved.begin(), m_recentObserved.end(),
                  evidenceId) != m_recentObserved.end()) {
        return coverageFailure("invalid or duplicate transmit evidence id");
    }
    try {
        m_pending.emplace(evidenceId, Pending{endpointId, submittedAt});
        m_lastSubmittedEvidenceId = evidenceId;
        return ::media::Status::success();
    } catch (const std::bad_alloc&) {
        m_terminalFailure = ::media::ErrorInfo::allocationFailed(
            "transmit evidence correlation");
        return ::media::Status::failure(*m_terminalFailure);
    }
}

::media::Status MediaDatagramTransmitEvidenceCollector::drainAvailable(
    MediaRunningTime now) noexcept
{
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    if (!m_plan) return ::media::Status::success();
    for (auto* port : m_ports) {
        auto drained = port->drainAvailableEvidence();
        if (!drained) {
            m_terminalFailure = drained.error();
            return ::media::Status::failure(*m_terminalFailure);
        }
        for (const auto& evidence : drained.value()) {
            if (evidence.generation != m_generation) {
                ++m_telemetry.crossGeneration;
                auto status = coverageFailure(
                    "cross-generation transmit evidence");
                if (!status) return status;
                continue;
            }
            if (std::find(m_recentObserved.begin(), m_recentObserved.end(),
                          evidence.evidenceId) != m_recentObserved.end()) {
                ++m_telemetry.duplicate;
                auto status = coverageFailure("duplicate transmit evidence");
                if (!status) return status;
                continue;
            }
            const auto pending = m_pending.find(evidence.evidenceId);
            if (pending == m_pending.end() ||
                pending->second.endpointId != evidence.endpointId) {
                ++m_telemetry.unmatched;
                auto status = coverageFailure(
                    "unmatched transmit evidence");
                if (!status) return status;
                continue;
            }
            auto expires = pending->second.submittedAt.checkedAdd(
                m_plan->maximumDrainResidence);
            if (!expires || now > expires.value()) ++m_telemetry.late;
            if (m_recentObserved.size() == m_recentObserved.capacity()) {
                m_recentObserved.erase(m_recentObserved.begin());
            }
            m_recentObserved.push_back(evidence.evidenceId);
            m_pending.erase(pending);
            ++m_telemetry.observed;
        }
    }
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        auto expires = it->second.submittedAt.checkedAdd(
            m_plan->maximumDrainResidence);
        if (!expires || now > expires.value()) {
            ++m_telemetry.lost;
            it = m_pending.erase(it);
            auto status = coverageFailure("transmit evidence expired");
            if (!status) return status;
        } else {
            ++it;
        }
    }
    m_telemetry.transmitTimestampCoverageComplete =
        m_telemetry.submitted != 0 &&
        m_telemetry.observed == m_telemetry.submitted &&
        m_telemetry.late == 0 && m_telemetry.lost == 0 &&
        m_telemetry.duplicate == 0 && m_telemetry.crossGeneration == 0 &&
        m_telemetry.unmatched == 0;
    m_telemetry.deliveryEvidenceProven = false;
    return ::media::Status::success();
}

::media::Status MediaDatagramTransmitEvidenceCollector::coverageFailure(
    const char* message) noexcept
{
    if (!m_plan ||
        m_plan->coverageGapPolicy ==
            MediaDatagramEvidenceCoverageGapPolicy::Report) {
        return ::media::Status::success();
    }
    m_terminalFailure = ::media::ErrorInfo::ioFailure(message);
    return ::media::Status::failure(*m_terminalFailure);
}

} // namespace media::ffmpeg::graph
