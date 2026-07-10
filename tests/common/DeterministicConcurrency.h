#pragma once

#include <condition_variable>
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

    void waitForArrivals(std::size_t expected)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [&] { return m_arrivals >= expected; });
    }

    void open()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
        m_condition.notify_all();
    }

    void waitUntilOpen()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [&] { return m_open; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::size_t m_arrivals = 0;
    bool m_open = false;
};

} // namespace media_transcode::test
