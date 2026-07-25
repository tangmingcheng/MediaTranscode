#include "common/TestAssert.h"

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
}

#include <iostream>

int main()
{
    constexpr int UnsupportedExitCode = 77;
    const AVHWDeviceType type = av_hwdevice_find_type_by_name("cuda");
    if (type == AV_HWDEVICE_TYPE_NONE) {
        std::cout << "SKIPPED: unsupported hardware backend cuda is not registered\n";
        return UnsupportedExitCode;
    }
    AVBufferRef* device = nullptr;
    const int result = av_hwdevice_ctx_create(&device, type, nullptr, nullptr, 0);
    if (result < 0 || !device) {
        std::cout << "SKIPPED: unsupported hardware device cuda cannot be created, ffmpeg_error="
                  << result << '\n';
        if (device) av_buffer_unref(&device);
        return UnsupportedExitCode;
    }
    av_buffer_unref(&device);
    std::cout << "HARDWARE_SUPPORTED backend=cuda device_created=true\n";
    return 0;
}
