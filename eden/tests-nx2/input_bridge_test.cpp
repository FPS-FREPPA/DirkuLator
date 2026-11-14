// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_bridge.h"
#include <atomic>
#include <cassert>
#include <filesystem>
#include <iostream>
using namespace Eden::Bridge;

Command CommandOf(std::string_view text) {
    auto command = Parse(text); assert(command); return *command;
}
void ObserveQuiet(State& state, std::uint64_t start) {
    auto sample = ps5::pad::neutral_data(); sample.connected = 1;
    state.Observe(sample, start); state.Observe(sample, start + 250);
}
void CoreTests() {
    const std::string good = "v1 s 1 input 100 16384 255 128 128 128 0 0";
    assert(Parse(good));
    for (const auto text : {"", "v2 s 1 arm 0 0 128 128 128 128 0 0",
        "v1 s 1 input 0 0 128 128 128 128 0 0", "v1 s 1 input 3001 0 128 128 128 128 0 0",
        "v1 s 0 input 100 0 128 128 128 128 0 0", "v1 s -1 arm 0 0 128 128 128 128 0 0",
        "v1 s 1 input 100 65536 128 128 128 128 0 0", "v1 s 1 input 100 0 256 128 128 128 0 0",
        "v1 s 1 input 100 0 128 128 128 128 0", "v1 s 1 arm 0 0 128 128 128 128 0 0 junk",
        "v1 s 1 arm 0 1 128 128 128 128 0 0", "v1 s 1 arm 0 0 +128 128 128 128 0 0",
        "v1 s 18446744073709551616 arm 0 0 128 128 128 128 0 0"}) assert(!Parse(text));
    assert(!Parse(std::string(513, 'x')));
    for (std::size_t i = 0; i < good.size(); ++i) {
        auto corrupted = good; corrupted[i] = '\0'; assert(!Parse(corrupted));
    }
    State state("s");
    assert(!state.Submit(CommandOf("v1 s 1 arm 0 0 128 128 128 128 0 0"), 1000));
    ObserveQuiet(state, 1000);
    assert(state.Submit(CommandOf("v1 s 1 arm 0 0 128 128 128 128 0 0"), 1250));
    assert(state.Submit(CommandOf("v1 s 2 input 100 16384 255 128 128 128 17 23"), 1250));
    auto physical = ps5::pad::neutral_data(); physical.connected = 1;
    physical.timestamp_us = 123456; physical.angular_velocity.x = 1.25f;
    physical.orientation.w = 0.75f; physical.acceleration.z = -0.9f;
    auto sample = physical;
    state.Apply({&sample, 1}, 1251);
    assert(sample.buttons == ps5::pad::kButtonCross && sample.left_stick.x == 255);
    assert(sample.triggers.l2 == 17 && sample.triggers.r2 == 23);
    assert(sample.timestamp_us == physical.timestamp_us && sample.angular_velocity.x == 1.25f);
    assert(sample.orientation.w == 0.75f && sample.acceleration.z == -0.9f && sample.connected == 1);
    assert(state.applied == 2);
    assert(!state.Submit(CommandOf("v1 s 2 input 3000 0 0 128 128 128 0 0"), 1290));
    auto release = state.Apply({}, 1350);
    assert(release && release->buttons == 0 && release->left_stick.x == 128);
    assert(!state.Apply({}, 1351));
    assert(state.status == std::string_view("expired"));
    state.Observe(physical, 1400);
    assert(state.Submit(CommandOf("v1 s 3 input 1000 16384 255 128 128 128 0 0"), 1400));
    std::array batch{physical, physical}; batch[1].buttons = ps5::pad::kButtonCircle;
    state.Apply(batch, 1401);
    assert(batch[0].buttons == 0 && batch[0].left_stick.x == 128);
    assert(batch[1].buttons == ps5::pad::kButtonCircle && !state.enabled);
    assert(state.status == std::string_view("physical_override"));
    ObserveQuiet(state, 1500);
    assert(!state.Submit(CommandOf("v1 s 4 input 100 16384 255 128 128 128 0 0"), 1750));
    assert(state.Submit(CommandOf("v1 s 5 arm 0 0 128 128 128 128 0 0"), 1750));
    assert(!state.Submit(CommandOf("v1 old 6 input 100 16384 255 128 128 128 0 0"), 1750));
    assert(!state.enabled);
    assert(state.Submit(CommandOf("v1 s 7 arm 0 0 128 128 128 128 0 0"), 1750));
    assert(state.Submit(CommandOf("v1 s 8 input 3000 16384 255 128 128 128 0 0"), 1750));
    sample = physical; state.Apply({&sample, 1}, 1751);
    release = state.Apply({}, 2002);
    assert(!state.enabled && release && release->buttons == 0);
    ObserveQuiet(state, 2100);
    assert(state.Submit(CommandOf("v1 s 9 arm 0 0 128 128 128 128 0 0"), 2350));
    assert(state.Submit(CommandOf("v1 s 10 input 3000 16384 255 128 128 128 0 0"), 2350));
    sample = physical; state.Apply({&sample, 1}, 2351);
    sample = physical; sample.buttons = ps5::pad::kButtonIntercepted;
    state.Apply({&sample, 1}, 2352);
    assert(!state.enabled && (sample.buttons & ps5::pad::kButtonIntercepted));
    assert(state.Submit(CommandOf("v1 s 11 stop 0 0 128 128 128 128 0 0"), 2353));
    std::cout << "PASS strict parsing, replay/session guards, exact expiry, physical batch override, motion preservation, disconnect/intercept and stop\n";
}

std::string Read(const std::filesystem::path& path) {
    std::ifstream in(path); return {std::istreambuf_iterator<char>(in), {}};
}
template<class Predicate> void Wait(Predicate predicate) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!predicate()) { assert(std::chrono::steady_clock::now() < limit); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
}
void WorkerTest(const std::filesystem::path& path) {
    std::filesystem::create_directories(path / "logs");
    InputBridge bridge;
    assert(bridge.Start(path.string()));
    std::atomic<std::uint32_t> physical_buttons{}, delivered_buttons{};
    std::atomic<unsigned> delivered_x{128};
    std::jthread consumer([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            auto sample = ps5::pad::neutral_data(); sample.connected = 1;
            sample.buttons = physical_buttons.load();
            auto synthetic = bridge.Apply({&sample, 1}); assert(!synthetic);
            delivered_buttons = sample.buttons; delivered_x = sample.left_stick.x;
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    });
    const auto status = path / "logs/input-bridge-status.json";
    Wait([&]{return Read(status).find("\"physical_ready\":true") != std::string::npos;});
    const auto json = Read(status);
    const auto start = json.find("\"session\":\"") + 11;
    const auto session = json.substr(start, json.find('"', start) - start);
    assert(!session.empty());
    auto send = [&](unsigned sequence, std::string_view command) {
        const auto temp = path / "command.tmp";
        { std::ofstream file(temp); file << "v1 " << session << ' ' << sequence << ' ' << command << '\n'; }
        std::filesystem::rename(temp, path / "input-bridge-command.txt");
    };
    send(1, "arm 0 0 128 128 128 128 0 0");
    Wait([&]{return Read(status).find("\"enabled\":true") != std::string::npos;});
    send(2, "input 250 16384 255 128 128 128 0 0");
    Wait([&]{return delivered_buttons == 16384 && delivered_x == 255;});
    Wait([&]{return Read(status).find("\"applied_sequence\":2") != std::string::npos;});
    Wait([&]{return delivered_buttons == 0 && delivered_x == 128;});
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    assert(delivered_buttons == 0 && delivered_x == 128);
    send(3, "input 2000 16384 255 128 128 128 0 0");
    Wait([&]{return delivered_buttons == 16384;});
    physical_buttons = ps5::pad::kButtonCircle;
    Wait([&]{return delivered_buttons == ps5::pad::kButtonCircle;});
    Wait([&]{return Read(status).find("physical_override") != std::string::npos;});
    physical_buttons = 0;
    Wait([&]{return delivered_buttons == 0;});
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    send(4, "input 2000 16384 255 128 128 128 0 0");
    Wait([&]{return Read(status).find("not_armed") != std::string::npos;});
    assert(delivered_buttons == 0);
    consumer.request_stop(); consumer.join(); bridge.Stop();
    assert(Read(status).find("\"ready\":false") != std::string::npos);
    assert(bridge.Start(path.string()));
    Wait([&]{return Read(status).find("session_mismatch") != std::string::npos;});
    assert(Read(status).find("\"enabled\":false") != std::string::npos);
    bridge.Stop();
    std::cout << "PASS real worker/file transport, applied acknowledgment, dead-man release, physical cancellation, re-arm requirement and stale-file restart\n";
}
int main(int argc, char** argv) {
    assert(argc == 2); CoreTests(); WorkerTest(argv[1]);
}
