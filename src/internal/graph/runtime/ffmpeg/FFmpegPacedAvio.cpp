#include "internal/graph/runtime/ffmpeg/FFmpegPacedAvio.h"

extern "C" {
#include <libavformat/avio.h>
#include <libavformat/version_major.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <new>
#include <thread>

namespace media::ffmpeg {
namespace {
