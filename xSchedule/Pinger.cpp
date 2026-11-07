/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "Pinger.h"
#include "../xlights/src-core/utils/UtilFunctions.h"
#include "../xlights/src-core/utils/ip_utils.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"
#include "../xlights/src-core/outputs/ControllerEthernet.h"
#include "../xlights/src-core/outputs/ControllerSerial.h"
#include "../xlights/src-core/outputs/OutputManager.h"
#include "events/ListenerManager.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include <log.h>
#include "Diagnostics.h"

class PingThread : public wxThread {
    APinger* _pinger;
    std::atomic<bool> _stop;
    std::atomic<bool> _running;
    std::mutex _lock;
    std::condition_variable _wake;

public:
    PingThread(APinger* pinger) :
        wxThread(wxTHREAD_JOINABLE) {
        _pinger = pinger;
        _stop = false;
        _running = false;
    }

    virtual ~PingThread() {


        if (_running && !_stop) {
            _stop = true;
        }
    }

    void Stop() {
        spdlog::debug("Asking pinging thread {} to stop", (const char*)_pinger->GetName().c_str());
        std::unique_lock<std::mutex> lock(_lock);
        _stop = true;
        _wake.notify_all();
    }

    // if ping fails try this many times before setting the result
#define PINGRETRIES 3

    virtual void* Entry() override {
        _running = true;
        Diagnostics::ThreadName name("ping " + _pinger->GetName());

        if (_pinger->IsInactive()) {
            spdlog::debug("Pinging thread {} is inactive. Skipping", (const char*)_pinger->GetName().c_str());
        } else {
            spdlog::debug("Pinging thread {} started", (const char*)_pinger->GetName().c_str());
            while (!_stop) {
                auto res = Output::PINGSTATE::PING_UNKNOWN;

                // if it previously failed ... only try once
                int loops = PINGRETRIES;
                if (_pinger->GetFailCount() > 0)
                    loops = 1;

                for (int i = 0; !_stop && i < loops; i++) {
                    res = _pinger->Ping();
                    if (res != Output::PINGSTATE::PING_ALLFAILED) {
                        break;
                    }
                }
                _pinger->SetPingResult(res);

                // sleeps the whole interval rather than waking every 10 ms to check; Stop() wakes it
                std::unique_lock<std::mutex> lock(_lock);
                _wake.wait_for(lock, std::chrono::seconds(_pinger->GetPingInterval()), [this]() { return _stop.load(); });
            }
        }
        _running = false;

        return nullptr;
    }
};

APinger::APinger(ListenerManager* lm, Controller* controller) {
    _listenerManager = lm;
    _controller = controller;
    _why = "Output";
    if (dynamic_cast<ControllerEthernet*>(controller) != nullptr) {
        _ip = dynamic_cast<ControllerEthernet*>(controller)->GetIP();
    }

    CheckLocal();

    _active = controller->IsActive();
    _lastResult = Output::PINGSTATE::PING_UNKNOWN;
    _failCount = 0;
    _pingThread = new PingThread(this);
    _pingThread->Create();
    _pingThread->Run();
}

APinger::APinger(ListenerManager* lm, const std::string ip, const std::string why) {
    _listenerManager = lm;
    _controller = nullptr;
    _ip = ip;
    _why = why;
    _lastResult = Output::PINGSTATE::PING_UNKNOWN;

    CheckLocal();

    _failCount = 0;
    _pingThread = new PingThread(this);
    _pingThread->Create();
    _pingThread->Run();
}

APinger::~APinger() {
}

Output::PINGSTATE APinger::GetPingResult() const {
    return _lastResult;
}

bool APinger::GetPingResult(Output::PINGSTATE state) const {
    switch (state) {
    case Output::PINGSTATE::PING_ALLFAILED:
        return false;
    case Output::PINGSTATE::PING_OK:
    case Output::PINGSTATE::PING_WEBOK:
    case Output::PINGSTATE::PING_OPEN:
    case Output::PINGSTATE::PING_OPENED:
    case Output::PINGSTATE::PING_UNKNOWN:
    case Output::PINGSTATE::PING_UNAVAILABLE:
    default:
        return true;
    }
}

std::string APinger::GetPingResultName(Output::PINGSTATE state) {
    switch (state) {
    case Output::PINGSTATE::PING_ALLFAILED:
        return "Failed";
    case Output::PINGSTATE::PING_UNAVAILABLE:
        return "Unavailable";
    case Output::PINGSTATE::PING_OK:
    case Output::PINGSTATE::PING_WEBOK:
    case Output::PINGSTATE::PING_OPEN:
    case Output::PINGSTATE::PING_OPENED:
        return "Ok";
    case Output::PINGSTATE::PING_UNKNOWN:
        return "Unknown";
    }

    return "Unknown";
}

Output::PINGSTATE APinger::Ping() {
    if (_controller != nullptr) {
        return _controller->Ping();
    }

    return IPOutput::Ping(_ip, "");
}

void APinger::CheckLocal() {
    _isLocal = false;

    auto ips = ip_utils::GetLocalIPs();

    for (const auto& it : ips) {
        if (ip_utils::IsInSameSubnet(_ip, it, "255.255.255.0")) {
            _isLocal = true;
            break;
        }
    }
}

void APinger::SetPingResult(Output::PINGSTATE result) {
    _lastResult = result;

    if (result == Output::PINGSTATE::PING_ALLFAILED)
        _failCount++;
    if (result == Output::PINGSTATE::PING_OK || result == Output::PINGSTATE::PING_OPEN || result == Output::PINGSTATE::PING_OPENED || result == Output::PINGSTATE::PING_WEBOK)
        _failCount = 0;

    _listenerManager->ProcessPacket("Ping", GetPingResult(result), _ip);
}

std::string APinger::GetName() const {
    if (_controller != nullptr) {
        return _controller->GetPingDescription();
    }
    return _ip + " " + _why + (_lastResult == Output::PINGSTATE::PING_ALLFAILED && IsLocal() ? _(" (Down)") : _(""));
}

void APinger::AskToStop() {
    if (_pingThread != nullptr) {
        _pingThread->Stop();
    }
}

void APinger::Stop() {
    // tell it to stop ... but it may take a bit of time to stop
    if (_pingThread != nullptr) {
        _pingThread->Stop();
        _pingThread->Delete();
        delete _pingThread;
        _pingThread = nullptr;
    }
    _controller = nullptr;
    _ip = "";
}

std::string GetID(Controller* controller) {
    if (dynamic_cast<ControllerEthernet*>(controller) != nullptr) {
        return dynamic_cast<ControllerEthernet*>(controller)->GetIP();
    } else if (dynamic_cast<ControllerSerial*>(controller) != nullptr) {
        return dynamic_cast<ControllerSerial*>(controller)->GetPort();
    }
    return "";
}

Pinger::Pinger(ListenerManager* listenerManager, OutputManager* outputManager) {
    _listenerManager = listenerManager;
    std::list<std::string> created;

    auto controllers = outputManager->GetControllers();

    for (const auto& it : controllers) {
        if (it->CanPing()) // && it->IsEnabled())
        {
            // check if we have already seen it
            bool found = false;
            for (const auto& cit : created) {
                if (cit == GetID(it)) {
                    // we have seen it
                    found = true;
                    break;
                }
            }

            if (!found) {
                created.push_back(GetID(it));
                _pingers.push_back(new APinger(listenerManager, it));
            }
        }
    }
}

Pinger::~Pinger() {
    _stopping = true;

    // tell them all to stop before waiting for any, so pings still in progress finish together
    for (const auto& it : _pingers) {
        it->AskToStop();
    }
    for (const auto& it : _pingers) {
        it->Stop();
        delete it;
    }
}

void Pinger::AddIP(const std::string ip, const std::string why) {
    if (ip == "")
        return;
    if (ip == "255.255.255.255")
        return;

    for (const auto& it : _pingers) {
        if (it->GetIP() == ip)
            return;
    }

    _pingers.push_back(new APinger(_listenerManager, ip, why));
}

void Pinger::RemoveNonOutputIPs() {
    std::list<APinger*> newPingers;
    for (const auto& it : _pingers) {
        if (!it->IsOutput()) {
            it->Stop();
            delete it;
        } else {
            newPingers.push_back(it);
        }
    }
    _pingers = newPingers;
}

APinger* Pinger::GetPinger(const std::string& ip) const {
    if (_stopping)
        return nullptr;

    for (const auto& it : _pingers) {
        if (it->GetIP() == ip)
            return it;
    }

    return nullptr;
}

APinger* Pinger::GetPingerByIndex(int index) const {
    if (_stopping)
        return nullptr;

    int i = 0;
    for (const auto& it : _pingers) {
        if (i == index)
            return it;
        i++;
    }
    return nullptr;
}
