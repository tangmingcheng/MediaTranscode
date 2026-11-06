#pragma once
#include <memory_resource>
#include <initializer_list>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
// Planning declarations contain values and borrowed static labels. The caller
// owns the resource. Unlike ordinary PMR vector move construction, the allocator-
// extended move can report allocation failure for MSVC debug iterator proxies.
template<class Entry> class MediaPlanningCatalog final {
public:
    explicit MediaPlanningCatalog(std::pmr::memory_resource& resource) : m_values(&resource) {}
    MediaPlanningCatalog(const MediaPlanningCatalog&) = delete;
    MediaPlanningCatalog& operator=(const MediaPlanningCatalog&) = delete;
    MediaPlanningCatalog(MediaPlanningCatalog&& other)
        : m_values(std::move(other.m_values), other.m_values.get_allocator()) {}
    // Assignment retains the destination resource, as required by its owner.
    MediaPlanningCatalog& operator=(MediaPlanningCatalog&& other) {
        if (this != &other) m_values = std::move(other.m_values);
        return *this;
    }
    auto begin() noexcept { return m_values.begin(); }
    auto end() noexcept { return m_values.end(); }
    auto begin() const noexcept { return m_values.begin(); }
    auto end() const noexcept { return m_values.end(); }
    Entry* data() noexcept { return m_values.data(); }
    const Entry* data() const noexcept { return m_values.data(); }
    std::size_t size() const noexcept { return m_values.size(); }
    bool empty() const noexcept { return m_values.empty(); }
    void push_back(const Entry& value) { m_values.push_back(value); }
    void assign(std::initializer_list<Entry> values) { m_values.assign(values); }
    auto insert(typename std::pmr::vector<Entry>::const_iterator position, const Entry& value)
    { return m_values.insert(position, value); }
private:
    std::pmr::vector<Entry> m_values;
};
} // namespace media::ffmpeg::graph
