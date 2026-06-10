#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace media {

    template <typename T>
    class FrameQueue {
    public:
        explicit FrameQueue(std::size_t maxSize = 64)
            : m_maxSize(maxSize)
        {
        }

        FrameQueue(const FrameQueue&) = delete;
        FrameQueue& operator=(const FrameQueue&) = delete;

        bool push(T item) {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_notFull.wait(lock, [this]() {
                return m_closed || m_queue.size() < m_maxSize;
                });

            if (m_closed) {
                return false;
            }

            m_queue.push(std::move(item));
            lock.unlock();

            m_notEmpty.notify_one();
            return true;
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_notEmpty.wait(lock, [this]() {
                return m_closed || !m_queue.empty();
                });

            if (m_queue.empty()) {
                return std::nullopt;
            }

            T item = std::move(m_queue.front());
            m_queue.pop();

            lock.unlock();
            m_notFull.notify_one();

            return item;
        }

        void close() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_closed = true;
            }

            m_notEmpty.notify_all();
            m_notFull.notify_all();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);

            while (!m_queue.empty()) {
                m_queue.pop();
            }
        }

        std::size_t size() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmpty;
        std::condition_variable m_notFull;

        std::queue<T> m_queue;
        std::size_t m_maxSize = 64;
        bool m_closed = false;
    };

} // namespace media