/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "ShowBackups.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <regex>
#include <tuple>

namespace ShowBackups {

namespace {

namespace fs = std::filesystem;

std::string Escape(const std::string& s)
{
    static const std::regex special(R"([.^$|()\[\]{}*+?\\])");
    return std::regex_replace(s, special, R"(\$&)");
}

int Number(const std::ssub_match& m)
{
    return (int)std::strtol(m.str().c_str(), nullptr, 10);
}

struct Named {
    Backup backup;
    int count = 1;
};

std::vector<Named> Scan(const fs::path& showDir, const std::string& showFile)
{
    std::vector<Named> res;
    const fs::path show(showFile);
    const std::regex pattern("^" + Escape(show.stem().string()) + R"(_(\d{4})-(\d{2})-(\d{2})_(\d{2})(\d{2})(\d{2})(?:_(\d+))?)" +
                             Escape(show.extension().string()) + "$");
    std::error_code ec;
    for (fs::directory_iterator it(Folder(showDir), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string name = it->path().filename().string();
        std::smatch m;
        if (!std::regex_match(name, m, pattern)) continue;
        Named n;
        n.backup.file = it->path();
        n.backup.when.tm_year = Number(m[1]) - 1900;
        n.backup.when.tm_mon = Number(m[2]) - 1;
        n.backup.when.tm_mday = Number(m[3]);
        n.backup.when.tm_hour = Number(m[4]);
        n.backup.when.tm_min = Number(m[5]);
        n.backup.when.tm_sec = Number(m[6]);
        n.backup.size = it->file_size(ec);
        n.count = m[7].matched ? Number(m[7]) : 1;
        res.push_back(n);
    }
    auto key = [](const Named& n) {
        const auto& t = n.backup.when;
        return std::make_tuple(t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, n.count);
    };
    std::sort(res.begin(), res.end(), [&key](const Named& a, const Named& b) { return key(a) > key(b); });
    return res;
}

bool Read(const fs::path& file, std::string& content)
{
    std::ifstream in(file, std::ios::binary);
    if (!in.is_open()) return false;
    content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// writes a temporary file and renames it into place, so a write cut short never leaves a damaged file
bool WriteInto(const std::string& content, const fs::path& to, std::string& error)
{
    std::error_code ec;
    fs::path temp = to;
    temp += ".copying";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << content;
        if (!out) ec = std::make_error_code(std::errc::io_error);
    }
    if (!ec) fs::rename(temp, to, ec);
    if (ec) {
        error = "Could not write " + to.string() + ": " + ec.message();
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

} // namespace

fs::path Folder(const fs::path& showDir)
{
    return showDir / "xScheduleBackups";
}

std::tm Now()
{
    std::time_t t = std::time(nullptr);
    std::tm res{};
#ifdef _WIN32
    localtime_s(&res, &t);
#else
    localtime_r(&t, &res);
#endif
    return res;
}

std::string NameFor(const std::string& showFile, const std::tm& when, int count)
{
    const fs::path show(showFile);
    char stamp[80];
    std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d_%02d%02d%02d", when.tm_year + 1900, when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec);
    std::string name = show.stem().string() + "_" + stamp;
    if (count > 1) name += "_" + std::to_string(count);
    return name + show.extension().string();
}

std::vector<Backup> List(const fs::path& showDir, const std::string& showFile)
{
    std::vector<Backup> res;
    for (const auto& n : Scan(showDir, showFile)) res.push_back(n.backup);
    return res;
}

bool Take(const fs::path& showDir, const std::string& showFile, const std::tm& now, std::string& error)
{
    const fs::path show = showDir / showFile;
    std::error_code ec;
    if (!fs::exists(show, ec)) return true;

    std::string current;
    if (!Read(show, current)) {
        error = "Could not read " + show.string();
        return false;
    }
    auto existing = Scan(showDir, showFile);
    std::string newest;
    if (!existing.empty() && Read(existing.front().backup.file, newest) && newest == current) return true;

    fs::create_directories(Folder(showDir), ec);
    if (ec) {
        error = "Could not make " + Folder(showDir).string() + ": " + ec.message();
        return false;
    }
    int count = 1;
    while (fs::exists(Folder(showDir) / NameFor(showFile, now, count), ec)) ++count;
    if (!WriteInto(current, Folder(showDir) / NameFor(showFile, now, count), error)) return false;

    existing = Scan(showDir, showFile);
    for (size_t i = Keep; i < existing.size(); ++i) fs::remove(existing[i].backup.file, ec);
    return true;
}

bool Restore(const fs::path& showDir, const std::string& showFile, const fs::path& backup, const std::tm& now, std::string& error)
{
    // read first: backing up the show can remove the oldest backup, which may be this one
    std::string content;
    if (!Read(backup, content)) {
        error = "Could not read " + backup.string();
        return false;
    }
    if (!Take(showDir, showFile, now, error)) return false;
    return WriteInto(content, showDir / showFile, error);
}

} // namespace ShowBackups
