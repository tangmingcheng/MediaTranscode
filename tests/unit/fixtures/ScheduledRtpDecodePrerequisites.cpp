#include "unit/fixtures/ScheduledRtpDecodePrerequisites.h"

#include "unit/fixtures/ScheduledRtpDecodeReceiver.h"

#include <iostream>

namespace media_transcode::test {

::media::Status ScheduledRtpDecodePrerequisites::check(
    const std::filesystem::path& ffmpeg,
    const std::filesystem::path& sample)
{
    if (auto platform = ScheduledRtpDecodeReceiver::preflightPlatformApis();
        !platform) {
        return platform;
    }
    if (auto executable = ScheduledRtpDecodeReceiver::preflightExecutable(
            ffmpeg); !executable) {
        return executable;
    }
    std::error_code filesystemError;
    if (!std::filesystem::is_regular_file(sample, filesystemError)) {
        return ::media::Status::failure(::media::ErrorInfo::unsupported(
            "scheduled RTP decode sample prerequisite is unavailable"));
    }
    return ::media::Status::success();
}

int ScheduledRtpDecodePrerequisites::externalFailureExit(
    const ::media::ErrorInfo& error)
{
    std::cerr << error.describe() << '\n';
    return 77;
}

int ScheduledRtpDecodePrerequisites::pipelineFailureExit(
    const ::media::ErrorInfo& error)
{
    std::cerr << error.describe() << '\n';
    return 1;
}

} // namespace media_transcode::test
