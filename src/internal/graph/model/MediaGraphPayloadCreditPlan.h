#pragma once

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaGraphPayloadCreditIntegration : std::uint8_t {
    Incomplete = 0,
    Complete = 1
};

struct MediaGraphPayloadCreditPlan final {
    std::uint64_t maximumBytes = 0;
    std::uint64_t maximumObjects = 0;
    std::uint64_t maximumUnitBytes = 0;
    std::uint32_t producerStrategyVersion = 0;
    MediaGraphPayloadCreditIntegration integration =
        MediaGraphPayloadCreditIntegration::Incomplete;
    std::string authority;

    bool isStructurallyValid() const noexcept
    {
        return maximumBytes > 0 && maximumObjects > 0 &&
            maximumUnitBytes > 0 && maximumUnitBytes <= maximumBytes &&
            producerStrategyVersion > 0 && !authority.empty();
    }

    bool isCompleteAndValid() const noexcept
    {
        return isStructurallyValid() &&
            integration == MediaGraphPayloadCreditIntegration::Complete;
    }
};

} // namespace media::ffmpeg::graph
