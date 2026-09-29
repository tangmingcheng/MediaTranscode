#pragma once

#include "internal/graph/runtime/storage/MediaImmutableArray.h"

#include <string_view>

namespace media::ffmpeg::graph {

// Length-delimited identity; embedded NUL bytes participate in equality.
// No terminator is allocated and no C-string interface is exposed.
class MediaCanonicalIdentity final {
public:
    static ::media::Result<MediaCanonicalIdentity> copy(std::string_view value)
    {
        auto storage = MediaImmutableArray<char>::copy({value.data(), value.size()});
        if (!storage) return ::media::Result<MediaCanonicalIdentity>::failure(storage.error());
        MediaCanonicalIdentity identity;
        identity.m_storage = std::move(storage).value();
        return ::media::Result<MediaCanonicalIdentity>::success(std::move(identity));
    }

    std::string_view view() const noexcept
    {
        if (m_storage.empty()) return {};
        return {m_storage.view().data(), m_storage.size()};
    }
    bool empty() const noexcept { return m_storage.empty(); }
    std::size_t storageBytes() const noexcept { return m_storage.storageBytes(); }
    friend bool operator==(const MediaCanonicalIdentity& left,
                           const MediaCanonicalIdentity& right) noexcept
    { return left.view() == right.view(); }

private:
    MediaImmutableArray<char> m_storage;
};

} // namespace media::ffmpeg::graph
