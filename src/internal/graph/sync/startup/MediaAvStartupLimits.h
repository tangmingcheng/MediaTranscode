#pragma once

#include <cstddef>

namespace media::ffmpeg::graph {

// The generic input observation budget; runtime capacity is a planner product.
inline constexpr std::size_t MediaAvStartupPreflightUnitCapacity = 256;

} // namespace media::ffmpeg::graph
