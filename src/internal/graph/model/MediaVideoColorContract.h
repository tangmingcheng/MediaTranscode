#pragma once
extern "C" {
#include <libavutil/pixfmt.h>
}
namespace media::ffmpeg::graph {
// Complete observed labels; equal unspecified labels preserve pixel-domain
// composition semantics without claiming color normalization.
struct MediaVideoColorContract final {
    AVColorRange range;
    AVColorPrimaries primaries;
    AVColorTransferCharacteristic transfer;
    AVColorSpace matrix;
    friend bool operator==(const MediaVideoColorContract&, const MediaVideoColorContract&) = default;
};
} // namespace media::ffmpeg::graph
