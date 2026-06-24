#include "local/FFmpegLocalFileTranscodeEngine.h"

#include <utility>

namespace media {

FFmpegLocalFileTranscodeEngine::FFmpegLocalFileTranscodeEngine() = default;

FFmpegLocalFileTranscodeEngine::~FFmpegLocalFileTranscodeEngine() = default;

bool FFmpegLocalFileTranscodeEngine::initialize(const TranscodeConfig& config)
{
    return m_delegate.initialize(config);
}

bool FFmpegLocalFileTranscodeEngine::start()
{
    return m_delegate.start();
}

void FFmpegLocalFileTranscodeEngine::stop()
{
    m_delegate.stop();
}

bool FFmpegLocalFileTranscodeEngine::wait()
{
    return m_delegate.wait();
}

bool FFmpegLocalFileTranscodeEngine::isRunning() const
{
    return m_delegate.isRunning();
}

std::string FFmpegLocalFileTranscodeEngine::lastError() const
{
    return m_delegate.lastError();
}

void FFmpegLocalFileTranscodeEngine::setProgressCallback(ProgressCallback cb)
{
    m_delegate.setProgressCallback(std::move(cb));
}

} // namespace media
