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

#include "ListenerBase.h"
#include <mutex>
#include <string>

#include <wx/sckipc.h>
#include <wx/wx.h>

class wxDatagramSocket;

class ListenerMQTT : public ListenerBase {
    std::string _ip = "127.0.0.1";
    std::string _username = "";
    std::string _password = "";
    std::string _clientId = "xSchedule";
    int _port = 1883;
    wxSocketClient _client;
    std::list<std::string> _toSubscribe;
    // every topic, to subscribe again after reconnecting
    std::list<std::string> _subscribed;
    std::mutex _topicLock;
    wxLongLong _nextRetry = 0;
    int _retryDelay = 1000;

    bool SendSubscribe(const std::string& topic);
    void Reconnect();

public:
    ListenerMQTT(ListenerManager* _listenerManager, const std::string& ip, int port, const std::string& username = "", const std::string& password = "", const std::string& clientId = "", const std::string& localIP = "");
    virtual ~ListenerMQTT() {
    }
    virtual void Start() override;
    virtual void Stop() override;
    virtual std::string GetType() const override {
        return "MQTT";
    }
    virtual void StartProcess(const std::string& localIP) override;
    virtual void StopProcess() override;
    virtual void Poll() override;
    virtual bool PollsWhenNotOk() const override {
        return true;
    }
    std::string GetBrokerIP() const {
        return _ip;
    }
    int GetBrokerPort() const {
        return _port;
    }
    bool Subscribe(const std::string& topic);
};
