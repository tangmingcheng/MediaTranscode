#pragma once

#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/time/MediaCanonicalTimeMapper.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaCanonicalInputNode final : public FFmpegNodeRuntime {
public:
    explicit MediaCanonicalInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

    static ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
    canonicalize(MediaBufferRef encodedAccessUnit,
                 const MediaCanonicalSourceTimestamp& protocolTimestamp,
                 const MediaCanonicalTimeMapper& mapper,
                 MediaScheduledStream stream,
                 MediaDecodeOrderMode decodeOrder,
                 MediaSourceAccessUnitSequence sourceSequence);

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    void resetState() noexcept;
    std::unique_ptr<MediaCanonicalTimeMapper> m_mapper;
    std::optional<MediaScheduledStream> m_stream;
    std::optional<MediaDecodeOrderMode> m_decodeOrder;
    std::optional<MediaRunningTime> m_duration;
    std::string m_sourceIdentity;
    std::uint64_t m_generation = 0;
    std::uint64_t m_nextSequence = 1;
    int m_audioSampleRate = 0;
    std::uint32_t m_audioSampleCount = 0;
};

} // namespace media::ffmpeg::graph
