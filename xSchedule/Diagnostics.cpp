/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "Diagnostics.h"

#include <log.h>
#include <spdlog/pattern_formatter.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <dirent.h>
#include <fstream>
#endif

namespace {
    std::shared_mutex _namesLock;
    std::unordered_map<size_t, std::string> _names;

    class ThreadNameFlag : public spdlog::custom_flag_formatter {
    public:
        void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
        {
            {
                std::shared_lock<std::shared_mutex> lock(_namesLock);
                auto it = _names.find(msg.thread_id);
                if (it != _names.end()) {
                    dest.append(it->second.data(), it->second.data() + it->second.size());
                    return;
                }
            }
            std::string id = std::to_string(msg.thread_id);
            dest.append(id.data(), id.data() + id.size());
        }
        std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
        {
            return std::make_unique<ThreadNameFlag>();
        }
    };

    long long SteadyMS()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // 0 while the frame timer is stopped, or before the first frame
    std::atomic<long long> _beat{ 0 };
    std::atomic<const char*> _what{ nullptr };
    std::mutex _detailLock;
    std::string _detail;
    std::string _playing;

    long long _frames = 0;
    long long _frameTotal = 0;
    long long _frameWorst = 0;
    std::atomic<int> _stalls{ 0 };

    thread_local std::string _commandSource;

    std::thread::id _mainThread;
    std::thread _watchdog;
    std::mutex _watchdogLock;
    std::condition_variable _watchdogSignal;
    bool _watchdogStop = false;

    std::string Describe()
    {
        const char* what = _what.load();
        std::lock_guard<std::mutex> lock(_detailLock);
        std::string res = what == nullptr ? std::string("nothing xSchedule tracks (a dialog, menu or other window event)") : std::string(what);
        if (what != nullptr && !_detail.empty()) {
            res += " (" + _detail + ")";
        }
        res += _playing.empty() ? std::string(", nothing playing") : ", playing " + _playing;
        return res;
    }

    void Watch()
    {
        Diagnostics::ThreadName name("watchdog");
        long long stallFrom = 0;
        long long reported = 0;
        // starting up, the window and the show are still settling
        const long long settled = SteadyMS() + 10000;
        std::string busy;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(_watchdogLock);
                if (_watchdogSignal.wait_for(lock, std::chrono::milliseconds(100), []() { return _watchdogStop; })) {
                    return;
                }
            }
            long long beat = _beat.load();
            long long now = SteadyMS();
            if (stallFrom != 0 && beat != stallFrom) {
                if (beat != 0) {
                    ++_stalls;
                    spdlog::warn("The main thread ran no frames for {} ms. It was busy with {}.", beat - stallFrom, busy);
                }
                stallFrom = 0;
            }
            if (beat == 0 || beat < settled) continue;
            long long gap = now - beat;
            if (stallFrom == 0 && gap >= 500) {
                stallFrom = beat;
                reported = 0;
                busy = Describe();
            }
            if (stallFrom != 0 && gap >= 2000 && now - reported >= 10000) {
                busy = Describe();
                spdlog::warn("The main thread has run no frames for {:.1f} s. It is busy with {}.", gap / 1000.0, busy);
                reported = now;
            }
        }
    }
}

namespace Diagnostics {
    std::unique_ptr<spdlog::formatter> MakeFormatter(const std::string& pattern)
    {
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<ThreadNameFlag>('N').set_pattern(pattern);
        return formatter;
    }

    void NameThread(const std::string& name)
    {
        std::unique_lock<std::shared_mutex> lock(_namesLock);
        _names[spdlog::details::os::thread_id()] = name;
    }

    void ForgetThread()
    {
        std::unique_lock<std::shared_mutex> lock(_namesLock);
        _names.erase(spdlog::details::os::thread_id());
    }

    Activity::Activity(const char* what, const std::string& detail) :
        _active(std::this_thread::get_id() == _mainThread)
    {
        if (!_active) return;
        _outerWhat = _what.load();
        _what = what;
        if (!detail.empty()) {
            std::lock_guard<std::mutex> lock(_detailLock);
            _outerDetail.swap(_detail);
            _detail = detail;
        } else if (!_detail.empty()) {
            std::lock_guard<std::mutex> lock(_detailLock);
            _outerDetail.swap(_detail);
        }
    }

    Activity::~Activity()
    {
        if (!_active) return;
        if (!_outerDetail.empty() || !_detail.empty()) {
            std::lock_guard<std::mutex> lock(_detailLock);
            _detail.swap(_outerDetail);
        }
        _what = _outerWhat;
    }

    void SetPlaying(const std::string& playing)
    {
        std::lock_guard<std::mutex> lock(_detailLock);
        _playing = playing;
    }

    CommandSource::CommandSource(const std::string& source) :
        _outer(_commandSource)
    {
        _commandSource = source;
    }

    CommandSource::~CommandSource()
    {
        _commandSource.swap(_outer);
    }

    std::string GetCommandSource()
    {
        return _commandSource;
    }

    void FrameDone(long long ms)
    {
        _beat = SteadyMS();
        ++_frames;
        _frameTotal += ms;
        if (ms > _frameWorst) _frameWorst = ms;
    }

    void FramesPaused()
    {
        _beat = 0;
    }

    void StartWatchdog()
    {
        if (_watchdog.joinable()) return;
        _mainThread = std::this_thread::get_id();
        _watchdogStop = false;
        _watchdog = std::thread(Watch);
    }

    void StopWatchdog()
    {
        if (!_watchdog.joinable()) return;
        {
            std::lock_guard<std::mutex> lock(_watchdogLock);
            _watchdogStop = true;
        }
        _watchdogSignal.notify_one();
        _watchdog.join();
    }

    std::string TakeFrameStats()
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "frames %lld, average %.1f ms, worst %lld ms, stalls %d", _frames,
                 _frames == 0 ? 0.0 : (double)_frameTotal / _frames, _frameWorst, _stalls.exchange(0));
        _frames = 0;
        _frameTotal = 0;
        _frameWorst = 0;
        return buf;
    }

    std::string ProcessStats()
    {
        char buf[256];
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX mem{};
        GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&mem, sizeof(mem));
        DWORD handles = 0;
        GetProcessHandleCount(GetCurrentProcess(), &handles);
        int threads = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{ sizeof(te) };
            DWORD me = GetCurrentProcessId();
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
                if (te.th32OwnerProcessID == me) ++threads;
            }
            CloseHandle(snap);
        }
        snprintf(buf, sizeof(buf), "memory %llu MB (peak %llu MB, private %llu MB), handles %lu, GDI %lu, USER %lu, threads %d",
                 (unsigned long long)mem.WorkingSetSize >> 20, (unsigned long long)mem.PeakWorkingSetSize >> 20,
                 (unsigned long long)mem.PrivateUsage >> 20, handles,
                 GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS), GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS), threads);
#else
        long rss = 0, peak = 0, threads = 0;
        std::ifstream status("/proc/self/status");
        std::string key;
        while (status >> key) {
            if (key == "VmRSS:") status >> rss;
            else if (key == "VmHWM:") status >> peak;
            else if (key == "Threads:") status >> threads;
            status.ignore(4096, '\n');
        }
        int fds = 0;
        if (DIR* dir = opendir("/proc/self/fd"); dir != nullptr) {
            while (readdir(dir) != nullptr) ++fds;
            closedir(dir);
            fds -= 2;
        }
        snprintf(buf, sizeof(buf), "memory %ld MB (peak %ld MB), open files %d, threads %ld", rss >> 10, peak >> 10, fds, threads);
#endif
        return buf;
    }
}
