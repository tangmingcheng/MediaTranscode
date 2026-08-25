#pragma once

#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaMpegTsUdpWireDatagramMaterializer;
class MediaRtpWireDatagramMaterializer;
class MediaWireDatagramBatchPartitionBuilder;

class MediaWireDatagramBatchBuilder final {
public:
    ::media::Status append(
        std::span<const std::uint8_t> bytes,
        std::uint64_t endpointId,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline,
        std::uint64_t globalSequence,
        MediaDatagramSubmitCommitLease commitLease);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>> finish();

private:
    friend class MediaMpegTsUdpWireDatagramMaterializer;
    friend class MediaRtpWireDatagramMaterializer;
    friend class MediaWireDatagramBatchPartitionBuilder;

    static ::media::Result<MediaWireDatagramBatchBuilder> create(
        const std::string& sessionKey,
        const std::string& serviceScopeId,
        std::uint64_t generation);
    MediaWireDatagramBatchBuilder(
        std::string sessionKey,
        std::string serviceScopeId,
        std::uint64_t generation) noexcept;

    std::string m_sessionKey;
    std::string m_serviceScopeId;
    std::uint64_t m_generation;
    std::vector<std::uint8_t> m_payload;
    std::vector<MediaWireDatagramBatchEntry> m_entries;
    bool m_finished = false;
};

} // namespace media::ffmpeg::graph
