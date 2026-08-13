#pragma once

#if defined(_WIN32)
#include "internal/graph/runtime/filesystem/MediaWin32AtomicFileReplacePort.h"
#else
#include "internal/graph/runtime/filesystem/MediaPosixAtomicFileReplacePort.h"
#endif

namespace media::ffmpeg::graph {

#if defined(_WIN32)
using MediaPlatformAtomicFileReplacePort = MediaWin32AtomicFileReplacePort;
#else
using MediaPlatformAtomicFileReplacePort = MediaPosixAtomicFileReplacePort;
#endif

} // namespace media::ffmpeg::graph
