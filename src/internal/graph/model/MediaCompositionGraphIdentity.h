#pragma once
#include "internal/graph/core/MediaNodeId.h"
#include <vector>
namespace media::ffmpeg::graph {
// Owned identity maps returned only after complete topology validation.
struct MediaCompositionGraphIdentity final {
    std::vector<MediaNodeId> positions;
    std::vector<MediaEdgeId> connections;
};
}
