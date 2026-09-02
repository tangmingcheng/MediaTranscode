#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

enum class MediaDatagramTransmitExecutionMode {
    Unknown = 0,
    UserspaceNonblocking = 1,
    LinuxSocketTxTime = 2
};

struct MediaDatagramTransmitKernelSchedulePlan final {
    std::string authority;
    std::uint64_t maximumCorrelationEntries;
    std::uint64_t maximumRunDatagrams;
    MediaRunningTime maximumErrorQueueResidence;
    std::uint64_t maximumScheduleAheadNanoseconds;
};

struct MediaDatagramTransmitExecutionPlan final {
    MediaDatagramTransmitExecutionMode mode;
    std::string authority;
    std::optional<MediaDatagramTransmitKernelSchedulePlan> kernelSchedule;
};

} // namespace media::ffmpeg::graph
