#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

class MediaTsPsiPlanIdentity final {
public:
    bool matches(const MediaTsMuxPlan& plan) const noexcept;

private:
    friend class MediaTsPsiSerializer;
    explicit MediaTsPsiPlanIdentity(
        const MediaTsMuxPlanParameters& parameters) noexcept;

    std::uint16_t m_patPid;
    std::uint16_t m_transportStreamId;
    std::uint16_t m_programNumber;
    std::uint16_t m_programMapPid;
    std::uint8_t m_tableVersion;
    MediaTsProgramPlan m_program;
};

class MediaTsPatSection final {
public:
    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    bool matches(const MediaTsMuxPlan& plan) const noexcept
    {
        return m_identity.matches(plan);
    }

private:
    friend class MediaTsPsiSerializer;
    MediaTsPatSection(MediaTsPsiPlanIdentity identity,
                      std::vector<std::uint8_t> bytes) noexcept
        : m_identity(std::move(identity)), m_bytes(std::move(bytes)) {}
    MediaTsPsiPlanIdentity m_identity;
    std::vector<std::uint8_t> m_bytes;
};

class MediaTsPmtSection final {
public:
    std::span<const std::uint8_t> bytes() const noexcept { return m_bytes; }
    bool matches(const MediaTsMuxPlan& plan) const noexcept
    {
        return m_identity.matches(plan);
    }

private:
    friend class MediaTsPsiSerializer;
    MediaTsPmtSection(MediaTsPsiPlanIdentity identity,
                      std::vector<std::uint8_t> bytes) noexcept
        : m_identity(std::move(identity)), m_bytes(std::move(bytes)) {}
    MediaTsPsiPlanIdentity m_identity;
    std::vector<std::uint8_t> m_bytes;
};

class MediaTsProgramTables final {
public:
    const MediaTsPatSection& pat() const noexcept { return m_pat; }
    const MediaTsPmtSection& pmt() const noexcept { return m_pmt; }

private:
    friend class MediaTsPsiSerializer;
    MediaTsProgramTables(MediaTsPatSection pat, MediaTsPmtSection pmt) noexcept
        : m_pat(std::move(pat)), m_pmt(std::move(pmt)) {}
    MediaTsPatSection m_pat;
    MediaTsPmtSection m_pmt;
};

class MediaTsPsiSerializer final {
public:
    static ::media::Result<MediaTsProgramTables> serialize(
        const MediaTsMuxPlan& plan);
};

} // namespace media::ffmpeg::graph
