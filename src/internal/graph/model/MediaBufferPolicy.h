#pragma once

#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaMemoryBudget.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaBufferOwnership {
    Unknown,
    MoveOnly,
    SharedRef,
    Borrowed
};

enum class MediaBufferPoolMode {
    None,
    PerChannel,
    PerNode,
    SharedGraphPool
};

enum class MediaBufferLifetime {
    Unknown,
    SingleConsumer,
    MultiConsumer,
    UntilFlush,
    UntilGraphStop
};

struct MediaBufferPolicy {
    MediaBufferOwnership ownership = MediaBufferOwnership::SharedRef;
    MediaBufferPoolMode poolMode = MediaBufferPoolMode::PerChannel;
    MediaBufferLifetime lifetime = MediaBufferLifetime::SingleConsumer;

    MediaHardwareFrameKind memoryKind = MediaHardwareFrameKind::Unknown;
    MediaMemoryBudget memoryBudget;
    std::string sharedAllocationGroup;

    bool allowPoolReuse = true;
    bool requireWritable = false;
    bool zeroCopyPreferred = true;
    bool allowHardwareFrames = true;
    bool allowSoftwareTransfer = true;

    constexpr bool operator==(const MediaBufferPolicy&) const noexcept = default;

    constexpr bool usesPool() const noexcept
    {
        return poolMode != MediaBufferPoolMode::None;
    }
};

} // namespace media::ffmpeg::graph
