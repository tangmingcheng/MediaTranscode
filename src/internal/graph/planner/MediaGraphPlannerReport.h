#pragma once

#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphPlannerSeverity {
    Info,
    Warning,
    Error
};

struct MediaGraphPlannerMessage {
    MediaGraphPlannerSeverity severity = MediaGraphPlannerSeverity::Info;
    std::string stage;
    std::string message;
};

struct MediaGraphPlannerReport {
    std::vector<MediaGraphPlannerMessage> messages;

    void info(std::string stage, std::string message)
    {
        messages.push_back({ MediaGraphPlannerSeverity::Info, std::move(stage), std::move(message) });
    }

    void warning(std::string stage, std::string message)
    {
        messages.push_back({ MediaGraphPlannerSeverity::Warning, std::move(stage), std::move(message) });
    }

    void error(std::string stage, std::string message)
    {
        messages.push_back({ MediaGraphPlannerSeverity::Error, std::move(stage), std::move(message) });
    }

    void append(const MediaGraphPlannerReport& other)
    {
        messages.insert(messages.end(), other.messages.begin(), other.messages.end());
    }

    bool ok() const noexcept
    {
        for (const auto& item : messages) {
            if (item.severity == MediaGraphPlannerSeverity::Error) {
                return false;
            }
        }
        return true;
    }
};

} // namespace media::ffmpeg::graph
