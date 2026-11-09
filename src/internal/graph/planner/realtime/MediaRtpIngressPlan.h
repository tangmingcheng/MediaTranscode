#pragma once

#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"
#include "internal/graph/planner/realtime/MediaRtpIngressObservation.h"
#include "internal/graph/planner/realtime/MediaRtpInputWaitPlan.h"

#include <cstddef>
#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaRtpIngressPlanFacts final {
    MediaRtpIngressAdapterKind adapterKind;
    std::size_t socketReceiveCapacityBytes;
    std::size_t maximumDatagramBytes;
    std::size_t batchByteCapacity;
    std::size_t descriptorCapacity;
    std::size_t requiredBufferAlignmentBytes;
    std::size_t reorderWindowPackets;
    std::int64_t maximumReorderDelayNanoseconds;
    MediaRtpIngressStorageOwnership storageOwnership;
    MediaRtpIngressCancellationContract cancellationContract;
    MediaRtpIngressCompletionEvidence completionEvidence;

    bool operator==(const MediaRtpIngressPlanFacts&) const noexcept = default;
};

class MediaRtpIngressPlan final {
public:
    MediaRtpIngressPlan() = delete;

    static ::media::Result<MediaRtpIngressPlan> create(
        const MediaRtpReceiveGeometry& geometry,
        const MediaRtpIngressObservation& observation,
        const MediaRtpInputWaitPlan& inputWait);
    static ::media::Result<MediaRtpIngressPlan> fromFacts(
        MediaRtpIngressPlanFacts facts);

    const MediaRtpReceiveGeometry& receiveGeometry() const noexcept;
    MediaRtpIngressAdapterKind adapterKind() const noexcept;
    std::size_t socketReceiveCapacityBytes() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t batchByteCapacity() const noexcept;
    std::size_t descriptorCapacity() const noexcept;
    std::size_t requiredBufferAlignmentBytes() const noexcept;
    std::size_t reorderWindowPackets() const noexcept;
    std::int64_t maximumReorderDelayNanoseconds() const noexcept;
    MediaRtpIngressStorageOwnership storageOwnership() const noexcept;
    MediaRtpIngressCancellationContract cancellationContract() const noexcept;
    MediaRtpIngressCompletionEvidence completionEvidence() const noexcept;
    MediaRtpIngressPlanFacts facts() const noexcept;
    bool sameProduct(const MediaRtpIngressPlan& other) const noexcept;
    ::media::Status validateProduct() const;

private:
    MediaRtpIngressPlan(
        MediaRtpReceiveGeometry geometry,
        std::size_t reorderWindowPackets,
        std::int64_t maximumReorderDelayNanoseconds) noexcept;

    MediaRtpReceiveGeometry m_receiveGeometry;
    std::size_t m_reorderWindowPackets;
    std::int64_t m_maximumReorderDelayNanoseconds;
};

} // namespace media::ffmpeg::graph
