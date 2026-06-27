#include "internal/graph/runtime/MediaGraphWorker.h"

namespace media::ffmpeg::graph {

MediaGraphWorker::MediaGraphWorker(MediaRuntimeNode& node,
                                   MediaGraphExecutionContext& context,
                                   MediaGraphWorkerConfig config)
    : m_node(node)
    , m_context(context)
    , m_config(config)
{
}

MediaGraphWorker::~MediaGraphWorker()
{
    requestStop();
    join();
}

::media::Status MediaGraphWorker::start()
{
    if (m_running) {
        return ::media::Status::success();
    }

    m_stopRequested = false;
    m_aborted = false;
    m_thread = std::thread(&MediaGraphWorker::run, this);
    return ::media::Status::success();
}

void MediaGraphWorker::requestStop() noexcept
{
    m_stopRequested = true;
}

void MediaGraphWorker::abort() noexcept
{
    m_aborted = true;
    m_stopRequested = true;
    m_node.abort(m_context);
}

void MediaGraphWorker::join()
{
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool MediaGraphWorker::running() const noexcept
{
    return m_running;
}

bool MediaGraphWorker::stopRequested() const noexcept
{
    return m_stopRequested;
}

bool MediaGraphWorker::aborted() const noexcept
{
    return m_aborted;
}

MediaNodeId MediaGraphWorker::nodeId() const noexcept
{
    return m_node.nodeId();
}

const MediaGraphWorkerMetrics& MediaGraphWorker::metrics() const noexcept
{
    return m_metrics;
}

void MediaGraphWorker::run()
{
    m_running = true;
    uint32_t consecutiveErrors = 0;

    while (!m_stopRequested && !m_aborted) {
        auto status = m_node.process(m_context);
        ++m_metrics.iterations;

        if (!status) {
            ++m_metrics.errors;
            ++consecutiveErrors;
            if (consecutiveErrors >= m_config.maxConsecutiveErrors) {
                m_aborted = true;
                break;
            }
        } else {
            consecutiveErrors = 0;
        }

        ++m_metrics.idleIterations;
        if (m_config.idleSleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_config.idleSleepMs));
        }
    }

    m_running = false;
}

} // namespace media::ffmpeg::graph
