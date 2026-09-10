#pragma once

#include <condition_variable>
#include <mutex>

namespace media::ffmpeg::graph {

// One preparation worker pauses while the control sequence admits or publishes
// its candidate. Candidate ownership never leaves that worker during rollback.
class MediaRealtimePreparationExchange final {
public:
    enum class Stage { Planning, Planned, Constructing, Constructed, Releasing, Complete };

    Stage stage() const { std::lock_guard lock(m_mutex); return m_stage; }
    bool planned()
    {
        std::unique_lock lock(m_mutex);
        m_stage = Stage::Planned;
        m_changed.wait(lock, [this] { return m_construct || m_release; });
        if (m_release) { m_stage = Stage::Releasing; return false; }
        m_stage = Stage::Constructing;
        return true;
    }
    void constructed()
    {
        std::unique_lock lock(m_mutex);
        m_stage = Stage::Constructed;
        m_changed.wait(lock, [this] { return m_release; });
        m_stage = Stage::Releasing;
    }
    void construct()
    {
        std::lock_guard lock(m_mutex);
        m_construct = true;
        m_stage = Stage::Constructing;
        m_changed.notify_one();
    }
    void release()
    {
        std::lock_guard lock(m_mutex);
        m_release = true;
        if (m_stage == Stage::Planned || m_stage == Stage::Constructed) m_stage = Stage::Releasing;
        m_changed.notify_one();
    }
    void complete() { std::lock_guard lock(m_mutex); m_stage = Stage::Complete; }
private:
    mutable std::mutex m_mutex;
    std::condition_variable m_changed;
    Stage m_stage = Stage::Planning;
    bool m_construct = false;
    bool m_release = false;
};

} // namespace media::ffmpeg::graph
