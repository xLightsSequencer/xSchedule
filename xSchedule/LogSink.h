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

#include <spdlog/sinks/sink.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/details/log_msg_buffer.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// The log file, written on a thread of its own so logging never waits on the disk. Lines logged at critical (the
// crash handler) and flush() are written straight away on the calling thread. A warning or error repeated more than a
// few times a minute is folded into one "repeated n more times" line. The file starts afresh each day and when it reaches
// maxFileSize; older files are kept for keepDays, within maxTotalSize altogether.
class LogSink : public spdlog::sinks::sink {
public:
    LogSink(const std::string& path, size_t maxFileSize, int keepDays, size_t maxTotalSize);
    ~LogSink() override;

    void log(const spdlog::details::log_msg& msg) override;
    void flush() override;
    void set_pattern(const std::string& pattern) override;
    void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override;

    // writes what is queued; lines logged afterwards are written on the calling thread
    void Stop();

    const std::string& GetPath() const { return _path; }

private:
    struct Repeat {
        std::chrono::steady_clock::time_point start;
        int count = 0;
        int folded = 0;
        spdlog::details::log_msg_buffer last;
    };

    void Run();
    void Write();
    void EndRepeat(Repeat& repeat);
    void EndRepeats(bool all);
    void OpenFile();
    void Rotate();
    void Prune();

    const std::string _path;
    const size_t _maxFileSize;
    const int _keepDays;
    const size_t _maxTotalSize;

    std::mutex _pendingLock;
    std::condition_variable _signal;
    std::vector<spdlog::details::log_msg_buffer> _pending;
    size_t _dropped = 0;
    std::unordered_map<size_t, Repeat> _repeats;
    bool _stop = false;
    bool _stopped = false;
    bool _urgent = false;

    std::recursive_timed_mutex _writeLock;
    std::thread::id _writingThread;
    std::unique_ptr<spdlog::formatter> _formatter;
    spdlog::details::file_helper _file;
    bool _fileOpen = false;
    size_t _fileSize = 0;
    std::chrono::system_clock::time_point _nextDay;
    std::chrono::system_clock::time_point _lastWritten = std::chrono::system_clock::now();

    std::thread _thread;
};
