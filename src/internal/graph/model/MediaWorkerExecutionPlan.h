#pragma once
#include "internal/graph/core/MediaNodeId.h"
namespace media::ffmpeg::graph {
enum class MediaWorkerExecutionRole { Ordinary, DatagramDistribution };
enum class MediaWorkerPlatformExecution { WindowsOrdinary, WindowsDistribution, LinuxNative };
struct MediaWorkerExecutionPlan final {
    MediaNodeId node;
    MediaWorkerExecutionRole role;
    MediaWorkerPlatformExecution platform;
};
}
