#pragma once
#include "internal/graph/model/MediaWorkerExecutionPlan.h"
#include "media_transcode/Result.h"
#include <cstdint>
namespace media::ffmpeg::graph {
// Owner-thread registration. No policy selection or fallback lives here.
class MediaWorkerThreadRegistration final {
public:
    MediaWorkerThreadRegistration() = default;
    ~MediaWorkerThreadRegistration();
    MediaWorkerThreadRegistration(const MediaWorkerThreadRegistration&) = delete;
    MediaWorkerThreadRegistration& operator=(const MediaWorkerThreadRegistration&) = delete;
    ::media::Status enter(const MediaWorkerExecutionPlan& plan);
    ::media::Status leave() noexcept;
private:
    friend ::media::Status probeMediaWindowsDistributionRegistration();
    ::media::Status enterDistribution();
    void* m_handle = nullptr;
    std::uint32_t m_ownerThread = 0;
    MediaNodeId m_node;
};
// Runs one joined temporary thread, retaining no handle across thread boundaries.
::media::Status probeMediaWindowsDistributionRegistration();
}
