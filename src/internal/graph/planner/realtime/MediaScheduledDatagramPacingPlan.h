#pragma once

#include "internal/graph/time/MediaRunningTime.h"

namespace media::ffmpeg::graph {

enum class MediaDatagramDispatchExecution {
    UserspaceWaitAndSend,
    KernelScheduledTransmit
};

enum class MediaDatagramTimingEvidence {
    UserspaceSendReturn,
    TransmitTimestamp
};

enum class MediaDatagramDeadlinePolicy {
    CanonicalOrdered
};

struct MediaScheduledDatagramPacingPlan final {
    MediaDatagramDispatchExecution execution;
    MediaDatagramTimingEvidence evidence;
    MediaDatagramDeadlinePolicy deadlinePolicy;

    friend bool operator==(
        const MediaScheduledDatagramPacingPlan&,
        const MediaScheduledDatagramPacingPlan&) = default;
};

} // namespace media::ffmpeg::graph
