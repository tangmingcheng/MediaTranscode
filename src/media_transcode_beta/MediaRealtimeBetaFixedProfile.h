#pragma once

#include "internal/graph/model/MediaHardwareBackendRequest.h"
#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/RealtimeStreamLayout.h"

#include <cstddef>
#include <string>

namespace media::beta {

class MediaRealtimeBetaFixedProfile final {
public:
    struct Product final {
        const char* identity;
        std::size_t startupMaximumVideoUnitBytes;
        int openTimeoutMs;
        int readTimeoutMs;
        int analyzeDurationUs;
        int probeSizeBytes;
        int progressTimeoutMs;
        int firstOutputTimeoutMs;
        int pollIntervalMs;
        bool lowLatency;
        ffmpeg::graph::MediaTranscodeStreamSet streamSet;
        bool disableHardware;
        ffmpeg::graph::MediaHardwareBackendRequest hardwareBackend;
        ffmpeg::graph::RealtimeInputStreamLayout inputLayout;
        ffmpeg::graph::RealtimeOutputStreamLayout outputLayout;
        ffmpeg::graph::MediaOutputTransportKind outputTransport;
    };

    MediaRealtimeBetaFixedProfile() = delete;

    static const Product& current() noexcept;
    static std::string diagnosticSummary();
};

} // namespace media::beta
