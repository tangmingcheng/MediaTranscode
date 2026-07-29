#pragma once

#include "internal/graph/model/MediaPacketSourceTiming.h"
#include "internal/graph/nodes/FFmpegNodeRuntime.h"
#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/lineage/MediaCanonicalAudioSourceTimeline.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaCanonicalInputNode final : public FFmpegNodeRuntime {
public:
    explicit MediaCanonicalInputNode(MediaNodeId nodeId);
    static MediaNodeKind staticKind() noexcept;
    ::media::Status stop(MediaGraphExecutionContext& context) override;
    void abort(MediaGraphExecutionContext& context) noexcept override;

    static ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
    canonicalize(MediaBufferRef encodedAccessUnit,
                 const MediaPacketSourceTiming& protocolTiming,
                 MediaRunningTime duration,
                 MediaScheduledStream stream,
                 MediaDecodeOrderMode decodeOrder,
                 std::string sourceIdentity,
                 MediaSourceAccessUnitSequence sourceSequence,
                 std::optional<MediaCanonicalAudioSampleInterval> audioInterval);

protected:
    ::media::Result<MediaNodeProcessResult> onProcess(
        MediaGraphExecutionContext& context) override;

private:
    ::media::Status configure(MediaGraphExecutionContext& context);
    ::media::Result<MediaRunningTime> durationFor(
        const MediaBufferRef& packet) const;
    void resetState() noexcept;
    std::optional<MediaScheduledStream> m_stream;
    std::optional<MediaDecodeOrderMode> m_decodeOrder;
    bool m_keyTraceEmitted = false;
    std::string m_sourceIdentity;
    std::optional<std::uint64_t> m_generation;
    std::uint64_t m_nextSequence = 1;
    int m_audioSampleRate = 0;
    std::uint32_t m_audioSampleCount = 0;
    std::optional<MediaCanonicalAudioSourceTimeline> m_audioTimeline;
};

} // namespace media::ffmpeg::graph
