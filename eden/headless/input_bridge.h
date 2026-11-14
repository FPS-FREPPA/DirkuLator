// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include "ps5_pad.hpp"

namespace Eden::Bridge {
inline constexpr std::uint64_t kMaxPulseMs = 3000;
inline constexpr std::uint64_t kPhysicalFreshMs = 250;
inline constexpr std::uint64_t kNeutralMs = 250;
inline constexpr std::uint32_t kButtons = 0xffff;
inline std::uint64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
enum class Operation { arm, input, stop };
struct Command {
    std::string_view session;
    std::uint64_t sequence{}, duration{};
    Operation operation{};
    std::uint32_t buttons{};
    std::uint8_t lx{128}, ly{128}, rx{128}, ry{128}, l2{}, r2{};
};
inline std::optional<Command> Parse(std::string_view text) {
    if (text.empty() || text.size() > 512) return {};
    std::array<std::string_view, 12> fields;
    std::size_t count = 0;
    while (!text.empty()) {
        const auto start = text.find_first_not_of(" \t\r\n");
        if (start == text.npos) break;
        text.remove_prefix(start);
        const auto end = text.find_first_of(" \t\r\n");
        if (count == fields.size()) return {};
        fields[count++] = text.substr(0, end);
        if (end == text.npos) break;
        text.remove_prefix(end);
    }
    if (count != fields.size() || fields[0] != "v1" || fields[1].empty() || fields[1].size() > 64) return {};
    for (const char c : fields[1])
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return {};
    Command result;
    result.session = fields[1];
    if (fields[3] == "arm") result.operation = Operation::arm;
    else if (fields[3] == "input") result.operation = Operation::input;
    else if (fields[3] == "stop") result.operation = Operation::stop;
    else return {};
    std::array<std::uint64_t, 9> numbers;
    constexpr std::array<unsigned, 9> positions{2, 4, 5, 6, 7, 8, 9, 10, 11};
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        const auto field = fields[positions[i]];
        const auto parsed = std::from_chars(field.data(), field.data() + field.size(), numbers[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != field.data() + field.size()) return {};
    }
    if (!numbers[0] || numbers[1] > kMaxPulseMs || (numbers[2] & ~std::uint64_t{kButtons})) return {};
    for (std::size_t i = 3; i < numbers.size(); ++i) if (numbers[i] > 255) return {};
    if (result.operation == Operation::input) {
        if (!numbers[1]) return {};
    } else if (numbers[1] || numbers[2] || numbers[3] != 128 || numbers[4] != 128 ||
               numbers[5] != 128 || numbers[6] != 128 || numbers[7] || numbers[8]) return {};
    result.sequence = numbers[0]; result.duration = numbers[1]; result.buttons = numbers[2];
    result.lx = numbers[3]; result.ly = numbers[4]; result.rx = numbers[5]; result.ry = numbers[6];
    result.l2 = numbers[7]; result.r2 = numbers[8];
    return result;
}

// Pure state machine: file I/O is confined to the worker below. Physical samples retain
// their motion, connection and timing fields; only explicit remote controls are substituted.
struct State {
    explicit State(std::string id = {}) : session{std::move(id)} {}
    std::string session;
    std::uint64_t sequence{}, applied{}, rejected{}, deadline{}, last_physical{}, neutral_since{};
    bool enabled{}, pulse{}, seen{}, neutral_tracking{}, substituted{};
    const char* status = "disabled";
    const char* error = "none";
    Command controls{};
    ps5::pad::Data physical = ps5::pad::neutral_data();

    static bool Activity(const ps5::pad::Data& sample) {
        const auto away = [](std::uint8_t x) { return x < 96 || x > 160; };
        return sample.buttons != 0 || sample.triggers.l2 > 32 || sample.triggers.r2 > 32 ||
               away(sample.left_stick.x) || away(sample.left_stick.y) ||
               away(sample.right_stick.x) || away(sample.right_stick.y);
    }
    void Cancel(const char* reason) {
        enabled = false; pulse = false; status = reason;
    }
    bool Quiet(std::uint64_t now) const {
        return seen && ps5::pad::is_usable(physical) && now >= last_physical &&
               now - last_physical <= kPhysicalFreshMs && neutral_tracking &&
               now >= neutral_since && now - neutral_since >= kNeutralMs;
    }
    bool Submit(const Command& command, std::uint64_t now) {
        if (command.session != session) {
            rejected = command.sequence; error = "session_mismatch"; Cancel("session_mismatch"); return false;
        }
        if (command.sequence <= sequence) {
            rejected = command.sequence; error = "stale_sequence"; return false;
        }
        if (command.operation != Operation::stop && !Quiet(now)) {
            rejected = command.sequence; error = "physical_not_ready"; Cancel("physical_not_ready"); return false;
        }
        if (command.operation == Operation::input && !enabled) {
            rejected = command.sequence; error = "not_armed"; return false;
        }
        if (command.duration > std::numeric_limits<std::uint64_t>::max() - now) {
            rejected = command.sequence; error = "deadline_overflow"; return false;
        }
        sequence = command.sequence; error = "none";
        if (command.operation == Operation::stop) { Cancel("stopped"); return true; }
        if (command.operation == Operation::arm) {
            enabled = true; pulse = false; status = "armed"; return true;
        }
        controls = command; controls.session = {};
        deadline = now + command.duration; pulse = true; status = "pending";
        return true;
    }
    void Observe(const ps5::pad::Data& sample, std::uint64_t now) {
        physical = sample; seen = true; last_physical = now;
        if (!ps5::pad::is_usable(sample)) {
            neutral_tracking = false;
            if (enabled) Cancel("controller_unavailable");
        } else if (Activity(sample)) {
            neutral_tracking = false;
            if (enabled) Cancel("physical_override");
        } else if (!neutral_tracking) {
            neutral_tracking = true; neutral_since = now;
        }
    }
    void Replace(ps5::pad::Data& sample) const {
        sample.buttons = controls.buttons;
        sample.left_stick = {controls.lx, controls.ly};
        sample.right_stick = {controls.rx, controls.ry};
        sample.triggers.l2 = controls.l2; sample.triggers.r2 = controls.r2;
    }
    std::optional<ps5::pad::Data> Apply(std::span<ps5::pad::Data> samples, std::uint64_t now) {
        // Scan the entire physical batch before changing any sample, so a user's press
        // anywhere in that batch cancels all remote input in the batch.
        for (const auto& sample : samples) Observe(sample, now);
        if (enabled && (!seen || now < last_physical || now - last_physical > kPhysicalFreshMs))
            Cancel("physical_stale");
        if (pulse && now >= deadline) { pulse = false; status = "expired"; }
        if (enabled && pulse) {
            applied = sequence; status = "active"; substituted = true;
            if (samples.empty()) { auto result = physical; Replace(result); return result; }
            for (auto& sample : samples) Replace(sample);
            return {};
        }
        if (substituted) {
            substituted = false;
            if (samples.empty()) return physical;
        }
        return {};
    }
};

class InputBridge {
public:
    ~InputBridge() { Stop(); }
    InputBridge() = default;
    InputBridge(const InputBridge&) = delete;
    InputBridge& operator=(const InputBridge&) = delete;

    bool Start(const std::string& data_dir) {
        Stop();
        const auto wall = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto tick = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        {
            std::lock_guard lock(mutex);
            state = State{std::to_string(wall) + "-" + std::to_string(tick)};
        }
        command_path = data_dir + "/input-bridge-command.txt";
        status_path = data_dir + "/logs/input-bridge-status.json";
        try {
            worker = std::jthread([this](std::stop_token stop) { Run(stop); });
            return true;
        } catch (...) {
            std::lock_guard lock(mutex); state.Cancel("worker_unavailable"); return false;
        }
    }
    void Stop() {
        if (worker.joinable()) { worker.request_stop(); worker.join(); }
    }
    void Unavailable() {
        std::lock_guard lock(mutex);
        state.neutral_tracking = false;
        state.physical = ps5::pad::neutral_data();
        state.Cancel("controller_unavailable");
    }
    void Observe(const ps5::pad::Data& sample) {
        std::lock_guard lock(mutex);
        state.Observe(sample, NowMs());
    }
    std::optional<ps5::pad::Data> Apply(std::span<ps5::pad::Data> samples) {
        std::lock_guard lock(mutex);
        return state.Apply(samples, NowMs());
    }
private:
    void WriteStatus(bool ready, std::uint64_t heartbeat) {
        std::string json;
        {
            std::lock_guard lock(mutex);
            const auto now = NowMs();
            std::ostringstream text;
            text << "{\"version\":1,\"ready\":" << (ready ? "true" : "false")
                 << ",\"session\":\"" << state.session << "\",\"heartbeat\":" << heartbeat
                 << ",\"enabled\":" << (state.enabled ? "true" : "false")
                 << ",\"remote_active\":" << (state.substituted ? "true" : "false")
                 << ",\"sequence\":" << state.sequence << ",\"applied_sequence\":" << state.applied
                 << ",\"rejected_sequence\":" << state.rejected
                 << ",\"state\":\"" << state.status << "\",\"error\":\"" << state.error
                 << "\",\"physical_ready\":" << (state.Quiet(now) ? "true" : "false")
                 << ",\"remaining_ms\":" << (state.pulse && state.deadline > now ? state.deadline - now : 0)
                 << ",\"maximum_pulse_ms\":" << kMaxPulseMs << "}\n";
            json = text.str();
        }
        const auto temporary = status_path + ".tmp";
        std::ofstream file(temporary, std::ios::trunc | std::ios::binary);
        if (!file) return;
        file << json; file.close();
        if (file) std::rename(temporary.c_str(), status_path.c_str());
    }
    void Run(std::stop_token stop) noexcept {
        try {
            std::string previous;
            std::uint64_t heartbeat = 0;
            unsigned unchanged = 0;
            WriteStatus(true, ++heartbeat);
            while (!stop.stop_requested()) {
                std::ifstream file(command_path, std::ios::binary);
                std::array<char, 513> buffer{};
                file.read(buffer.data(), buffer.size());
                std::string content(buffer.data(), static_cast<std::size_t>(file.gcount()));
                bool changed = content != previous;
                if (changed) {
                    previous = content;
                    const auto command = Parse(content);
                    bool accepted = false;
                    {
                        std::lock_guard lock(mutex);
                        if (command) accepted = state.Submit(*command, NowMs());
                        else { state.error = content.empty() ? "command_removed" : "malformed_command";
                               state.Cancel(state.error); }
                    }
                    if (command) std::printf("EDEN_INPUT_BRIDGE sequence=%llu accepted=%d\n",
                        static_cast<unsigned long long>(command->sequence), accepted ? 1 : 0);
                }
                // Ack and heartbeat I/O runs here, never in the controller polling thread.
                if (changed || ++unchanged >= 2) {
                    WriteStatus(true, ++heartbeat); unchanged = 0;
                }
                for (unsigned i = 0; i < 10 && !stop.stop_requested(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            { std::lock_guard lock(mutex); state.Cancel("closed"); }
            WriteStatus(false, ++heartbeat);
        } catch (...) {
            try {
                { std::lock_guard lock(mutex); state.Cancel("bridge_error"); }
                WriteStatus(false, 0);
            } catch (...) {}
        }
    }
    std::mutex mutex;
    State state;
    std::string command_path, status_path;
    std::jthread worker;
};
} // namespace Eden::Bridge
