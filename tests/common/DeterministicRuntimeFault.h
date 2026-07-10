#pragma once

#include "internal/graph/runtime/MediaRuntimeNode.h"
#include "media_transcode/Result.h"

#include <atomic>
#include <initializer_list>
#include <vector>

namespace media_transcode::test {

enum class RuntimeFaultStep {
    Progress,
    Waiting,
    Finished,
    Failure
};

class DeterministicRuntimeFaultNode final : public media::ffmpeg::graph::MediaRuntimeNode {
public:
    DeterministicRuntimeFaultNode(media::ffmpeg::graph::MediaNodeId id,
                                  std::initializer_list<RuntimeFaultStep> steps)
        : m_id(id)
        , m_steps(steps)
    {
    }

    media::ffmpeg::graph::MediaNodeId nodeId() const noexcept override
    {
        return m_id;
    }

    ::media::Result<media::ffmpeg::graph::MediaNodeProcessResult>
    process(media::ffmpeg::graph::MediaGraphExecutionContext&) override
    {
        const std::size_t index = calls.fetch_add(1);
        const RuntimeFaultStep step = index < m_steps.size() ? m_steps[index] : RuntimeFaultStep::Finished;
        using media::ffmpeg::graph::MediaNodeProcessResult;
        switch (step) {
        case RuntimeFaultStep::Progress:
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
        case RuntimeFaultStep::Waiting:
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
        case RuntimeFaultStep::Finished:
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        case RuntimeFaultStep::Failure:
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::internalError("deterministic runtime fault"));
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError("unknown deterministic runtime fault step"));
    }

    std::atomic_size_t calls{ 0 };

private:
    media::ffmpeg::graph::MediaNodeId m_id;
    std::vector<RuntimeFaultStep> m_steps;
};

} // namespace media_transcode::test
