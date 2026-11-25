/***************************************************************
 * This source file is part of xSchedule
 **************************************************************/

// xSchedule's implementation of the config/log-path functions declared in
// xLights' settings/XLightsConfigAdapter.h.
//
// Shared xLights code (wxUtilities window position helpers, the xlBaseApp crash
// handler, ...) reads and writes settings through GetXLightsConfig(). xLights
// backs that with its own settings.json; xSchedule must not write into xLights'
// settings, so here the adapter is backed by xSchedule's own wxConfig store
// (wxConfigBase::Get()), exactly where these settings lived before.
//
// The log path functions likewise point at xSchedule's own log file so crash
// reports attach the right log.

#include "xScheduleConfig.h"

#include "settings/XLightsConfigAdapter.h"

#include <wx/config.h>
#include <wx/string.h>

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>

#include <log.h>

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------

std::filesystem::path GetSettingsFilePath() {
    // xLights' settings file - see xlights/src-ui-wx/settings/XLightsConfigAdapter.cpp
    std::filesystem::path dir;
#if defined(_WIN32) || defined(__WXMSW__)
    const char* appData = std::getenv("APPDATA");
    dir = std::filesystem::path(appData && *appData ? appData : ".") / "xLights";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    dir = std::filesystem::path(home && *home ? home : ".") / "Library" / "Application Support" / "xLights";
#else
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig && *xdgConfig) {
        dir = std::filesystem::path(xdgConfig) / "xLights";
    } else {
        const char* home = std::getenv("HOME");
        dir = std::filesystem::path(home && *home ? home : ".") / ".config" / "xLights";
    }
#endif
    return dir / "settings.json";
}

std::filesystem::path GetLogFileFolder() {
#if defined(_WIN32) || defined(__WXMSW__)
    const char* appData = std::getenv("APPDATA");
    return std::filesystem::path(appData && *appData ? appData : ".");
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home && *home ? home : ".") / "Library" / "Logs";
#else
    return std::filesystem::path("/tmp");
#endif
}

std::string GetLogFileName() {
    return "xschedule_spdlog.log";
}

std::filesystem::path GetLogFilePath() {
    return GetLogFileFolder() / GetLogFileName();
}

// ---------------------------------------------------------------------------
// Reading xLights' own settings
// ---------------------------------------------------------------------------

std::string ReadXLightsSetting(const std::string& key, const std::string& def) {
    const auto path = GetSettingsFilePath();
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        try {
            std::ifstream in(path);
            auto data = nlohmann::json::parse(in, nullptr, false);
            if (!data.is_discarded() && data.contains("main") && data["main"].is_object()) {
                const auto& sec = data["main"];
                if (sec.contains(key)) {
                    const auto& v = sec[key];
                    if (v.is_string()) return v.get<std::string>();
                    if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
                    if (v.is_number_integer()) return std::to_string(v.get<long long>());
                    if (v.is_number_float()) return std::to_string(v.get<double>());
                }
                return def;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Unable to read xLights settings file {}: {}", path.string(), e.what());
        }
    }

    // older xLights - settings still in wxConfig
    wxConfig xlconfig("xLights");
    wxString val;
    if (xlconfig.Read(wxString(key), &val)) {
        return val.ToStdString();
    }
    return def;
}

// ---------------------------------------------------------------------------
// XLightsConfigAdapter backed by xSchedule's wxConfig
// ---------------------------------------------------------------------------

static XLightsConfigAdapter s_mainConfig("main");
static XLightsConfigAdapter s_todConfig("tod");
static XLightsConfigAdapter s_bookmarksConfig("bookmarks");

XLightsConfigAdapter* GetXLightsConfig() { return &s_mainConfig; }
XLightsConfigAdapter* GetXLightsTODConfig() { return &s_todConfig; }
XLightsConfigAdapter* GetXLightsBookmarksConfig() { return &s_bookmarksConfig; }

void InitializeXLightsConfig() {
    // nothing to do - wxConfig is created by wxWidgets on first use
}

void WipeXLightsConfig() {
    if (auto* c = wxConfigBase::Get()) c->DeleteAll();
}

double GetDefaultSeqDurationSeconds() {
    return DEFAULT_SEQ_DURATION_SECONDS;
}

namespace {
    wxConfigBase* Cfg() {
        return wxConfigBase::Get();
    }
}

// Sections other than "main" are kept in a wxConfig group of the same name.
static wxString Key(const std::string& section, const std::string& key) {
    if (section.empty() || section == "main") return wxString(key);
    return wxString("/" + section + "/" + key);
}

XLightsConfigAdapter::XLightsConfigAdapter(std::string section) :
    _section(std::move(section)) {
}

bool XLightsConfigAdapter::Read(const std::string& key, wxString* val, const wxString& def) const {
    auto* c = Cfg();
    if (c == nullptr) { *val = def; return false; }
    return c->Read(Key(_section, key), val, def);
}

bool XLightsConfigAdapter::Read(const std::string& key, std::string* val, const std::string& def) const {
    wxString v;
    bool const exists = Read(key, &v, wxString(def));
    *val = v.ToStdString();
    return exists;
}

bool XLightsConfigAdapter::Read(const std::string& key, bool* val, bool def) const {
    auto* c = Cfg();
    if (c == nullptr) { *val = def; return false; }
    return c->Read(Key(_section, key), val, def);
}

bool XLightsConfigAdapter::Read(const std::string& key, int* val, int def) const {
    auto* c = Cfg();
    if (c == nullptr) { *val = def; return false; }
    return c->Read(Key(_section, key), val, def);
}

bool XLightsConfigAdapter::Read(const std::string& key, long* val, long def) const {
    auto* c = Cfg();
    if (c == nullptr) { *val = def; return false; }
    return c->Read(Key(_section, key), val, def);
}

bool XLightsConfigAdapter::Read(const std::string& key, double* val, double def) const {
    auto* c = Cfg();
    if (c == nullptr) { *val = def; return false; }
    return c->Read(Key(_section, key), val, def);
}

bool XLightsConfigAdapter::Read(const std::string& key, size_t* val, size_t def) const {
    long v = 0;
    bool const exists = Read(key, &v, (long)def);
    *val = (size_t)v;
    return exists;
}

std::string XLightsConfigAdapter::Read(const std::string& key, const std::string& def) const {
    std::string v;
    Read(key, &v, def);
    return v;
}

long XLightsConfigAdapter::Read(const std::string& key, long def) const {
    return ReadLong(key, def);
}

bool XLightsConfigAdapter::ReadBool(const std::string& key, bool def) const {
    bool v = def;
    Read(key, &v, def);
    return v;
}

long XLightsConfigAdapter::ReadLong(const std::string& key, long def) const {
    long v = def;
    Read(key, &v, def);
    return v;
}

double XLightsConfigAdapter::ReadDouble(const std::string& key, double def) const {
    double v = def;
    Read(key, &v, def);
    return v;
}

void XLightsConfigAdapter::Write(const std::string& key, const std::string& val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), wxString(val));
}

void XLightsConfigAdapter::Write(const std::string& key, const wxString& val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), val);
}

void XLightsConfigAdapter::Write(const std::string& key, bool val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), val);
}

void XLightsConfigAdapter::Write(const std::string& key, int val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), val);
}

void XLightsConfigAdapter::Write(const std::string& key, unsigned int val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), (long)val);
}

void XLightsConfigAdapter::Write(const std::string& key, long val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), val);
}

void XLightsConfigAdapter::Write(const std::string& key, size_t val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), (long)val);
}

void XLightsConfigAdapter::Write(const std::string& key, double val) {
    if (auto* c = Cfg()) c->Write(Key(_section, key), val);
}

void XLightsConfigAdapter::DeleteEntry(const std::string& key) {
    if (auto* c = Cfg()) c->DeleteEntry(Key(_section, key));
}

void XLightsConfigAdapter::DeleteAll() {
    auto* c = Cfg();
    if (c == nullptr) return;
    if (_section.empty() || _section == "main") {
        c->DeleteAll();
    } else {
        c->DeleteGroup("/" + _section);
    }
}

void XLightsConfigAdapter::Flush() {
    if (auto* c = Cfg()) c->Flush();
}

void XLightsConfigAdapter::SetPath(const std::string&) {
}

bool XLightsConfigAdapter::GetFirstEntry(std::string& key, long& index) const {
    _iterKeys.clear();
    auto* c = Cfg();
    if (c != nullptr) {
        wxString const oldPath = c->GetPath();
        if (!(_section.empty() || _section == "main")) c->SetPath("/" + _section);
        wxString k;
        long idx = 0;
        bool more = c->GetFirstEntry(k, idx);
        while (more) {
            _iterKeys.push_back(k.ToStdString());
            more = c->GetNextEntry(k, idx);
        }
        c->SetPath(oldPath);
    }
    index = 0;
    return GetNextEntry(key, index);
}

bool XLightsConfigAdapter::GetNextEntry(std::string& key, long& index) const {
    if (index < 0 || (size_t)index >= _iterKeys.size()) return false;
    key = _iterKeys[index++];
    return true;
}

XLightsConfigAdapter::EntryType XLightsConfigAdapter::GetEntryType(const std::string& key) const {
    auto* c = Cfg();
    if (c == nullptr) return EntryType::Unknown;
    switch (c->GetEntryType(Key(_section, key))) {
    case wxConfigBase::Type_String:
        return EntryType::String;
    case wxConfigBase::Type_Boolean:
        return EntryType::Boolean;
    case wxConfigBase::Type_Integer:
        return EntryType::Integer;
    case wxConfigBase::Type_Float:
        return EntryType::Float;
    default:
        return EntryType::Unknown;
    }
}

std::string XLightsConfigAdapter::GetAppName() const {
    auto* c = Cfg();
    return c != nullptr ? c->GetAppName().ToStdString() : "xSchedule";
}

std::string XLightsConfigAdapter::GetPath() const {
    return "/" + _section;
}

int XLightsConfigAdapter::GetNumberOfEntries(bool recurse) const {
    auto* c = Cfg();
    return c != nullptr ? (int)c->GetNumberOfEntries(recurse) : 0;
}

int XLightsConfigAdapter::GetNumberOfGroups(bool recurse) const {
    auto* c = Cfg();
    return c != nullptr ? (int)c->GetNumberOfGroups(recurse) : 0;
}

long XLightsConfigAdapter::GetStyle() const {
    auto* c = Cfg();
    return c != nullptr ? c->GetStyle() : 0;
}

std::string XLightsConfigAdapter::GetVendorName() const {
    auto* c = Cfg();
    return c != nullptr ? c->GetVendorName().ToStdString() : "";
}
