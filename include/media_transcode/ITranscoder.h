#pragma once

#include "MediaTranscodeTypes.h"

#include <memory>
#include <string>

namespace media {

/**
 * @brief Legacy internal engine interface used by the existing FFmpegTranscoder.
 *
 * This interface is not part of the recommended public library API. Third-party
 * applications should include media_transcode/MediaTranscode.h and call
 * transcodeLocalFile(). It is kept only to avoid rewriting the already working
 * FFmpegTranscoder implementation during the public API cleanup.
 */
class ITranscoder {
public:
    virtual ~ITranscoder() = default;

    virtual bool initialize(const TranscodeConfig& config) = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;

    // Reserved for future realtime frame input. The current local-file encoder
    // does not support external frame push.
    virtual bool pushFrame(void* frame) = 0;

    virtual void setProgressCallback(ProgressCallback cb) = 0;

    virtual bool wait()
    {
        return !isRunning();
    }

    virtual bool isRunning() const
    {
        return false;
    }

    virtual std::string lastError() const
    {
        return {};
    }
};

using ITranscoderPtr = std::shared_ptr<ITranscoder>;

} // namespace media
