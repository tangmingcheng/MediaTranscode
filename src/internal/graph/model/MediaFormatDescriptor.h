#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaStreamKind.h"
#include "internal/graph/model/MediaTimeDescriptor.h"

#include <string>

namespace media::ffmpeg::graph {

enum class MediaCodecDomain {
    Unknown,
    Video,
    Audio,
    Subtitle,
    Data
};

enum class MediaCodecOperation {
    Unknown,
    Decode,
    Encode,
    Copy,
    Mux,
    Demux
};

enum class MediaSampleFormatKind {
    Unknown,
    Packed,
    Planar
};

struct MediaCodecDescriptor {
    MediaCodecDomain domain = MediaCodecDomain::Unknown;
    MediaCodecOperation operation = MediaCodecOperation::Unknown;

    std::string codecName;
    std::string codecLongName;
    std::string profile;

    MediaBitrate bitrate = 0;
    int level = 0;

    bool lossless = false;
    bool passthrough = false;
};

struct MediaVideoFormatDescriptor {
    MediaSize size;
    std::string pixelFormat;
    std::string colorSpace;
    std::string colorRange;
    std::string colorPrimaries;
    std::string colorTransfer;

    MediaRational sampleAspectRatio;
    MediaRational frameRate;

    constexpr bool hasKnownSize() const noexcept
    {
        return size.isValid();
    }
};

struct MediaAudioFormatDescriptor {
    MediaSampleRate sampleRate = 0;
    MediaChannelCount channels = 0;
    std::string channelLayout;
    std::string sampleFormat;
    MediaSampleFormatKind sampleFormatKind = MediaSampleFormatKind::Unknown;

    constexpr bool hasKnownLayout() const noexcept
    {
        return sampleRate > 0 && channels > 0;
    }
};

struct MediaFormatDescriptor {
    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    MediaStreamIndex streamIndex = invalidMediaStreamIndex;

    MediaCodecDescriptor codec;
    MediaVideoFormatDescriptor video;
    MediaAudioFormatDescriptor audio;
    MediaTimeDescriptor time;
    MediaHardwareDescriptor hardware;

    bool isInput = false;
    bool isOutput = false;
    bool isRealtime = false;

    constexpr bool hasStreamIndex() const noexcept
    {
        return streamIndex != invalidMediaStreamIndex;
    }
};

} // namespace media::ffmpeg::graph
