#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaPayloadKind.h"
#include "internal/graph/model/MediaStreamKind.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphPayloadCreditIntegration : std::uint8_t {
    Incomplete = 0,
    Complete = 1
};

enum class MediaGraphPayloadAllocationAccounting : std::uint8_t {
    EngineManagedBytesAndObject = 1,
    ObservedOnlyExternalBytesAndEngineManagedObject = 2
};

struct MediaGraphPayloadProducerStrategy final {
    MediaNodeId nodeId;
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaPayloadKind payloadKind = MediaPayloadKind::Unknown;
    MediaGraphPayloadAllocationAccounting accounting =
        MediaGraphPayloadAllocationAccounting::EngineManagedBytesAndObject;
    std::uint64_t maximumReservationBytes = 0;
    std::string authority;

    bool valid() const noexcept
    {
        return nodeId.isValid() && streamKind != MediaStreamKind::Unknown &&
            payloadKind != MediaPayloadKind::Unknown &&
            maximumReservationBytes > 0 && !authority.empty();
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

    bool isStructurallyValid() const noexcept
    {
        return maximumBytes > 0 && maximumObjects > 0 &&
            maximumUnitBytes > 0 && maximumUnitBytes <= maximumBytes &&
            producerStrategyVersion > 0 && !authority.empty() &&
            !producers.empty();
    }

    bool isCompleteAndValid() const noexcept
    {
        if (!isStructurallyValid() ||
            integration != MediaGraphPayloadCreditIntegration::Complete) {
            return false;
        }
        for (std::size_t index = 0; index < producers.size(); ++index) {
            if (!producers[index].valid() ||
                producers[index].maximumReservationBytes > maximumUnitBytes) {
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
