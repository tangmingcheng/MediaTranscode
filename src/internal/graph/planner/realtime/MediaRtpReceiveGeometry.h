#pragma once

#include "internal/graph/planner/realtime/MediaRtpIngressCapability.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpChannel.h"

#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaRtpReceiveGeometryFacts final {
    MediaRtpIngressAdapterKind adapterKind;
    std::size_t socketReceiveCapacityBytes;
    std::size_t maximumDatagramBytes;
    std::size_t batchByteCapacity;
    std::size_t descriptorCapacity;
    std::size_t requiredBufferAlignmentBytes;
    MediaRtpIngressStorageOwnership storageOwnership;
    MediaRtpIngressCancellationContract cancellationContract;
    MediaRtpIngressCompletionEvidence completionEvidence;
    MediaRtpUdpChannel channel;

    bool operator==(const MediaRtpReceiveGeometryFacts&) const noexcept = default;
};

// Receive storage is planned before the first datagram. Reorder timing is not
// part of this product and must be derived separately from source observations.
class MediaRtpReceiveGeometry final {
public:
    MediaRtpReceiveGeometry() = delete;
    static ::media::Result<MediaRtpReceiveGeometry> create(
        const MediaRtpIngressCapability& capability,
        std::size_t preparedInputByteBudget,
        std::size_t maximumDatagramBytes,
        MediaRtpUdpChannel channel);
    static ::media::Result<MediaRtpReceiveGeometry> fromFacts(
        MediaRtpReceiveGeometryFacts facts);

    MediaRtpIngressAdapterKind adapterKind() const noexcept;
    std::size_t socketReceiveCapacityBytes() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t batchByteCapacity() const noexcept;
    std::size_t descriptorCapacity() const noexcept;
    std::size_t requiredBufferAlignmentBytes() const noexcept;
    MediaRtpIngressStorageOwnership storageOwnership() const noexcept;
    MediaRtpIngressCancellationContract cancellationContract() const noexcept;
    MediaRtpIngressCompletionEvidence completionEvidence() const noexcept;
    MediaRtpUdpChannel channel() const noexcept;
    MediaRtpReceiveGeometryFacts facts() const noexcept;
    bool sameProduct(const MediaRtpReceiveGeometry& other) const noexcept;
    ::media::Status validateProduct() const;

private:
    explicit MediaRtpReceiveGeometry(MediaRtpReceiveGeometryFacts facts) noexcept;
    MediaRtpReceiveGeometryFacts m_facts;
};

} // namespace media::ffmpeg::graph
