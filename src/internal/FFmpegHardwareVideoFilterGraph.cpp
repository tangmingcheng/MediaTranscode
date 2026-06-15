#include "internal/FFmpegHardwareVideoFilterGraph.h"

#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {

    std::string HardwareVideoFilterGraphBuilder::buildDescription(const Config& config,
                                                                  std::string* error)
    {
        if (config.deviceType == HardwareDeviceType::None ||
            config.deviceType == HardwareDeviceType::Auto) {
            if (error) {
                *error = "hardware filter graph build failed: invalid hardware device type";
            }
            return {};
        }

        std::ostringstream desc;
        bool hasFilter = false;

        if (config.enableScale) {
            if (!supportsHardwareScale(config.deviceType)) {
                if (error) {
                    *error = "hardware filter graph build failed: device does not have a mapped scale filter";
                }
                return {};
            }

            if (config.outputWidth <= 0 || config.outputHeight <= 0) {
                if (error) {
                    *error = "hardware filter graph build failed: invalid output size";
                }
                return {};
            }

            desc << scaleFilterName(config.deviceType)
                 << "="
                 << config.outputWidth
                 << ":"
                 << config.outputHeight;
            hasFilter = true;
        }

        if (!config.keepFramesOnDevice) {
            if (hasFilter) {
                desc << ",";
            }

            desc << "hwdownload";
            hasFilter = true;

            const char* formatName = softwarePixelFormatName(config.softwareFormat);
            if (formatName) {
                desc << ",format=pix_fmts=" << formatName;
            }
        }

        if (!hasFilter) {
            return "null";
        }

        return desc.str();
    }

    bool HardwareVideoFilterGraphBuilder::supportsHardwareScale(HardwareDeviceType deviceType)
    {
        return scaleFilterName(deviceType) != nullptr;
    }

    const char* HardwareVideoFilterGraphBuilder::scaleFilterName(HardwareDeviceType deviceType)
    {
        switch (deviceType) {
        case HardwareDeviceType::CUDA:
            return "scale_cuda";
        case HardwareDeviceType::QSV:
            return "scale_qsv";
        case HardwareDeviceType::VAAPI:
            return "scale_vaapi";
        default:
            return nullptr;
        }
    }

    const char* HardwareVideoFilterGraphBuilder::softwarePixelFormatName(AVPixelFormat format)
    {
        if (format == AV_PIX_FMT_NONE) {
            return nullptr;
        }

        return av_get_pix_fmt_name(format);
    }

} // namespace media::ffmpeg
