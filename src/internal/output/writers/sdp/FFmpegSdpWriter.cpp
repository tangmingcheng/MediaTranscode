#include "internal/output/writers/sdp/FFmpegSdpWriter.h"

#include "internal/FFmpegError.h"

#include <fstream>
#include <string>

namespace media::ffmpeg {

Status FFmpegSdpWriter::save(AVFormatContext* formatContext,
                             const std::string& path)
{
    if (!formatContext) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegSdpWriter save failed: formatContext is null"));
    }

    if (path.empty()) {
        return Status::success();
    }

    char buffer[16384] = {};
    AVFormatContext* contexts[] = { formatContext };
    const int ret = av_sdp_create(contexts, 1, buffer, sizeof(buffer));
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "av_sdp_create failed",
            ret));
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out) {
        return Status::failure(ErrorInfo::ioFailure(
            "FFmpegSdpWriter save failed: open output path failed: " + path));
    }

    out << buffer;
    if (!out) {
        return Status::failure(ErrorInfo::ioFailure(
            "FFmpegSdpWriter save failed: write output path failed: " + path));
    }

    return Status::success();
}

} // namespace media::ffmpeg
