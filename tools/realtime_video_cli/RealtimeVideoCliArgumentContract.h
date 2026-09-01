#pragma once

#include "../common/GraphCliSupport.h"

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <initializer_list>
#include <stdexcept>
#include <string>

namespace media::ffmpeg::graph::cli {

inline void rejectPresentRealtimeArguments(
    int argc,
    char** argv,
    std::initializer_list<const char*> arguments,
    const char* route)
{
    for (const char* argument : arguments) {
        if (hasArg(argc, argv, argument)) {
            throw std::invalid_argument(
                std::string(argument) + " is not valid for " + route);
        }
    }
}

inline void rejectIrrelevantRealtimeArgs(
    int argc,
    char** argv,
    RealtimeInputType inputType,
    MediaOutputTransportKind outputTransport)
{
    switch (inputType) {
    case RealtimeInputType::Url:
        rejectPresentRealtimeArguments(
            argc, argv,
            {"--mpegts-max-pcr-gap-ms", "--video-rtp-url",
             "--video-rtp-codec", "--video-rtp-payload-type",
             "--video-rtp-clock-rate", "--video-rtp-fmtp",
             "--audio-rtp-url", "--audio-rtp-codec",
             "--audio-rtp-payload-type", "--audio-rtp-clock-rate",
             "--audio-rtp-channels", "--audio-rtp-fmtp"},
            "URL input");
        break;
    case RealtimeInputType::RtpPort:
        rejectPresentRealtimeArguments(
            argc, argv,
            {"--input", "--rtsp-transport", "--mpegts-max-pcr-gap-ms"},
            "raw RTP input");
        break;
    case RealtimeInputType::MpegTsUdp:
        rejectPresentRealtimeArguments(
            argc, argv,
            {"--rtsp-transport", "--video-rtp-url",
             "--video-rtp-codec", "--video-rtp-payload-type",
             "--video-rtp-clock-rate", "--video-rtp-fmtp",
             "--audio-rtp-url", "--audio-rtp-codec",
             "--audio-rtp-payload-type", "--audio-rtp-clock-rate",
             "--audio-rtp-channels", "--audio-rtp-fmtp"},
            "MPEG-TS/UDP input");
        break;
    }

    switch (outputTransport) {
    case MediaOutputTransportKind::RtpAvp:
        rejectPresentRealtimeArguments(
            argc, argv, {"--output"}, "RTP output");
        break;
    case MediaOutputTransportKind::UdpDatagrams:
        rejectPresentRealtimeArguments(
            argc, argv, {"--rtp-host", "--rtp-port", "--sdp"},
            "UDP output");
        break;
    }
}

inline std::string parseUrlInputRtspTransport(
    int argc,
    char** argv,
    const std::string& inputUrl)
{
    if (isRtspUrl(inputUrl)) {
        return requiredArg(argc, argv, "--rtsp-transport");
    }
    rejectPresentRealtimeArguments(
        argc, argv, {"--rtsp-transport"}, "non-RTSP URL input");
    return {};
}

} // namespace media::ffmpeg::graph::cli
