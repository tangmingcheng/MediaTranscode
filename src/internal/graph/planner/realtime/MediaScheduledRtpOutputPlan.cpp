#include "internal/graph/planner/realtime/MediaScheduledRtpOutputPlan.h"

#include <type_traits>

namespace media::ffmpeg::graph {

static_assert(!std::is_copy_constructible_v<MediaScheduledRtpOutputPlan>);

} // namespace media::ffmpeg::graph
