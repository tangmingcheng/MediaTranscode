#pragma once

#include "RealtimeCliControlInput.h"
#include "RealtimeVideoOutputOptions.h"
#include "application/realtime/MediaRealtimeVideoRunController.h"

#include <charconv>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include <string>
#include <string_view>
#include <vector>

namespace media::ffmpeg::graph::cli {

inline const char* realtimeOutputStateName(MediaRealtimeOutputState state) noexcept
{
    switch (state) {
    case MediaRealtimeOutputState::Preparing: return "preparing";
    case MediaRealtimeOutputState::WaitingForRandomAccess: return "waiting_for_random_access";
    case MediaRealtimeOutputState::Running: return "running";
    case MediaRealtimeOutputState::Draining: return "draining";
    case MediaRealtimeOutputState::Retired: return "retired";
    case MediaRealtimeOutputState::Failed: return "failed";
    }
    return "unknown";
}

inline void printRealtimeOutput(const MediaRealtimeOutputSnapshot& output)
{
    std::ostringstream line;
    line << "[CLI] output_id=" << output.outputId
        << " state=" << realtimeOutputStateName(output.state)
        << " detail=" << std::quoted(output.detail)
        << " description=" << std::quoted(output.descriptionPath);
    if (output.error) {
        line << " error=" << static_cast<int>(output.error->code)
            << " native=" << output.error->nativeCode
            << " stage=" << static_cast<int>(output.stage)
            << " reason=" << std::quoted(output.error->message);
    }
    mediaGraphDiagnosticLog(true, MediaGraphDiagnosticPhase::RuntimeLifecycle, line.str());
}

// Quotes delimit tokens; backslash only escapes a matching quote. No shell expansion.
inline std::vector<std::string> tokenizeRealtimeCommand(std::string_view line)
{
    std::vector<std::string> arguments;
    std::string token;
    char quote = 0;
    bool started = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (!character) throw std::invalid_argument("control command contains a NUL byte");
        if (character == '\\' && quote && index + 1 < line.size() && line[index + 1] == quote) {
            token.push_back(line[++index]);
        } else if (quote) {
            if (character == quote) quote = 0;
            else token.push_back(character);
        } else if (character == '\'' || character == '"') {
            quote = character;
            started = true;
        } else if (std::isspace(static_cast<unsigned char>(character))) {
            if (started) { arguments.push_back(std::move(token)); token.clear(); started = false; }
        } else { token.push_back(character); started = true; }
    }
    if (quote) throw std::invalid_argument("control command has an unterminated quote");
    if (started) arguments.push_back(std::move(token));
    return arguments;
}

class RealtimeOutputControl final {
public:
    explicit RealtimeOutputControl(MediaRealtimeVideoRunControl& control) : m_control(control)
    {
        try { m_input.emplace(); }
        catch (const std::exception& error) {
            std::cerr << "[CLI] control_unavailable reason=" << std::quoted(error.what()) << '\n';
        }
    }

    void poll()
    {
        if (!m_input) return;
        std::optional<std::string> line;
        try {
            line = m_input->pollLine();
        } catch (const std::exception& error) {
            m_input.reset();
            std::cerr << "[CLI] control_closed media_session_continues=true reason=" << std::quoted(error.what()) << '\n';
            return;
        }
        try {
            if (line) execute(*line);
        } catch (const std::exception& error) {
            std::cerr << "[CLI] control_rejected reason=" << std::quoted(error.what()) << '\n';
        }
        if (m_input->closed()) {
            m_input.reset();
            std::cout << "[CLI] control_eof media_session_continues=true\n";
        }
    }

private:
    void execute(const std::string& line)
    {
        auto arguments = tokenizeRealtimeCommand(line);
        if (arguments.empty()) return;
        const auto& command = arguments.front();
        if (command == "add") {
            std::vector<char*> argv;
            argv.reserve(arguments.size());
            for (auto& argument : arguments) argv.push_back(argument.data());
            const auto argc = static_cast<int>(argv.size());
            const auto allowed = realtimeVideoOutputValueArgs();
            rejectUnknownArgs(argc, argv.data(), allowed, {});
            if (arguments.size() % 2 != 1)
                throw std::invalid_argument("add requires named option/value pairs");
            std::set<std::string_view> supplied;
            for (std::size_t index = 1; index < arguments.size(); index += 2) {
                if (!containsKey(allowed, arguments[index]) || !supplied.insert(arguments[index]).second)
                    throw std::invalid_argument("add contains an unnamed or duplicate option");
            }
            MediaTranscodeParameterSet parameters;
            parseCommonVideoTranscodeOptions(argc, argv.data(), parameters);
            auto request = parseRealtimeVideoOutputOptions(argc, argv.data(), parameters);
            auto admitted = m_control.addOutput(request);
            if (!admitted) {
                std::cerr << "[CLI] add_rejected reason=" << std::quoted(admitted.error().message) << '\n';
                return;
            }
            std::cout << "[CLI] add_admitted output_id=" << admitted.value() << '\n';
            return;
        }
        if (command == "list") {
            if (arguments.size() != 1) throw std::invalid_argument("list accepts no arguments");
            const auto outputs = m_control.outputSnapshots();
            std::cout << "[CLI] output_count=" << outputs.size() << '\n';
            for (const auto& output : outputs) printRealtimeOutput(output);
            return;
        }
        if (command != "remove" && command != "status")
            throw std::invalid_argument("expected add, remove, list, or status");
        if (arguments.size() != 2) throw std::invalid_argument(command + " requires one output ID");
        std::uint64_t outputId{};
        const auto& text = arguments[1];
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), outputId);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !outputId)
            throw std::invalid_argument("output ID must be a positive unsigned integer");
        if (command == "remove") {
            const auto status = m_control.removeOutput(outputId);
            std::cout << "[CLI] remove_" << (status ? "admitted" : "rejected")
                << " output_id=" << outputId;
            if (!status) std::cout << " reason=" << std::quoted(status.error().message);
            std::cout << '\n';
            return;
        }
        for (const auto& output : m_control.outputSnapshots()) {
            if (output.outputId == outputId) { printRealtimeOutput(output); return; }
        }
        throw std::invalid_argument("output ID is not present in this session");
    }

    MediaRealtimeVideoRunControl& m_control;
    std::optional<RealtimeCliControlInput> m_input;
};

} // namespace media::ffmpeg::graph::cli
