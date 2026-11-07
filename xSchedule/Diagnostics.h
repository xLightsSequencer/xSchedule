#pragma once

/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <memory>
#include <string>

namespace spdlog {
    class formatter;
}

// What the log needs to explain a problem after the fact: thread names on each line, a watchdog that reports when the
// main thread stops running frames and what it was busy with, and the numbers for the periodic health line.
namespace Diagnostics {
    // a pattern formatter that also understands %N, the name of the thread that logged the line
    std::unique_ptr<spdlog::formatter> MakeFormatter(const std::string& pattern);

    void NameThread(const std::string& name);
    void ForgetThread();

    // names the calling thread for as long as it is in scope
    class ThreadName {
    public:
        explicit ThreadName(const std::string& name) { NameThread(name); }
        ~ThreadName() { ForgetThread(); }
        ThreadName(const ThreadName&) = delete;
        ThreadName& operator=(const ThreadName&) = delete;
    };

    // What the main thread is doing, reported if it stalls; does nothing on other threads. Nests, putting back the
    // outer one when it goes out of scope. what must be a string literal.
    class Activity {
    public:
        explicit Activity(const char* what, const std::string& detail = "");
        ~Activity();
        Activity(const Activity&) = delete;
        Activity& operator=(const Activity&) = delete;

    private:
        bool _active;
        const char* _outerWhat = nullptr;
        std::string _outerDetail;
    };

    void SetPlaying(const std::string& playing);

    // who asked for the commands run on this thread while it is in scope: "the web interface", "event MQTT:Doorbell"
    class CommandSource {
    public:
        explicit CommandSource(const std::string& source);
        ~CommandSource();
        CommandSource(const CommandSource&) = delete;
        CommandSource& operator=(const CommandSource&) = delete;

    private:
        std::string _outer;
    };
    std::string GetCommandSource();

    // the main thread finished a frame that took ms
    void FrameDone(long long ms);
    // the frame timer is stopped on purpose, so no frames is not a stall
    void FramesPaused();

    void StartWatchdog();
    void StopWatchdog();

    // "frames 12000, average 3.1 ms, worst 48 ms, stalls 0", counted since the last call
    std::string TakeFrameStats();
    // memory, handles and threads of this process
    std::string ProcessStats();
}
