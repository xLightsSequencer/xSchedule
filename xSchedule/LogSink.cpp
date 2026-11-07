/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "LogSink.h"

#include <spdlog/pattern_formatter.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <functional>
#include <string_view>

namespace {
    constexpr auto REPEAT_WINDOW = std::chrono::seconds(60);
    constexpr int REPEATS_SHOWN = 5;
    constexpr size_t MAX_REPEATS_TRACKED = 1000;
    constexpr size_t MAX_PENDING = 50000;

    std::tm LocalTime(std::chrono::system_clock::time_point t)
    {
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        return tm;
    }

    std::chrono::system_clock::time_point NextMidnight(std::chrono::system_clock::time_point t)
    {
        std::tm tm = LocalTime(t);
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_mday += 1;
        tm.tm_isdst = -1;
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    std::filesystem::path Stem(const std::filesystem::path& path)
    {
        return path.parent_path() / path.stem();
    }

    // rotated files are named after the time of their last line: xschedule_spdlog_2026-10-10_235958.log
    std::filesystem::path RotatedName(const std::filesystem::path& path, std::chrono::system_clock::time_point last)
    {
        std::tm tm = LocalTime(last);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "_%Y-%m-%d_%H%M%S", &tm);
        std::filesystem::path name = Stem(path).string() + stamp + path.extension().string();
        for (int i = 2; std::filesystem::exists(name); ++i) {
            name = Stem(path).string() + stamp + "_" + std::to_string(i) + path.extension().string();
        }
        return name;
    }
}

LogSink::LogSink(const std::string& path, size_t maxFileSize, int keepDays, size_t maxTotalSize) :
    _path(path), _maxFileSize(maxFileSize), _keepDays(keepDays), _maxTotalSize(maxTotalSize),
    _formatter(std::make_unique<spdlog::pattern_formatter>())
{
    std::error_code ec;
    auto written = std::filesystem::last_write_time(_path, ec);
    if (!ec) {
        auto last = std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(written - std::filesystem::file_time_type::clock::now());
        if (NextMidnight(last) <= std::chrono::system_clock::now()) {
            std::filesystem::rename(_path, RotatedName(_path, last), ec);
        }
    }
    OpenFile();
    Prune();
    _thread = std::thread([this]() { Run(); });
}

LogSink::~LogSink()
{
    if (_thread.joinable()) {
        Stop();
    }
    _file.close();
}

void LogSink::OpenFile()
{
    try {
        _file.open(_path, false);
        _fileSize = _file.size();
        _fileOpen = true;
    } catch (...) {
        _fileOpen = false;
        _fileSize = 0;
    }
    _nextDay = NextMidnight(std::chrono::system_clock::now());
}

void LogSink::Rotate()
{
    _file.close();
    _fileOpen = false;
    std::error_code ec;
    std::filesystem::rename(_path, RotatedName(_path, _lastWritten), ec);
    if (ec) {
        // another program has the file open without allowing a rename, so keep a copy and start the file again
        std::filesystem::copy_file(_path, RotatedName(_path, _lastWritten), ec);
        std::filesystem::resize_file(_path, 0, ec);
    }
    OpenFile();
    Prune();
}

void LogSink::Prune()
{
    namespace fs = std::filesystem;
    fs::path path(_path);
    std::string stem = path.stem().string();
    std::string ext = path.extension().string();

    struct Old {
        fs::path path;
        fs::file_time_type written;
        uintmax_t size;
    };
    std::vector<Old> old;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path.parent_path(), ec)) {
        std::string name = entry.path().filename().string();
        // the dated files, and xschedule_spdlog.1.log and .2.log from the size-only rotation older versions used
        bool ours = name.size() > stem.size() + ext.size() && name.compare(0, stem.size(), stem) == 0 &&
                    (name[stem.size()] == '_' || name[stem.size()] == '.') &&
                    name.compare(name.size() - ext.size(), ext.size(), ext) == 0;
        if (!ours || !entry.is_regular_file(ec)) continue;
        old.push_back({ entry.path(), entry.last_write_time(ec), entry.file_size(ec) });
    }
    std::sort(old.begin(), old.end(), [](const Old& a, const Old& b) { return a.written > b.written; });

    auto cutoff = fs::file_time_type::clock::now() - std::chrono::hours(24 * _keepDays);
    uintmax_t total = _fileSize;
    for (const auto& it : old) {
        total += it.size;
        if (it.written < cutoff || total > _maxTotalSize) {
            fs::remove(it.path, ec);
        }
    }
}

void LogSink::log(const spdlog::details::log_msg& msg)
{
    if (!should_log(msg.level)) return;

    bool now = msg.level >= spdlog::level::critical;
    bool urgent = false;
    {
        std::lock_guard<std::mutex> lock(_pendingLock);
        now = now || _stopped;
        // only warnings and errors fold: info lines such as step starts are the show's timeline
        if (!now && msg.level >= spdlog::level::warn) {
            size_t key = std::hash<std::string_view>()(std::string_view(msg.payload.data(), msg.payload.size())) ^
                         (std::hash<std::string_view>()(std::string_view(msg.logger_name.data(), msg.logger_name.size())) * 31) ^
                         (size_t)msg.level;
            auto t = std::chrono::steady_clock::now();
            auto it = _repeats.find(key);
            if (it != _repeats.end() && t - it->second.start < REPEAT_WINDOW) {
                if (++it->second.count > REPEATS_SHOWN) {
                    if (it->second.folded++ == 0) {
                        it->second.last = spdlog::details::log_msg_buffer(msg);
                    }
                    return;
                }
            } else {
                if (it != _repeats.end()) {
                    EndRepeat(it->second);
                    _repeats.erase(it);
                }
                if (_repeats.size() < MAX_REPEATS_TRACKED) {
                    _repeats[key].start = t;
                    _repeats[key].count = 1;
                }
            }
        }
        if (_pending.size() < MAX_PENDING) {
            _pending.emplace_back(msg);
        } else {
            ++_dropped;
        }
        if (msg.level >= spdlog::level::err) {
            _urgent = urgent = true;
        }
    }
    if (now) {
        Write();
    } else if (urgent) {
        _signal.notify_one();
    }
}

// call with _pendingLock held
void LogSink::EndRepeat(Repeat& repeat)
{
    if (repeat.folded == 0) return;
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - repeat.start).count();
    std::string text = "(repeated " + std::to_string(repeat.folded) + " more time" + (repeat.folded == 1 ? "" : "s") +
                       " in the last " + std::to_string(secs) + " s) " + std::string(repeat.last.payload.data(), repeat.last.payload.size());
    spdlog::details::log_msg summary(spdlog::source_loc{}, repeat.last.logger_name, repeat.last.level, text);
    summary.thread_id = repeat.last.thread_id;
    if (_pending.size() < MAX_PENDING) {
        _pending.emplace_back(summary);
    } else {
        ++_dropped;
    }
    repeat.folded = 0;
}

void LogSink::EndRepeats(bool all)
{
    std::lock_guard<std::mutex> lock(_pendingLock);
    auto t = std::chrono::steady_clock::now();
    for (auto it = _repeats.begin(); it != _repeats.end();) {
        if (all || t - it->second.start >= REPEAT_WINDOW) {
            EndRepeat(it->second);
            it = _repeats.erase(it);
        } else {
            ++it;
        }
    }
}

void LogSink::Write()
{
    std::unique_lock<std::recursive_timed_mutex> write(_writeLock, std::defer_lock);
    if (!write.try_lock_for(std::chrono::seconds(2))) return;
    // a crash while this thread was writing: what is half written can't be trusted to carry on
    if (_writingThread == std::this_thread::get_id()) return;
    _writingThread = std::this_thread::get_id();

    std::vector<spdlog::details::log_msg_buffer> batch;
    size_t dropped;
    {
        std::lock_guard<std::mutex> lock(_pendingLock);
        batch.swap(_pending);
        dropped = _dropped;
        _dropped = 0;
    }

    auto put = [this](const spdlog::details::log_msg& msg) {
        if (msg.time >= _nextDay || _fileSize >= _maxFileSize) {
            Rotate();
        }
        if (!_fileOpen) return;
        spdlog::memory_buf_t buf;
        _formatter->format(msg, buf);
        _file.write(buf);
        _fileSize += buf.size();
        _lastWritten = msg.time;
    };
    try {
        if (dropped > 0) {
            std::string text = std::to_string(dropped) + " log lines were dropped: lines came in faster than they could be written.";
            put(spdlog::details::log_msg(spdlog::source_loc{}, "xschedule", spdlog::level::warn, text));
        }
        for (const auto& msg : batch) {
            put(msg);
        }
        if (_fileOpen) {
            _file.flush();
        }
    } catch (...) {
    }
    _writingThread = std::thread::id();
}

void LogSink::Run()
{
    for (;;) {
        bool stop;
        {
            std::unique_lock<std::mutex> lock(_pendingLock);
            _signal.wait_for(lock, std::chrono::milliseconds(500), [this]() { return _stop || _urgent; });
            stop = _stop;
            _urgent = false;
        }
        EndRepeats(false);
        Write();
        if (stop) break;
    }
}

void LogSink::flush()
{
    Write();
}

void LogSink::Stop()
{
    {
        std::lock_guard<std::mutex> lock(_pendingLock);
        _stop = true;
    }
    _signal.notify_one();
    if (_thread.joinable()) {
        _thread.join();
    }
    EndRepeats(true);
    {
        std::lock_guard<std::mutex> lock(_pendingLock);
        _stopped = true;
    }
    Write();
}

void LogSink::set_pattern(const std::string& pattern)
{
    set_formatter(std::make_unique<spdlog::pattern_formatter>(pattern));
}

void LogSink::set_formatter(std::unique_ptr<spdlog::formatter> formatter)
{
    std::lock_guard<std::recursive_timed_mutex> write(_writeLock);
    _formatter = std::move(formatter);
}
