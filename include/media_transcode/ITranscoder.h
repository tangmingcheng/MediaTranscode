#pragma once
#include "MediaTranscodeTypes.h"
#include <memory>

namespace media {

    class ITranscoder {
    public:
        virtual ~ITranscoder() = default;

        virtual bool initialize(const TranscodeConfig& config) = 0;
        virtual bool start() = 0;
        virtual void stop() = 0;
        virtual bool pushFrame(void* frame) = 0; // AVFrame* 或平台帧
        virtual void setProgressCallback(ProgressCallback cb) = 0;
    };

    using ITranscoderPtr = std::shared_ptr<ITranscoder>;

} // namespace media