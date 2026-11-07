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

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

// Copies of the show file as it was before each save, in xScheduleBackups in the show folder (xLights uses Backup).
namespace ShowBackups {

constexpr size_t Keep = 50;

struct Backup {
    std::filesystem::path file;
    std::tm when{};
    uintmax_t size = 0;
};

std::filesystem::path Folder(const std::filesystem::path& showDir);
std::tm Now();
// xlights_2026-10-16_143012.xschedule, with _2, _3 ... for more than one in a second
std::string NameFor(const std::string& showFile, const std::tm& when, int count = 1);
// newest first
std::vector<Backup> List(const std::filesystem::path& showDir, const std::string& showFile);
// keeps a copy of the show file as it is; nothing when there is no show file or the newest backup is the same. Only
// the newest Keep backups are kept.
bool Take(const std::filesystem::path& showDir, const std::string& showFile, const std::tm& now, std::string& error);
// puts a backup in place of the show file, backing up the show file first
bool Restore(const std::filesystem::path& showDir, const std::string& showFile, const std::filesystem::path& backup, const std::tm& now, std::string& error);

} // namespace ShowBackups
