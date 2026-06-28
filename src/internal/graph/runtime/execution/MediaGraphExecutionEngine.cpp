#include "internal/graph/runtime/execution/MediaGraphExecutionEngine.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Status MediaGraphExecutionEngine::prepare(MediaGraph graph,
                                                   MediaGraphExecutionOptions options)
{
    if (m_state == MediaGraphExecutionEngineState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphExecutionEngine prepare failed: engine is running"));
    }

    reset();
    m_options = std::move(options);
    m_runtime.setThreadingPolicy(m_options.threadingPolicy);

    auto compileStatus = m_runtime.compile(std::move(graph));
    if (!compileStatus) {
        m_state = MediaGraphExecutionEngineState::Failed;
        return compileStatus;
    }

    if (m_options.autoRegisterDefaultNodes) {
        auto registerStatus = m_runtime.registerDefaultRuntimeNodes();
        if (!registerStatus) {
            m_state = MediaGraphExecutionEngineState::Failed;
            return registerStatus;
        }
    }

    m_state = MediaGraphExecutionEngineState::Prepared;
    return ::media::Status::success();
}

::media::Status MediaGraphExecutionEngine::start()
{
    if (m_state != MediaGraphExecutionEngineState::Prepared &&
        m_state != MediaGraphExecutionEngineState::Stopped) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphExecutionEngine start failed: engine is not prepared"));
    }

    ::media::Status status = ::media::Status::success();
    if (m_options.mode == MediaGraphExecutionMode::ThreadedRuntime) {
        status = m_runtime.startThreaded();
    } else {
        status = m_runtime.start();
    }

    if (!status) {
        m_state = MediaGraphExecutionEngineState::Failed;
        return status;
    }

    m_state = MediaGraphExecutionEngineState::Running;
    return ::media::Status::success();
}

::media::Status MediaGraphExecutionEngine::processOnce()
{
    if (m_state != MediaGraphExecutionEngineState::Running) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphExecutionEngine processOnce failed: engine is not running"));
    }

    return m_runtime.processOnce();
}

::media::Result<MediaGraphExecutionResult> MediaGraphExecutionEngine::run()
{
    if (m_state != MediaGraphExecutionEngineState::Prepared &&
        m_state != MediaGraphExecutionEngineState::Running) {
        return ::media::Result<MediaGraphExecutionResult>::failure(
            ::media::ErrorInfo::notInitialized("MediaGraphExecutionEngine run failed: engine is not prepared"));
    }

    if (m_options.mode == MediaGraphExecutionMode::Manual) {
        return ::media::Result<MediaGraphExecutionResult>::failure(
            ::media::ErrorInfo::invalidArgument("MediaGraphExecutionEngine run failed: manual mode requires explicit control"));
    }

    MediaGraphExecutionResult result;

    if (m_options.mode == MediaGraphExecutionMode::ThreadedRuntime) {
        if (!m_runtime.running()) {
            auto startStatus = start();
            if (!startStatus) {
                return ::media::Result<MediaGraphExecutionResult>::failure(startStatus.error());
            }
            result.started = true;
        }

        result.report = report();
        m_state = MediaGraphExecutionEngineState::Running;
        return ::media::Result<MediaGraphExecutionResult>::success(std::move(result));
    }

    auto runResult = m_runtime.run();
    if (!runResult) {
        m_state = MediaGraphExecutionEngineState::Failed;
        return ::media::Result<MediaGraphExecutionResult>::failure(runResult.error());
    }

    result.run = runResult.value();
    result.started = true;
    result.stopped = m_options.stopOnCompletion;
    result.report = report();

    m_state = MediaGraphExecutionEngineState::Completed;
    return ::media::Result<MediaGraphExecutionResult>::success(std::move(result));
}

::media::Status MediaGraphExecutionEngine::stop()
{
    auto status = m_runtime.stop();
    if (!status) {
        m_state = MediaGraphExecutionEngineState::Failed;
        return status;
    }

    m_state = MediaGraphExecutionEngineState::Stopped;
    return ::media::Status::success();
}

void MediaGraphExecutionEngine::abort() noexcept
{
    m_runtime.abort();
    m_state = MediaGraphExecutionEngineState::Failed;
}

void MediaGraphExecutionEngine::reset()
{
    if (m_state == MediaGraphExecutionEngineState::Running) {
        abort();
    }

    m_runtime.reset();
    m_state = MediaGraphExecutionEngineState::Empty;
}

MediaGraphRuntime& MediaGraphExecutionEngine::runtime() noexcept
{
    return m_runtime;
}

const MediaGraphRuntime& MediaGraphExecutionEngine::runtime() const noexcept
{
    return m_runtime;
}

MediaGraphRuntimeReport MediaGraphExecutionEngine::report() const
{
    return MediaGraphRuntimeReporter::capture(m_runtime);
}

MediaGraphExecutionEngineState MediaGraphExecutionEngine::state() const noexcept
{
    return m_state;
}

const MediaGraphExecutionOptions& MediaGraphExecutionEngine::options() const noexcept
{
    return m_options;
}

::media::Result<MediaGraphExecutionResult> MediaGraphExecutionEngine::execute(
    MediaGraph graph,
    MediaGraphExecutionOptions options)
{
    MediaGraphExecutionEngine engine;
    auto prepareStatus = engine.prepare(std::move(graph), std::move(options));
    if (!prepareStatus) {
        return ::media::Result<MediaGraphExecutionResult>::failure(prepareStatus.error());
    }

    return engine.run();
}

} // namespace media::ffmpeg::graph
