#pragma once

#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaGraphOptimizationSeverity {
    Info,
    Warning,
    Error
};

struct MediaGraphOptimizationMessage {
    MediaGraphOptimizationSeverity severity = MediaGraphOptimizationSeverity::Info;
    std::string passName;
    std::string message;
};

struct MediaGraphOptimizationReport {
    std::vector<MediaGraphOptimizationMessage> messages;

    void info(std::string passName, std::string message)
    {
        messages.push_back({ MediaGraphOptimizationSeverity::Info, std::move(passName), std::move(message) });
    }

    void warning(std::string passName, std::string message)
    {
        messages.push_back({ MediaGraphOptimizationSeverity::Warning, std::move(passName), std::move(message) });
    }

    void error(std::string passName, std::string message)
    {
        messages.push_back({ MediaGraphOptimizationSeverity::Error, std::move(passName), std::move(message) });
    }

    bool ok() const noexcept
    {
        for (const auto& item : messages) {
            if (item.severity == MediaGraphOptimizationSeverity::Error) {
                return false;
            }
        }
        return true;
    }
};

} // namespace media::ffmpeg::graph
