#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphPayloadCreditMode : std::uint8_t {
    NonRealtimeNotApplicable = 1,
    RealtimeRequired = 2,
};

enum class MediaGraphPayloadCreditIntegration : std::uint8_t {
    Incomplete = 0,
    Complete = 1
};

enum class MediaGraphPayloadAllocationAccounting : std::uint8_t {
    EngineManagedBytesAndObject = 1,
    ObservedOnlyExternalBytesAndEngineManagedObject = 2
};

enum class MediaFrameCreditAllocationScope : std::uint8_t {
    EngineLogicalBytes = 1,
    ExternalDeviceObservedOnly = 2
};

struct MediaFrameCreditContract final {
    MediaFrameCreditAllocationScope allocationScope =
        MediaFrameCreditAllocationScope::EngineLogicalBytes;
    std::uint64_t maximumLogicalBytes = 0;
    std::uint64_t maximumObjectsPerAllocation = 0;
    std::string authority;

    bool valid() const noexcept
    {
        return maximumLogicalBytes > 0 &&
            maximumObjectsPerAllocation == 1 && !authority.empty();
    }

    std::uint64_t reservedBytes() const noexcept
    {
        return allocationScope ==
                MediaFrameCreditAllocationScope::EngineLogicalBytes
            ? maximumLogicalBytes : 0;
    }
};

struct MediaGraphPayloadProducerStrategy final {
    MediaNodeId nodeId;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;
    MediaGraphPayloadAllocationAccounting accounting =
        MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject;
    std::optional<MediaFrameCreditContract> frameCredit;
    std::uint64_t maximumReservationBytes = 0;
    bool runtimeIntegrated = false;
    std::string authority;

    bool valid() const noexcept
    {
        const bool frameContractValid = payloadKind == MediaPayloadKind::Frame
            ? frameCredit && frameCredit->valid() &&
                frameCredit->maximumLogicalBytes == maximumReservationBytes &&
                accounting == (frameCredit->allocationScope ==
                        MediaFrameCreditAllocationScope::EngineLogicalBytes
                    ? MediaGraphPayloadAllocationAccounting::
                        EngineManagedBytesAndObject
                    : MediaGraphPayloadAllocationAccounting::
                        ObservedOnlyExternalBytesAndEngineManagedObject)
            : !frameCredit;
        return nodeId.isValid() && streamKind != MediaStreamKind::Unknown &&
            payloadKind != MediaPayloadKind::Unknown &&
            frameContractValid && maximumReservationBytes > 0 &&
            !authority.empty();
    }

    std::uint64_t maximumReservedBytes() const noexcept
    {
        return accounting == MediaGraphPayloadAllocationAccounting::
                EngineManagedBytesAndObject
            ? maximumReservationBytes : 0;
    }
};

struct MediaGraphPayloadProducerRequirement final {
    MediaNodeId nodeId;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;
    std::string missingAuthority;

    bool valid() const noexcept
    {
        return nodeId.isValid() && streamKind != MediaStreamKind::Unknown &&
            payloadKind != MediaPayloadKind::Unknown &&
            !missingAuthority.empty();
    }
};

struct MediaGraphPayloadCreditPlan final {
    std::uint64_t maximumBytes = 0;
    std::uint64_t maximumObjects = 0;
    std::uint64_t maximumUnitBytes = 0;
    std::uint32_t producerStrategyVersion = 0;
    MediaGraphPayloadCreditIntegration integration =
        MediaGraphPayloadCreditIntegration::Incomplete;
    std::string authority;
    std::vector<MediaGraphPayloadProducerStrategy> producers;
    std::vector<MediaGraphPayloadProducerRequirement> missingProducers;

    bool isStructurallyValid() const noexcept
    {
        if (maximumBytes == 0 || maximumObjects == 0 ||
            maximumUnitBytes == 0 ||
            producerStrategyVersion == 0 || authority.empty() ||
            (producers.empty() && missingProducers.empty())) {
            return false;
        }
        for (const auto& producer : producers) {
            if (!producer.valid()) return false;
        }
        for (const auto& missing : missingProducers) {
            if (!missing.valid()) return false;
        }
        return true;
    }

    bool isCompleteAndValid() const noexcept
    {
        if (!isStructurallyValid() ||
            integration != MediaGraphPayloadCreditIntegration::Complete ||
            !missingProducers.empty()) {
            return false;
        }
        for (std::size_t index = 0; index < producers.size(); ++index) {
            if (!producers[index].valid() ||
                producers[index].maximumReservedBytes() > maximumUnitBytes ||
                !producers[index].runtimeIntegrated) {
                return false;
            }
            for (std::size_t previous = 0; previous < index; ++previous) {
                if (producers[previous].nodeId == producers[index].nodeId &&
                    producers[previous].streamKind == producers[index].streamKind &&
                    producers[previous].payloadKind == producers[index].payloadKind) {
                    return false;
                }
            }
        }
        return true;
    }
};

} // namespace media::ffmpeg::graph
