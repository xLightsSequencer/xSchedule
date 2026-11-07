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

#include <filesystem>
#include <list>
#include <string>

// xSchedule's settings are in settings.json (see SettingsFiles.h), read and written through the xLights settings
// store, so GetXLightsConfig() and the shared xLights helpers (IsDarkMode, GetConfigBool, ...) see them.
namespace ScheduleConfig {
    // points the settings store at xSchedule's settings.json, importing the registry (wxConfig) the first time
    void Initialize();
    std::filesystem::path SettingsFile();
    void Wipe();

    bool GetBool(const std::string& key, bool defaultValue);
    void SetBool(const std::string& key, bool value);

    bool IsSuppressDarkMode();
    void SetSuppressDarkMode(bool suppress);

    // xLights moved its settings out of the registry into settings.json in April 2026 and stopped updating the
    // registry. These read xLights' own settings from there, falling back to the registry for older xLights.
    std::string GetXLightsSetting(const std::string& key);
    std::list<std::string> GetXLightsMediaDirs();
    // Show folders can differ in slashes and case and still be the same folder
    bool IsSameFolder(const std::string& a, const std::string& b);
}
