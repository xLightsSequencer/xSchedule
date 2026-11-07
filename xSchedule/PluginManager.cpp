/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "PluginManager.h"
#include "xSchedulePlugin.h"
#include "xScheduleMain.h"
#include <wx/stdpaths.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/filename.h>
#include "xScheduleApp.h"
#include "ScheduleManager.h"
#include <log.h>
#include <algorithm>
#include "Diagnostics.h"

uint32_t __nextId = 1;

void* GetFunction(wxDynamicLibrary* dl, const std::string& filename, const std::string& function)
{
    void* fn = dl->GetSymbol(function);
    if (fn == nullptr) spdlog::warn("Plugin DLL {} did not contain {}.", filename, function);
    return fn;
}

PluginManager::PluginState::PluginState(wxDynamicLibrary* dl, const std::string& filename) : _filename(filename), _dl(dl), _started(false), _id(__nextId++)
{
    if (_dl) {
        _loadFn = (p_xSchedule_Load)GetFunction(_dl, _filename, "xSchedule_Load");
        _unloadFn = (p_xSchedule_Unload)GetFunction(_dl, _filename, "xSchedule_Unload");
        _startFn = (p_xSchedule_Start)GetFunction(_dl, _filename, "xSchedule_Start");
        _stopFn = (p_xSchedule_Stop)GetFunction(_dl, _filename, "xSchedule_Stop");
        _handleWebFn = (p_xSchedule_HandleWeb)GetFunction(_dl, _filename, "xSchedule_HandleWeb");
        _wipeFn = (p_xSchedule_WipeSettings)GetFunction(_dl, _filename, "xSchedule_WipeSettings");
        _manipulateBufferFn = (p_xSchedule_ManipulateBuffer)GetFunction(_dl, _filename, "xSchedule_ManipulateBuffer");
        _notifyStatusFn = (p_xSchedule_NotifyStatus)GetFunction(_dl, _filename, "xSchedule_NotifyStatus");
        _getVirtualWebFolderFn = (p_xSchedule_GetVirtualWebFolder)GetFunction(_dl, _filename, "xSchedule_GetVirtualWebFolder");
        _getMenuLabelFn = (p_xSchedule_GetMenuLabel)GetFunction(_dl, _filename, "xSchedule_GetMenuLabel");
        _fireEventFn = (p_xSchedule_FireEvent)GetFunction(_dl, _filename, "xSchedule_FireEvent");
        _sendCommandFn = (p_xSchedule_SendCommand)GetFunction(_dl, _filename, "xSchedule_SendCommand");
    } else {
        _loadFn = nullptr;
        _unloadFn = nullptr;
        _startFn = nullptr;
        _stopFn = nullptr;
        _handleWebFn = nullptr;
        _wipeFn = nullptr;
        _manipulateBufferFn = nullptr;
        _notifyStatusFn = nullptr;
        _getVirtualWebFolderFn = nullptr;
        _getMenuLabelFn = nullptr;
        _fireEventFn = nullptr;
        _sendCommandFn = nullptr;
    }
}
PluginManager::PluginState::~PluginState()
{
    if (_dl) {
        spdlog::debug("PluginState destructor unloading dynamic library '{}'.", _filename);
        ///_dl->Detach(); ????
        delete _dl;
        spdlog::debug("PluginState destructor unloaded dynamic library '{}'.", _filename);
    }
}

PluginManager::PluginManager()
{
}

void PluginManager::ScanFolder(const std::string& folder)
{
    spdlog::debug("Scanning for plugins");

#ifdef __WXMSW__
    std::string filespec = "*.dll";
#else
    std::string filespec = "*.so";
#endif
    wxArrayString files;
    wxDir::GetAllFiles(folder, &files, filespec);

    // libraries shipped beside xSchedule are not plugins (loading avfilter causes problems); FFmpeg's
    // names include the library version, so they are matched by prefix
    const std::vector<std::string> ignore = { "hidapi", "libcurl", "libcurl-x64", "libgcc_s_seh-1", "libgcc_s_dw2-1",
                                              "libstdc++-6", "libwinpthread-1", "sdl2" };
    const std::vector<std::string> ignorePrefix = { "avcodec-", "avdevice-", "avfilter-", "avformat-", "avutil-",
                                                    "postproc-", "swresample-", "swscale-" };

    for (auto f : files)
    {
        spdlog::debug("   Examining " + f.ToStdString());

        wxFileName filen(f);
        const std::string name = filen.GetName().Lower().ToStdString();
        if (std::find(ignore.begin(), ignore.end(), name) != ignore.end() ||
            std::any_of(ignorePrefix.begin(), ignorePrefix.end(), [&name](const std::string& p) { return name.rfind(p, 0) == 0; })) {
            spdlog::debug("   Ignored.");
            continue;
        }
        // load the dll
        wxDynamicLibrary* dl = new wxDynamicLibrary();
        dl->Load(f);

        if (dl->IsLoaded()) {
            spdlog::debug("       Loaded.");
            // look for our function
            p_xSchedule_Load fn = (p_xSchedule_Load)dl->GetSymbol("xSchedule_Load");

            // if found add to list
            if (fn != nullptr) {
                PluginState* pis = new PluginState(dl, f);

                wxFileName fn(f);
                _plugins[fn.GetName()] = pis;

                spdlog::debug("       Plugin found {} : {}", fn.GetName().ToStdString(), f.ToStdString());
            } else {
                spdlog::debug("       xSchedule_Load entry point not found.");
                delete dl;
            }
        } else {
            delete dl;
        }
    }
}

bool PluginManager::DoLoad(const std::string& plugin, char* showDir)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;

    p_xSchedule_Load fn = _plugins.at(plugin)->_loadFn;
    if (fn != nullptr) {
        spdlog::debug("Loading plugin {}", plugin);
        bool res = fn(showDir);
        spdlog::debug("Loaded plugin {} -> {}", plugin, res);
        return res;
    }
    return false;
}

void PluginManager::DoUnload(const std::string& plugin)
{
    if (_plugins.find(plugin) == _plugins.end()) return;

    p_xSchedule_Unload fn = _plugins.at(plugin)->_unloadFn;
    if (fn != nullptr) {
        spdlog::debug("Unloading plugin {}", plugin);
        fn();
        spdlog::debug("Unloaded plugin {}", plugin);
    }
}

bool PluginManager::DoStart(const std::string& plugin, char* showDir, char* xScheduleURL)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    if (_plugins.at(plugin)->_started) return true;

    p_xSchedule_Start fn = _plugins.at(plugin)->_startFn;
    if (fn != nullptr) {
        spdlog::debug("Starting plugin {}", plugin);
        _plugins.at(plugin)->_started = fn(showDir, xScheduleURL, Action);
        spdlog::debug("Started plugin {} -> {}", plugin, _plugins.at(plugin)->_started);
    }
    return _plugins.at(plugin)->_started;
}

bool PluginManager::HandleWeb(const std::string& plugin, const std::string& action, const std::wstring& parms, const std::wstring& data, const std::wstring& reference, std::wstring& response)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    if (!_plugins.at(plugin)->_started) return false;

    p_xSchedule_HandleWeb fn = _plugins.at(plugin)->_handleWebFn;
    if (fn != nullptr) {
        wchar_t resp[4096];
        bool res = fn((const char*)action.c_str(), (const wchar_t*)parms.c_str(), (const wchar_t*)data.c_str(), (const wchar_t*)reference.c_str(), resp, sizeof(resp));
        response = std::wstring(resp);
        return res;
    }

    return false;
}

void PluginManager::ManipulateBuffer(uint8_t* buffer, size_t bufferSize)
{
    for (auto it : _plugins) {
        DoManipulateBuffer(it.first, buffer, bufferSize);
    }
}

void PluginManager::NotifyStatus(const std::string& statusJSON)
{
    const char* s = (const char*)statusJSON.c_str();
    for (auto it : _plugins) {
        DoNotifyStatus(it.first, s);
    }
}

bool PluginManager::FirePluginEvent(const std::string& plugin, const std::string& eventType, const std::string& eventParam)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    if (!_plugins.at(plugin)->_started) return false;

    bool res = false;

    p_xSchedule_FireEvent fn = _plugins.at(plugin)->_fireEventFn;
    if (fn != nullptr) {
        res = fn((const char*)eventType.c_str(), (const char*)eventParam.c_str());
        spdlog::debug("Fired event to plugin {} {}:{} -> {}", plugin, eventType, eventParam, res);
    }

    return res;
}

bool PluginManager::FireEvent(const std::string& eventType, const std::string& eventParam)
{
	bool res = false;
    for (auto it : _plugins) {
        res |= FirePluginEvent(it.first, eventType, eventParam);
    }
    return res;
}

std::string PluginManager::GetPluginFromLabel(const std::string& label) const
{
    for (const auto& it : _plugins) {
        if (GetMenuLabel(it.first) == label) {
            return it.first;
        }
    }
    return "";
}

bool PluginManager::SendCommand(const std::string& plugin, const std::string& command, const std::string& parameters, bool* success, std::string* msg)
{
    if (_plugins.find(plugin) == _plugins.end()) {
        *success = false;
        *msg = "Unknown plugin";
        return false;
    }
    if (!_plugins.at(plugin)->_started) {
        *success = false;
        *msg = "Plugin is not started";
        return false;
    }
    p_xSchedule_SendCommand fn = _plugins.at(plugin)->_sendCommandFn;
    if (fn != nullptr) {
        char message[4096];
        memset(message, 0x00, sizeof(message));
        *success = fn(command.c_str(), parameters.c_str(), message, sizeof(message));
        message[sizeof(message) - 1] = 0x00;
        *msg = message;
    }
    return true;
}

bool PluginManager::IsStarted(const std::string& plugin) const
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    return _plugins.at(plugin)->_started;
}

bool PluginManager::DoStop(const std::string& plugin)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    if (!_plugins.at(plugin)->_started) return false;

    p_xSchedule_Stop fn = _plugins.at(plugin)->_stopFn;
    if (fn != nullptr) {
        fn();
        spdlog::debug("Stopped plugin {}", plugin);
        _plugins.at(plugin)->_started = false;
    }
    return true;
}

void PluginManager::DoWipeSettings(const std::string& plugin)
{
    if (_plugins.find(plugin) == _plugins.end()) return;

    p_xSchedule_WipeSettings fn = _plugins.at(plugin)->_wipeFn;
    if (fn != nullptr) {
        fn();
    }
}

bool PluginManager::DoManipulateBuffer(const std::string& plugin, uint8_t* buffer, size_t bufferSize)
{
    if (_plugins.find(plugin) == _plugins.end()) return false;
    if (!_plugins.at(plugin)->_started) return false;

    p_xSchedule_ManipulateBuffer fn = _plugins.at(plugin)->_manipulateBufferFn;
    if (fn != nullptr) {
        fn(buffer, bufferSize);
        return true;
    }
    return false;
}

void PluginManager::DoNotifyStatus(const std::string& plugin, const char* statusJSON)
{
    if (_plugins.find(plugin) == _plugins.end()) return;
    if (!_plugins.at(plugin)->_started) return;

    static bool reenter = false;

    if (reenter) return;
    reenter = true;

    p_xSchedule_NotifyStatus fn = _plugins.at(plugin)->_notifyStatusFn;
    if (fn != nullptr) {
        fn(statusJSON);
    }

    reenter = false;
}

std::string PluginManager::GetVirtualWebFolder(const std::string& plugin) const
{
    if (_plugins.find(plugin) == _plugins.end()) return "";
    if (!_plugins.at(plugin)->_started) return "";

    char buffer[200];
    memset(buffer, 0x00, sizeof(buffer));

    p_xSchedule_GetVirtualWebFolder fn = _plugins.at(plugin)->_getVirtualWebFolderFn;
    if (fn != nullptr) {
        fn(buffer, sizeof(buffer));
    }
    return std::string(buffer);
}

std::string PluginManager::GetMenuLabel(const std::string& plugin) const
{
    if (_plugins.find(plugin) == _plugins.end()) return "";

    char buffer[200];
    memset(buffer, 0x00, sizeof(buffer));

    p_xSchedule_GetMenuLabel fn = _plugins.at(plugin)->_getMenuLabelFn;
    if (fn != nullptr) {
        fn(buffer, sizeof(buffer));
    }
    return std::string(buffer);
}

void PluginManager::RegisterStaticPlugins() {
}


void PluginManager::Initialise(const std::string& showDir)
{
    spdlog::debug("PluginManager::Initialise starting for show dir '{}'.", showDir);

    // scan xSchedule folder first

    wxFileName fi(wxStandardPaths::Get().GetExecutablePath().ToStdString());

    ScanFolder(fi.GetPath());

    // then scan show folder
    ScanFolder(showDir);

    // then add anything that is statically compiled in
    RegisterStaticPlugins();

    for (auto it : _plugins) {
        spdlog::debug("PluginManager::Initialise loading plugin '{}'.", it.first);
        DoLoad(it.first, (char*)showDir.c_str());
        spdlog::debug("PluginManager::Initialise loaded plugin '{}'.", it.first);
    }

    spdlog::debug("PluginManager::Initialise complete. {} plugin(s).", _plugins.size());
}

bool PluginManager::StartPlugin(const std::string& plugin, const std::string& showDir, const std::string& xScheduleURL)
{
    return DoStart(plugin, (char *)showDir.c_str(), (char*)xScheduleURL.c_str());
}

bool PluginManager::StopPlugin(const std::string& plugin)
{
    return DoStop(plugin);
}

uint32_t PluginManager::GetId(const std::string& plugin) const
{
    if (_plugins.find(plugin) == _plugins.end()) return -1;

    return _plugins.at(plugin)->_id;
}

std::string PluginManager::GetPluginFromId(uint32_t id) const
{
    for (auto it : _plugins)
    {
        if (it.second->_id == id)
        {
            return it.first;
        }
    }
    return "";
}

void PluginManager::StopPlugins()
{
    for (auto it : _plugins)
    {
        DoStop(it.first);
    }
}

void PluginManager::WipeSettings()
{
    for (auto it : _plugins) {
        DoWipeSettings(it.first);
    }
}

void PluginManager::Uninitialise()
{
    spdlog::debug("PluginManager::Uninitialise starting. {} plugin(s).", _plugins.size());
    for (auto it : _plugins)
    {
        spdlog::debug("PluginManager::Uninitialise unloading plugin '{}'.", it.first);
        DoUnload(it.first);
        spdlog::debug("PluginManager::Uninitialise unloaded plugin '{}'. Deleting plugin state.", it.first);
        delete it.second;
        spdlog::debug("PluginManager::Uninitialise deleted plugin state for '{}'.", it.first);
    }
    _plugins.clear();
    spdlog::debug("PluginManager::Uninitialise complete.");
}

std::vector<std::string> PluginManager::GetPlugins() const
{
    std::vector<std::string> res;

    for (auto it : _plugins)
    {
        res.push_back(it.first);
    }
    return res;
}

PluginManager::~PluginManager()
{
    Uninitialise();
}

bool Action(const char* command, const wchar_t* parameters, const char* data, char* buffer, size_t bufferSize)
{
    memset(buffer, 0x00, bufferSize);
    std::string c(command);
    std::wstring p(parameters);

    if (c == "Stop plugin") {
        // A plugin closing its own window asks to be stopped. Stopping it shuts down its copy of wxWidgets, which
        // can't happen while its close handler is still running, so it is done once that has returned, and saved
        // like unticking the plugin in the Plugins menu.
        std::string label = wxString(p).ToStdString();
        wxTheApp->CallAfter([label]() {
            ((xScheduleFrame*)wxTheApp->GetTopWindow())->StopPluginAndRemember(label);
        });
        return true;
    }
    std::string d(data);
    wxString dd(data);
    size_t rate = 0;
    wxString msg;
    if (((xScheduleFrame*)wxTheApp->GetTopWindow())->GetScheduleManager()->IsQuery(c))
    {
        wxString ip;
        wxString reference;
        bool result = ((xScheduleFrame*)wxTheApp->GetTopWindow())->GetScheduleManager()->Query(c, p, dd, msg, ip, reference);
        if (result || msg == "")
        {
            msg = dd;
        }
        else
        {
            msg = "{\"result\":\"failed\",\"command\":\"" +
                c + "\",\"message\":\"" +
                msg + "\"}";
        }
        strncpy(buffer, (const char*)msg.c_str(), bufferSize - 1);
        return result;
    }
    else
    {
        Diagnostics::CommandSource source("a plugin");
        bool result = ((xScheduleFrame*)wxTheApp->GetTopWindow())->GetScheduleManager()->Action(c, p, d, nullptr, nullptr, nullptr, rate, msg);
        if (result || msg == "")
        {
            msg = "{\"result\":\"ok\"}";
        }
        else
        {
            msg = "{\"result\":\"failed\",\"command\":\"" +
                c + "\",\"message\":\"" +
                msg + "\"}";
        }
        strncpy(buffer, (const char*)msg.c_str(), bufferSize - 1);
        return result;
    }
}
