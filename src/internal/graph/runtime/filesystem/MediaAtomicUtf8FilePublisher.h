#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaAtomicFileReplaceTransaction {
public:
    virtual ~MediaAtomicFileReplaceTransaction() = default;

    virtual ::media::Status writeAll(std::span<const std::uint8_t> bytes) = 0;
    virtual ::media::Status flushAndClose() = 0;
    virtual ::media::Status replaceTarget() = 0;
};

class MediaAtomicFileReplacePort {
public:
    virtual ~MediaAtomicFileReplacePort() = default;

    virtual ::media::Result<std::unique_ptr<MediaAtomicFileReplaceTransaction>> begin(
        std::string_view targetPathUtf8) = 0;
};

class MediaAtomicUtf8FilePublisher final {
public:
    explicit MediaAtomicUtf8FilePublisher(MediaAtomicFileReplacePort& port) noexcept;

    ::media::Status publish(std::string_view targetPathUtf8,
                            std::string_view contentUtf8);

private:
    MediaAtomicFileReplacePort& m_port;
};

} // namespace media::ffmpeg::graph
