#pragma once

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <mutex>

namespace media_transcode::test {

class DeterministicGate final {
public:
    void arrive()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_arrivals;
        m_condition.notify_all();
    }

    bool waitForArrivals(std::size_t expected,
                         std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_condition.wait_for(lock, timeout, [&] { return m_arrivals >= expected; });
    }

    void open()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
        m_condition.notify_all();
    }

    bool waitUntilOpen(std::chrono::milliseconds timeout = std::chrono::seconds(5))
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_condition.wait_for(lock, timeout, [&] { return m_open; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::size_t m_arrivals = 0;
    bool m_open = false;
};

} // namespace media_transcode::test
