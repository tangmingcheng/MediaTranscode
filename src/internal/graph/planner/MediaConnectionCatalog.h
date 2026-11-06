#pragma once
#include <memory_resource>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
// Connection declarations contain values and borrowed static labels. The caller
// owns the resource. Unlike ordinary PMR vector move construction, the allocator-
// extended move can report allocation failure for MSVC debug iterator proxies.
template<class Connection> class MediaConnectionCatalog final {
public:
    explicit MediaConnectionCatalog(std::pmr::memory_resource& resource) : m_values(&resource) {}
    MediaConnectionCatalog(const MediaConnectionCatalog&) = delete;
    MediaConnectionCatalog& operator=(const MediaConnectionCatalog&) = delete;
    MediaConnectionCatalog(MediaConnectionCatalog&& other)
        : m_values(std::move(other.m_values), other.m_values.get_allocator()) {}
    // Assignment retains the destination resource, as required by its owner.
    MediaConnectionCatalog& operator=(MediaConnectionCatalog&& other) {
        if (this != &other) m_values = std::move(other.m_values);
        return *this;
    }
    auto begin() noexcept { return m_values.begin(); }
    auto end() noexcept { return m_values.end(); }
    auto begin() const noexcept { return m_values.begin(); }
    auto end() const noexcept { return m_values.end(); }
    Connection* data() noexcept { return m_values.data(); }
    const Connection* data() const noexcept { return m_values.data(); }
    std::size_t size() const noexcept { return m_values.size(); }
    bool empty() const noexcept { return m_values.empty(); }
    void push_back(const Connection& value) { m_values.push_back(value); }
private:
    std::pmr::vector<Connection> m_values;
};
} // namespace media::ffmpeg::graph
