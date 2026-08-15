#pragma once

#include "media_transcode/Result.h"

#include <cstddef>

namespace media::ffmpeg::graph {

enum class MediaRtpIngressAdapterKind {
    WindowsRegisteredIo = 0,
    WindowsOverlappedCompletionQueue = 1,
    LinuxIoUringZeroCopy = 2,
    LinuxReceiveMultipleMessages = 3
};

enum class MediaRtpIngressStorageOwnership {
    RegisteredReusableArena = 0,
    ReusableMessageArena = 1
};

enum class MediaRtpIngressCancellationContract {
    CompletionQueueWake = 0,
    DescriptorWake = 1
};

enum class MediaRtpIngressCompletionEvidence {
    RegisteredCompletionQueue = 0,
    OverlappedCompletionPort = 1,
    IoUringCompletionQueue = 2,
    ReceiveMultipleMessagesReturn = 3
};

struct MediaRtpIngressCapabilityFacts final {
    MediaRtpIngressAdapterKind adapterKind;
    std::size_t effectiveSocketReceiveBytes;
    std::size_t maximumReceiveDescriptors;
    std::size_t requiredBufferAlignmentBytes;
    MediaRtpIngressStorageOwnership storageOwnership;
    MediaRtpIngressCancellationContract cancellationContract;
    MediaRtpIngressCompletionEvidence completionEvidence;
};

class MediaRtpIngressCapability final {
public:
    MediaRtpIngressCapability() = delete;

    static ::media::Result<MediaRtpIngressCapability> create(
        MediaRtpIngressCapabilityFacts facts);

    MediaRtpIngressAdapterKind adapterKind() const noexcept;
    std::size_t effectiveSocketReceiveBytes() const noexcept;
    std::size_t maximumReceiveDescriptors() const noexcept;
    std::size_t requiredBufferAlignmentBytes() const noexcept;
    MediaRtpIngressStorageOwnership storageOwnership() const noexcept;
    MediaRtpIngressCancellationContract cancellationContract() const noexcept;
    MediaRtpIngressCompletionEvidence completionEvidence() const noexcept;
    ::media::Status validateProduct() const;

private:
    explicit MediaRtpIngressCapability(
        MediaRtpIngressCapabilityFacts facts) noexcept;

    MediaRtpIngressCapabilityFacts m_facts;
};

} // namespace media::ffmpeg::graph
