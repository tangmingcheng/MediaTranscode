#include "internal/graph/protocol/rtp/MediaHevcRtpCapability.h"

#include "internal/graph/utils/MediaCodecNameUtils.h"

namespace media::ffmpeg::graph {

::media::Status validateHevcRtpNonInterleavedFmtp(
    const MediaRtpFmtpParameters& parameters)
{
    const auto txMode = parameters.find("tx-mode");
    if (txMode != parameters.end() &&
        lowercaseAscii(txMode->second) != "srst") {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "HEVC RTP only supports non-interleaved SRST transmission"));
    }
    if (!parameters.contains("sprop-max-don-diff")) {
        return ::media::Status::success();
    }
    auto maximumDonDifference = requiredRtpFmtpInt(
        parameters, "sprop-max-don-diff");
    if (!maximumDonDifference) {
        return ::media::Status::failure(maximumDonDifference.error());
    }
    if (maximumDonDifference.value() != 0) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "HEVC RTP DONL optional payload header is unsupported: sprop-max-don-diff"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
