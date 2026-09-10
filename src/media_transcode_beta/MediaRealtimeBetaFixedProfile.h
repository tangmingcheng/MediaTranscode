#pragma once

#include "internal/graph/model/MediaOutputTransportKind.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/model/RealtimeStreamLayout.h"

#include <string>

namespace media::beta {

class MediaRealtimeBetaFixedProfile final {
public:
    struct Product final {
        const char* identity;
        int openTimeoutMs;
        int readTimeoutMs;
        int analyzeDurationUs;
        int probeSizeBytes;
        int progressTimeoutMs;
        int firstOutputTimeoutMs;
        int pollIntervalMs;
        ffmpeg::graph::MediaTranscodeStreamSet streamSet;
    };

    MediaRealtimeBetaFixedProfile() = delete;

    static const Product& current() noexcept;
    static std::string diagnosticSummary();
};

} // namespace media::beta
