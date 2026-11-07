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

#include <wx/config.h>

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// xSchedule keeps its settings in JSON files in the same place xLights keeps its own settings.json: xSchedule's in
// settings.json, each plugin's in a file of its own.

inline std::filesystem::path XScheduleSettingsFolder()
{
    std::filesystem::path dir;
#if defined(_WIN32) || defined(__WXMSW__)
    const char* appData = std::getenv("APPDATA");
    dir = std::filesystem::path(appData && *appData ? appData : ".") / "xSchedule";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    dir = std::filesystem::path(home && *home ? home : ".") / "Library" / "Application Support" / "xSchedule";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig) {
        dir = std::filesystem::path(xdgConfig) / "xSchedule";
    } else {
        const char* home = std::getenv("HOME");
        dir = std::filesystem::path(home && *home ? home : ".") / ".config" / "xSchedule";
    }
#endif
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// Copies the settings an older version kept in wxConfig (the registry on Windows) into a JSON object
inline void ImportLegacySettings(const std::string& wxAppName, nlohmann::json& into)
{
    wxConfig cfg(wxAppName);
    wxString key;
    long index = 0;
    for (bool more = cfg.GetFirstEntry(key, index); more; more = cfg.GetNextEntry(key, index)) {
        const std::string k = key.ToStdString();
        switch (cfg.GetEntryType(key)) {
        case wxConfigBase::Type_Boolean:
            into[k] = cfg.ReadBool(key, false);
            break;
        case wxConfigBase::Type_Integer:
            into[k] = (long long)cfg.ReadLong(key, 0);
            break;
        case wxConfigBase::Type_Float:
            into[k] = cfg.ReadDouble(key, 0.0);
            break;
        default:
            into[k] = cfg.Read(key, "").ToStdString();
            break;
        }
    }
}

inline nlohmann::json ReadJSONFile(const std::filesystem::path& file)
{
    std::ifstream in(file);
    if (!in.is_open()) return nlohmann::json();
    auto parsed = nlohmann::json::parse(in, nullptr, false);
    return parsed.is_discarded() ? nlohmann::json() : parsed;
}

// A plugin's settings. On Linux the plugins resolve XLightsSettings to xSchedule's own copy (the app links with
// -rdynamic), so they must not point it at a file of their own; they use this instead.
class PluginSettings {
public:
    // legacyApp: the wxConfig name the settings were kept under before, imported the first time
    PluginSettings(const std::string& name, const std::string& legacyApp) :
        _file(XScheduleSettingsFolder() / (name + ".json")) {
        std::error_code ec;
        if (std::filesystem::exists(_file, ec)) {
            auto data = ReadJSONFile(_file);
            if (data.is_object()) _data = data;
        } else {
            ImportLegacySettings(legacyApp, _data);
            _dirty = true;
            Flush();
        }
    }

    bool HasEntry(const std::string& key) const { return _data.contains(key); }

    std::string Read(const std::string& key, const std::string& def) const {
        if (!_data.contains(key)) return def;
        const auto& v = _data[key];
        if (v.is_string()) return v.get<std::string>();
        if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
        if (v.is_number()) return v.dump();
        return def;
    }
    long ReadLong(const std::string& key, long def) const {
        if (!_data.contains(key)) return def;
        const auto& v = _data[key];
        if (v.is_number()) return (long)v.get<double>();
        if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
        if (v.is_string()) {
            const std::string s = v.get<std::string>();
            char* end = nullptr;
            long res = std::strtol(s.c_str(), &end, 10);
            return end != s.c_str() ? res : def;
        }
        return def;
    }
    bool ReadBool(const std::string& key, bool def) const {
        if (!_data.contains(key)) return def;
        const auto& v = _data[key];
        if (v.is_boolean()) return v.get<bool>();
        if (v.is_number()) return v.get<double>() != 0;
        if (v.is_string()) return v.get<std::string>() == "1" || v.get<std::string>() == "true";
        return def;
    }

    void Write(const std::string& key, const std::string& value) { Set(key, value); }
    void Write(const std::string& key, const char* value) { Set(key, std::string(value != nullptr ? value : "")); }
    void Write(const std::string& key, bool value) { Set(key, value); }
    void Write(const std::string& key, int value) { Set(key, (long long)value); }
    void Write(const std::string& key, long value) { Set(key, (long long)value); }

    void DeleteAll() {
        _data = nlohmann::json::object();
        _dirty = true;
    }

    void Flush() {
        if (!_dirty) return;
        auto tmp = _file;
        tmp += ".tmp";
        {
            std::ofstream out(tmp);
            if (!out.is_open()) return;
            out << _data.dump(2);
        }
        std::error_code ec;
        std::filesystem::rename(tmp, _file, ec);
        if (!ec) _dirty = false;
    }

private:
    template <typename T>
    void Set(const std::string& key, const T& value) {
        _data[key] = value;
        _dirty = true;
    }

    std::filesystem::path _file;
    nlohmann::json _data = nlohmann::json::object();
    bool _dirty = false;
};

// One of xSchedule's own settings, for a plugin. An xSchedule from before settings.json still keeps them in wxConfig.
inline std::string ReadXScheduleSetting(const std::string& key, const std::string& def)
{
    const auto file = XScheduleSettingsFolder() / "settings.json";
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
        auto data = ReadJSONFile(file);
        if (data.is_object() && data.contains("main") && data["main"].is_object() && data["main"].contains(key)) {
            const auto& v = data["main"][key];
            if (v.is_string()) return v.get<std::string>();
            if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
            if (v.is_number()) return v.dump();
        }
        return def;
    }
    wxConfig config("xSchedule");
    wxString value;
    config.Read(wxString(key), &value, wxString(def));
    return value.ToStdString();
}
