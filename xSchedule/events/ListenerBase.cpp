/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "ListenerBase.h"
#include "../ScheduleManager.h"
#include <log.h>
#include <wx/wx.h>
#include "../Diagnostics.h"

ListenerBase::ListenerBase(ListenerManager* listenerManager, const std::string& localIP) {
    _listenerManager = listenerManager;
    _stop = false;
    _thread = nullptr;
    _isOk = false;
    _localIP = localIP;
}

ListenerThread::ListenerThread(ListenerBase* listener, const std::string& localIP) :
    wxThread(wxTHREAD_JOINABLE) {

    _listener = listener;
    _stop = false;
    _running = false;
    _localIP = localIP;
    if (Run() != wxTHREAD_NO_ERROR) {
        spdlog::error("Failed to start listener thread for {}", listener->GetType());
    } else {
        spdlog::info("Listener thread for {} created.", listener->GetType());
    }
}

void* ListenerThread::Entry() {

    if (_listener == nullptr) {
        spdlog::info("Listener thread started but listener was null. Exiting.");
        return nullptr;
    }

    _running = true;
    Diagnostics::ThreadName name("listener " + _listener->GetType());
    spdlog::info("Listener thread for {} running.", _listener->GetType());

    _listener->StartProcess(_localIP);
    if (!_listener->IsOk() && !_listener->PollsWhenNotOk()) {
        spdlog::warn("Listener {} could not start. Trying again every 30 s.", _listener->GetType());
    }

    // A listener that can't start (a network one whose address isn't up yet at boot, a port in use, a device
    // unplugged) tries again every 30 s instead of staying off until xSchedule restarts.
    while (!_stop && !_listener->IsOk() && !_listener->PollsWhenNotOk()) {
        for (int i = 0; i < 300 && !_stop; i++) {
            wxMilliSleep(100);
        }
        if (_stop) break;
        spdlog::debug("Listener {} trying to start again.", _listener->GetType());
        _listener->StopProcess();
        _listener->StartProcess(_localIP);
        if (_listener->IsOk()) {
            spdlog::info("Listener {} started.", _listener->GetType());
        }
    }

    if (_listener->IsOk() || _listener->PollsWhenNotOk()) {
        while (!_stop) {
            _listener->Poll();
        }
    }

    _listener->StopProcess();
    _running = false;

    return nullptr;
}
