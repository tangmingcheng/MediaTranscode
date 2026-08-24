#include "internal/graph/runtime/network/MediaDatagramTransmitEvidenceCollector.h"

#include <algorithm>
#include <limits>
#include <new>
#include <unordered_set>

namespace media::ffmpeg::graph {

MediaDatagramTransmitEvidenceCollector::MediaDatagramTransmitEvidenceCollector(
    std::uint64_t generation,
    std::uint64_t maximumTrackedDatagrams,
    std::optional<MediaDatagramTransmitEvidencePlan> plan,
    std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule) noexcept
    : m_generation(generation),
      m_maximumTrackedDatagrams(maximumTrackedDatagrams),
      m_plan(std::move(plan)),
      m_kernelSchedule(std::move(kernelSchedule))
{
}

::media::Result<MediaDatagramTransmitEvidenceCollector>
MediaDatagramTransmitEvidenceCollector::create(
    std::uint64_t generation,
    std::uint64_t maximumTrackedDatagrams,
    std::optional<MediaDatagramTransmitEvidencePlan> plan,
    std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule,
    std::vector<MediaDatagramTransmitEvidenceEndpoint> endpoints)
{
    using ResultType =
        ::media::Result<MediaDatagramTransmitEvidenceCollector>;
    if (generation == 0 || maximumTrackedDatagrams == 0 ||
        maximumTrackedDatagrams >
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()) ||
        endpoints.empty() ||
        (kernelSchedule &&
         (kernelSchedule->authority.empty() ||
          kernelSchedule->maximumCorrelationEntries == 0 ||
          kernelSchedule->maximumCorrelationEntries < maximumTrackedDatagrams ||
          kernelSchedule->maximumRunDatagrams == 0 ||
          kernelSchedule->maximumRunDatagrams >
              static_cast<std::uint64_t>(
                  (std::numeric_limits<std::uint32_t>::max)()) + 1ULL ||
          kernelSchedule->maximumErrorQueueResidence <=
              MediaRunningTime::fromNanoseconds(0) ||
          kernelSchedule->maximumScheduleAheadNanoseconds == 0 ||
          kernelSchedule->maximumScheduleAheadNanoseconds >
              static_cast<std::uint64_t>(
                  (std::numeric_limits<std::int64_t>::max)())))) {
        return ResultType::failure(::media::ErrorInfo::invalidArgument(
            "invalid transmit evidence collector hard bounds"));
    }
    try {
        MediaDatagramTransmitEvidenceCollector collector(
            generation, maximumTrackedDatagrams, std::move(plan),
            std::move(kernelSchedule));
        if (collector.m_kernelSchedule) {
            auto residence = MediaRunningTime::fromNanoseconds(
                static_cast<std::int64_t>(collector.m_kernelSchedule->
                    maximumScheduleAheadNanoseconds)).checkedAdd(
                        collector.m_kernelSchedule->maximumErrorQueueResidence);
            if (!residence) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "SO_TXTIME correlation horizon is not representable"));
            }
            collector.m_launchCorrelationResidence = residence.value();
        }
        collector.m_entries.reserve(
            static_cast<std::size_t>(maximumTrackedDatagrams));
        collector.m_endpoints.reserve(endpoints.size());
        for (auto& endpoint : endpoints) {
            const auto timestampAvailability =
                endpoint.capabilities.timestampAvailability;
            const auto correlationMode =
                endpoint.capabilities.correlationMode;
            if (endpoint.endpointId == 0 || !endpoint.port ||
                endpoint.capabilities.zeroCopyEnabled ||
                (timestampAvailability !=
                     MediaDatagramTransmitTimestampAvailability::NotRequested &&
                 timestampAvailability !=
                     MediaDatagramTransmitTimestampAvailability::Available &&
                 timestampAvailability !=
                     MediaDatagramTransmitTimestampAvailability::Unavailable) ||
                (timestampAvailability ==
                     MediaDatagramTransmitTimestampAvailability::Available &&
                 (endpoint.capabilities.timestampSource ==
                     MediaDatagramTransmitTimestampSource::Unknown ||
                 endpoint.capabilities.timestampCounterFrequency == 0 ||
                 (correlationMode !=
                      MediaDatagramTransmitCorrelationMode::CallerSelectedUint32 &&
                  correlationMode !=
                      MediaDatagramTransmitCorrelationMode::KernelSequentialUint32))) ||
                (timestampAvailability !=
                     MediaDatagramTransmitTimestampAvailability::Available &&
                 (endpoint.capabilities.timestampSource !=
                      MediaDatagramTransmitTimestampSource::Unknown ||
                  endpoint.capabilities.timestampCounterFrequency != 0 ||
                  correlationMode != MediaDatagramTransmitCorrelationMode::None))) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "invalid transmit evidence endpoint capability"));
            }
            if (endpoint.capabilities.correlationMode ==
                    MediaDatagramTransmitCorrelationMode::KernelSequentialUint32 &&
                (!collector.m_plan ||
                 collector.m_plan->lastEvidenceId <
                     collector.m_plan->firstEvidenceId ||
                 collector.m_plan->lastEvidenceId -
                         collector.m_plan->firstEvidenceId >
                     static_cast<std::uint64_t>(
                         (std::numeric_limits<std::uint32_t>::max)()))) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "kernel timestamp correlation requires a uint32-bounded run budget"));
            }
            if (endpoint.capabilities.correlationMode ==
                    MediaDatagramTransmitCorrelationMode::CallerSelectedUint32 &&
                collector.m_plan &&
                collector.m_plan->lastEvidenceId >
                    (std::numeric_limits<std::uint32_t>::max)()) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "caller-selected timestamp ids exceed the uint32 planned run range"));
            }
            if (endpoint.capabilities.kernelTransmitTimeAvailable !=
                collector.m_kernelSchedule.has_value()) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "kernel transmit-time capability differs from its typed plan"));
            }
            EndpointState state{
                endpoint.port, endpoint.capabilities, 0, false, {}, {}};
            state.byPlatformId.reserve(
                static_cast<std::size_t>(maximumTrackedDatagrams));
            state.byLaunchTimeLowBits.reserve(
                static_cast<std::size_t>(maximumTrackedDatagrams));
            if (!collector.m_endpoints.emplace(
                    endpoint.endpointId, std::move(state)).second) {
                return ResultType::failure(::media::ErrorInfo::invalidArgument(
                    "duplicate transmit evidence endpoint"));
            }
        }
        return ResultType::success(std::move(collector));
    } catch (const std::bad_alloc&) {
        return ResultType::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramTransmitEvidenceCollector"));
    }
}

::media::Result<std::vector<MediaDatagramTransmitEvidenceReservation>>
MediaDatagramTransmitEvidenceCollector::reserveBeforeSubmit(
    std::uint64_t endpointId,
    std::span<const std::uint64_t> evidenceIds,
    std::span<const std::optional<std::uint64_t>> launchTimes,
    MediaRunningTime submittedAt) noexcept
{
    using ResultType = ::media::Result<
        std::vector<MediaDatagramTransmitEvidenceReservation>>;
    if (m_terminalFailure) return ResultType::failure(*m_terminalFailure);
    auto endpoint = m_endpoints.find(endpointId);
    if (endpoint == m_endpoints.end() || evidenceIds.empty() ||
        evidenceIds.size() != launchTimes.size() ||
        submittedAt.nanoseconds() < 0 ||
        (m_lastNow && submittedAt < *m_lastNow)) {
        return ResultType::failure(::media::ErrorInfo::invalidArgument(
            "invalid pre-submit evidence reservation"));
    }
    const bool timestampExpected = m_plan &&
        endpoint->second.capabilities.timestampAvailability ==
            MediaDatagramTransmitTimestampAvailability::Available;
    std::unordered_set<std::uint64_t> batchIds;
    try {
        batchIds.reserve(evidenceIds.size());
    } catch (const std::bad_alloc&) {
        return ResultType::failure(::media::ErrorInfo::allocationFailed(
            "pre-submit evidence batch identity"));
    }
    std::uint64_t previous = m_lastEvidenceId.value_or(0);
    std::uint64_t requiredTracking = 0;
    for (std::size_t index = 0; index < evidenceIds.size(); ++index) {
        const auto evidenceId = evidenceIds[index];
        if (evidenceId == 0 || evidenceId <= previous ||
            !batchIds.insert(evidenceId).second ||
            (m_plan && (evidenceId < m_plan->firstEvidenceId ||
                        evidenceId > m_plan->lastEvidenceId))) {
            return ResultType::failure(::media::ErrorInfo::invalidArgument(
                "evidence ids must be unique, increasing, and in range"));
        }
        previous = evidenceId;
        if (timestampExpected || launchTimes[index]) ++requiredTracking;
    }
    bool omitTimestampForReport = false;
    if (requiredTracking > m_maximumTrackedDatagrams - m_entries.size()) {
        const bool hasLaunch = std::any_of(
            launchTimes.begin(), launchTimes.end(),
            [](const auto& value) { return value.has_value(); });
        if (hasLaunch || !m_plan ||
            m_plan->coverageGapPolicy ==
                MediaDatagramEvidenceCoverageGapPolicy::Fail) {
            return ResultType::failure(::media::ErrorInfo::wouldBlock(
                "transmit observation ledger is at its planner hard bound"));
        }
        omitTimestampForReport = true;
    }

    std::vector<MediaDatagramTransmitEvidenceReservation> reservations;
    std::vector<std::uint64_t> inserted;
    try {
        reservations.reserve(evidenceIds.size());
        inserted.reserve(evidenceIds.size());
        auto nextKernelId = endpoint->second.nextKernelCorrelationId;
        auto kernelIdsExhausted =
            endpoint->second.kernelCorrelationIdsExhausted;
        for (std::size_t index = 0; index < evidenceIds.size(); ++index) {
            const bool trackTimestamp = timestampExpected &&
                                        !omitTimestampForReport;
            const bool track = trackTimestamp || launchTimes[index];
            const bool requestTimestamp = trackTimestamp ||
                (timestampExpected &&
                 endpoint->second.capabilities.correlationMode ==
                     MediaDatagramTransmitCorrelationMode::KernelSequentialUint32);
            std::optional<std::uint32_t> platformId;
            if (requestTimestamp) {
                if (endpoint->second.capabilities.correlationMode ==
                    MediaDatagramTransmitCorrelationMode::CallerSelectedUint32) {
                    if (evidenceIds[index] >
                        (std::numeric_limits<std::uint32_t>::max)()) {
                        throw ::media::ErrorInfo::invalidArgument(
                            "caller-selected timestamp id exceeds uint32 run budget");
                    }
                    platformId = static_cast<std::uint32_t>(evidenceIds[index]);
                } else {
                    if (kernelIdsExhausted) {
                        throw ::media::ErrorInfo::invalidArgument(
                            "kernel timestamp id run budget is exhausted");
                    }
                    platformId = nextKernelId;
                    if (nextKernelId ==
                        (std::numeric_limits<std::uint32_t>::max)()) {
                        kernelIdsExhausted = true;
                    } else {
                        ++nextKernelId;
                    }
                }
                if (endpoint->second.byPlatformId.contains(*platformId)) {
                    throw ::media::ErrorInfo::invalidArgument(
                        "platform timestamp id conflicts with outstanding work");
                }
            }
            reservations.push_back(
                MediaDatagramTransmitEvidenceReservation{
                    evidenceIds[index], platformId});
            if (!track) continue;
            const auto launchLow = launchTimes[index]
                ? std::optional<std::uint32_t>(static_cast<std::uint32_t>(
                      *launchTimes[index]))
                : std::nullopt;
            if (launchLow &&
                endpoint->second.byLaunchTimeLowBits.contains(*launchLow)) {
                throw ::media::ErrorInfo::invalidArgument(
                    "SO_TXTIME launch id conflicts with outstanding work");
            }
            std::optional<MediaRunningTime> launchRetainUntil;
            if (launchLow) {
                auto retainUntil = submittedAt.checkedAdd(
                    *m_launchCorrelationResidence);
                if (!retainUntil) {
                    throw ::media::ErrorInfo::invalidArgument(
                        "SO_TXTIME correlation retention is not representable");
                }
                launchRetainUntil = retainUntil.value();
            }
            Entry entry{endpointId, evidenceIds[index], platformId.value_or(0),
                        launchLow, launchRetainUntil, submittedAt,
                        EntryState::Prepared,
                        trackTimestamp, false};
            m_entries.emplace(evidenceIds[index], std::move(entry));
            inserted.push_back(evidenceIds[index]);
            if (platformId) {
                endpoint->second.byPlatformId.emplace(
                    *platformId, evidenceIds[index]);
            }
            if (launchLow) {
                endpoint->second.byLaunchTimeLowBits.emplace(
                    *launchLow, evidenceIds[index]);
            }
        }
        endpoint->second.nextKernelCorrelationId = nextKernelId;
        endpoint->second.kernelCorrelationIdsExhausted = kernelIdsExhausted;
        m_lastEvidenceId = previous;
        m_lastNow = submittedAt;
        return ResultType::success(std::move(reservations));
    } catch (const ::media::ErrorInfo& error) {
        for (const auto evidenceId : inserted) eraseEntry(evidenceId);
        return ResultType::failure(error);
    } catch (const std::bad_alloc&) {
        for (const auto evidenceId : inserted) eraseEntry(evidenceId);
        return ResultType::failure(::media::ErrorInfo::allocationFailed(
            "pre-submit evidence reservation"));
    }
}

void MediaDatagramTransmitEvidenceCollector::markSubmittedPrefix(
    std::span<const MediaDatagramTransmitEvidenceReservation> reservations,
    std::uint64_t submittedPrefix) noexcept
{
    if (submittedPrefix > reservations.size()) return;
    for (std::uint64_t index = 0; index < submittedPrefix; ++index) {
        incrementCounter(m_telemetry.submitted);
        const auto entry = m_entries.find(reservations[index].evidenceId);
        if (entry == m_entries.end()) {
            if (m_plan) incrementCounter(m_telemetry.timestampUntracked);
            continue;
        }
        if (entry->second.state != EntryState::Prepared) continue;
        entry->second.state = EntryState::Submitted;
        if (entry->second.timestampExpected) {
            incrementCounter(m_telemetry.timestampTracked);
        } else if (m_plan) {
            incrementCounter(m_telemetry.timestampUntracked);
        }
    }
    m_telemetry.deliveryEvidenceProven = false;
}

void MediaDatagramTransmitEvidenceCollector::cancelPrepared(
    std::span<const MediaDatagramTransmitEvidenceReservation> reservations,
    std::uint64_t firstIndex) noexcept
{
    for (std::uint64_t index = firstIndex; index < reservations.size(); ++index) {
        const auto entry = m_entries.find(reservations[index].evidenceId);
        if (entry != m_entries.end() &&
            entry->second.state == EntryState::Prepared) {
            eraseEntry(reservations[index].evidenceId);
        }
    }
}

::media::Status MediaDatagramTransmitEvidenceCollector::drainAvailable(
    MediaRunningTime now) noexcept
{
    if (m_terminalFailure) return ::media::Status::failure(*m_terminalFailure);
    if (now.nanoseconds() < 0 || (m_lastNow && now < *m_lastNow)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "transmit evidence clock must be nonnegative"));
    }
    m_lastNow = now;
    for (auto& [endpointId, endpoint] : m_endpoints) {
        std::vector<std::uint32_t> outstanding;
        try {
            outstanding.reserve(endpoint.byPlatformId.size());
            for (const auto& [platformId, evidenceId] : endpoint.byPlatformId) {
                const auto entry = m_entries.find(evidenceId);
                if (entry != m_entries.end() &&
                    entry->second.state == EntryState::Submitted &&
                    entry->second.timestampExpected &&
                    !entry->second.timestampObserved) {
                    outstanding.push_back(platformId);
                }
            }
        } catch (const std::bad_alloc&) {
            m_terminalFailure = ::media::ErrorInfo::allocationFailed(
                "transmit evidence drain ids");
            return ::media::Status::failure(*m_terminalFailure);
        }
        auto drained = endpoint.port->drainAvailableEvents(outstanding);
        if (!drained) {
            m_terminalFailure = drained.error();
            return ::media::Status::failure(*m_terminalFailure);
        }
        for (const auto& event : drained.value()) {
            auto ingested = ingestEvent(event, now);
            if (!ingested) return ingested;
        }
        (void)endpointId;
    }
    std::vector<std::uint64_t> expired;
    try {
        expired.reserve(m_entries.size());
        for (auto& [evidenceId, entry] : m_entries) {
            if (entry.state != EntryState::Submitted) continue;
            if (entry.timestampExpected && m_plan) {
                auto deadline = entry.submittedAt.checkedAdd(
                    m_plan->maximumDrainResidence);
                if (!deadline || now > deadline.value()) {
                    if (!entry.timestampObserved) {
                        incrementCounter(m_telemetry.lost);
                        auto status = coverageFailure(
                            "transmit timestamp evidence expired");
                        if (!status) return status;
                    }
                    const auto endpoint = m_endpoints.find(entry.endpointId);
                    if (endpoint != m_endpoints.end()) {
                        endpoint->second.byPlatformId.erase(
                            entry.platformCorrelationId);
                    }
                    entry.timestampExpected = false;
                }
            }
            if (entry.launchTimeLowBits &&
                entry.launchCorrelationRetainUntil &&
                now > *entry.launchCorrelationRetainUntil) {
                const auto endpoint = m_endpoints.find(entry.endpointId);
                if (endpoint != m_endpoints.end()) {
                    endpoint->second.byLaunchTimeLowBits.erase(
                        *entry.launchTimeLowBits);
                }
                entry.launchTimeLowBits.reset();
            }
            if (!entry.timestampExpected && !entry.launchTimeLowBits) {
                expired.push_back(evidenceId);
            }
        }
    } catch (const std::bad_alloc&) {
        m_terminalFailure = ::media::ErrorInfo::allocationFailed(
            "transmit evidence expiry scan");
        return ::media::Status::failure(*m_terminalFailure);
    }
    for (const auto evidenceId : expired) eraseEntry(evidenceId);
    m_telemetry.transmitTimestampCoverageComplete =
        m_telemetry.submitted != 0 &&
        m_telemetry.timestampTracked == m_telemetry.submitted &&
        m_telemetry.observed == m_telemetry.submitted &&
        m_telemetry.late == 0 && m_telemetry.lost == 0 &&
        m_telemetry.duplicate == 0 && m_telemetry.crossGeneration == 0 &&
        m_telemetry.unmatched == 0 && !m_telemetry.counterSaturated;
    m_telemetry.deliveryEvidenceProven = false;
    return ::media::Status::success();
}

::media::Status MediaDatagramTransmitEvidenceCollector::settleOnClose(
    MediaRunningTime now) noexcept
{
    auto drained = drainAvailable(now);
    if (!drained) return drained;
    std::vector<std::uint64_t> remaining;
    try {
        remaining.reserve(m_entries.size());
        for (const auto& [evidenceId, entry] : m_entries) {
            if (entry.state == EntryState::Submitted &&
                entry.timestampExpected && !entry.timestampObserved) {
                incrementCounter(m_telemetry.lost);
            }
            remaining.push_back(evidenceId);
        }
    } catch (const std::bad_alloc&) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed(
            "transmit evidence close settlement"));
    }
    for (const auto evidenceId : remaining) eraseEntry(evidenceId);
    if (m_plan && m_telemetry.lost != 0 &&
        m_plan->coverageGapPolicy ==
            MediaDatagramEvidenceCoverageGapPolicy::Fail) {
        return coverageFailure("transmit timestamp evidence missing at close");
    }
    m_telemetry.deliveryEvidenceProven = false;
    return ::media::Status::success();
}

::media::Status MediaDatagramTransmitEvidenceCollector::ingestEvent(
    const MediaDatagramTransmitPlatformEvent& event,
    MediaRunningTime now) noexcept
{
    if (event.generation != m_generation) {
        incrementCounter(m_telemetry.crossGeneration);
        return coverageFailure("cross-generation transmit platform event");
    }
    const auto endpoint = m_endpoints.find(event.endpointId);
    if (endpoint == m_endpoints.end()) {
        incrementCounter(m_telemetry.unmatched);
        return coverageFailure("unknown endpoint transmit platform event");
    }
    if (event.kind == MediaDatagramTransmitPlatformEventKind::Timestamp) {
        const auto mapped = endpoint->second.byPlatformId.find(
            event.platformCorrelationId);
        if (mapped == endpoint->second.byPlatformId.end()) {
            incrementCounter(m_telemetry.unmatched);
            return coverageFailure("unmatched transmit timestamp event");
        }
        const auto entry = m_entries.find(mapped->second);
        if (entry == m_entries.end() ||
            entry->second.state != EntryState::Submitted) {
            incrementCounter(m_telemetry.unmatched);
            return coverageFailure("timestamp arrived before submit commit");
        }
        if (entry->second.timestampObserved) {
            incrementCounter(m_telemetry.duplicate);
            return coverageFailure("duplicate transmit timestamp event");
        }
        if (event.timestampSource !=
                endpoint->second.capabilities.timestampSource ||
            event.rawTimestampFrequency !=
                endpoint->second.capabilities.timestampCounterFrequency ||
            event.rawTimestampFrequency == 0) {
            m_terminalFailure = ::media::ErrorInfo::ioFailure(
                "transmit timestamp clock metadata changed at runtime");
            return ::media::Status::failure(*m_terminalFailure);
        }
        auto deadline = entry->second.submittedAt.checkedAdd(
            m_plan->maximumDrainResidence);
        if (!deadline || now > deadline.value()) {
            incrementCounter(m_telemetry.late);
            auto status = coverageFailure("late transmit timestamp event");
            if (!status) return status;
        }
        entry->second.timestampObserved = true;
        incrementCounter(m_telemetry.observed);
        m_telemetry.lastTimestampSource = event.timestampSource;
        m_telemetry.lastRawTimestampCounter = event.rawTimestampCounter;
        m_telemetry.lastRawTimestampFrequency = event.rawTimestampFrequency;
        return ::media::Status::success();
    }

    const auto mapped = endpoint->second.byLaunchTimeLowBits.find(
        event.launchTimeLowBits);
    if (mapped == endpoint->second.byLaunchTimeLowBits.end()) {
        m_terminalFailure = ::media::ErrorInfo::ioFailure(
            "SO_TXTIME error is not correlated to submitted work");
        return ::media::Status::failure(*m_terminalFailure);
    }
    const auto entry = m_entries.find(mapped->second);
    if (entry == m_entries.end() || entry->second.state != EntryState::Submitted) {
        m_terminalFailure = ::media::ErrorInfo::ioFailure(
            "SO_TXTIME error arrived for unsubmitted work");
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (event.kind == MediaDatagramTransmitPlatformEventKind::TxTimeMissed) {
        incrementCounter(m_telemetry.txTimeMissed);
    } else if (event.kind ==
               MediaDatagramTransmitPlatformEventKind::TxTimeInvalid) {
        incrementCounter(m_telemetry.txTimeInvalid);
    } else {
        m_terminalFailure = ::media::ErrorInfo::ioFailure(
            "unknown SO_TXTIME error kind");
        return ::media::Status::failure(*m_terminalFailure);
    }
    m_terminalFailure = ::media::ErrorInfo::ioFailure(
        "kernel rejected or missed scheduled Datagram launch");
    return ::media::Status::failure(*m_terminalFailure);
}

void MediaDatagramTransmitEvidenceCollector::eraseEntry(
    std::uint64_t evidenceId) noexcept
{
    const auto entry = m_entries.find(evidenceId);
    if (entry == m_entries.end()) return;
    const auto endpoint = m_endpoints.find(entry->second.endpointId);
    if (endpoint != m_endpoints.end()) {
        if (entry->second.timestampExpected) {
            endpoint->second.byPlatformId.erase(
                entry->second.platformCorrelationId);
        }
        if (entry->second.launchTimeLowBits) {
            endpoint->second.byLaunchTimeLowBits.erase(
                *entry->second.launchTimeLowBits);
        }
    }
    m_entries.erase(entry);
}

void MediaDatagramTransmitEvidenceCollector::incrementCounter(
    std::uint64_t& counter) noexcept
{
    if (counter == (std::numeric_limits<std::uint64_t>::max)()) {
        m_telemetry.counterSaturated = true;
        return;
    }
    ++counter;
}

::media::Status MediaDatagramTransmitEvidenceCollector::coverageFailure(
    const char* message) noexcept
{
    if (m_plan && m_plan->coverageGapPolicy ==
                      MediaDatagramEvidenceCoverageGapPolicy::Fail) {
        m_terminalFailure = ::media::ErrorInfo::ioFailure(message);
        return ::media::Status::failure(*m_terminalFailure);
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
