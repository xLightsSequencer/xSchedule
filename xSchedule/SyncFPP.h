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

#include "ScheduleOptions.h"
#include "SyncManager.h"
#include <wx/socket.h>

#include <ctime>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

// Remotes can be host names. Sync packets go out from the frame loop, so names are looked up on another thread: a
// slow or failing DNS lookup held up the show. Packets to a remote are skipped until its address is known.
class HostLookup {
    std::mutex _lock;
    std::map<std::string, std::string> _found;
    std::map<std::string, time_t> _failed;
    std::map<std::string, std::future<std::string>> _pending;

public:
    bool Get(const std::string& host, unsigned short port, wxIPV4address& address);
};

class ListenerManager;

class SyncFPP : public SyncBase {
    virtual void SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const = 0;

    // What this master last told its remotes about one file. Each master keeps its own: several FPP modes can be on
    // at once, and shared state left all but the first without periodic syncs.
    struct Tracked {
        std::string item;
        uint32_t lastSyncMS = 0;
    };
    mutable Tracked _seq;
    mutable Tracked _media;
    mutable uint32_t _lastStepMS = 0;
    mutable uint32_t _lastStepNo = 0xFFFFFFFF;
    void Track(Tracked& tracked, const std::string& item, uint32_t stepMS, uint32_t frameMS, bool restart, bool isSeq) const;

protected:
    static std::vector<uint8_t> MakeSyncPacket(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS);

public:
    static void Ping(bool remote, const std::string& localIP);

    SyncFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm) :
        SyncBase(sm, rm, options, schm) {
    }
    virtual void SendSync(uint32_t frameMS, uint32_t stepLengthMS, uint32_t stepMS, uint32_t playlistMS, const std::string& fseq, const std::string& media, const std::string& step, const std::string& timeItem, uint32_t stepno, int overridetimeSecs) const override;
    virtual void SendStop() const override;
};

class SyncBroadcastFPP : public SyncFPP {
    wxDatagramSocket* _fppBroadcastSocket = nullptr;
    wxIPV4address _remoteAddr;

    virtual void SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const override;

public:
    SyncBroadcastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP);
    SyncBroadcastFPP(SyncBroadcastFPP&& from) noexcept;
    virtual ~SyncBroadcastFPP();
    virtual std::string GetType() const override {
        return "FPPBROADCAST";
    }
};

class SyncUnicastFPP : public SyncFPP {
    wxDatagramSocket* _fppUnicastSocket = nullptr;
    std::list<std::string> _remotes;
    std::shared_ptr<HostLookup> _lookup = std::make_shared<HostLookup>();

    virtual void SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const override;

public:
    SyncUnicastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP);
    SyncUnicastFPP(SyncUnicastFPP&& from) noexcept;
    virtual ~SyncUnicastFPP();
    virtual std::string GetType() const override {
        return "FPPUNICAST";
    }
};

class SyncUnicastCSVFPP : public SyncFPP {
    wxDatagramSocket* _fppUnicastSocket = nullptr;
    std::list<std::string> _remotes;
    std::shared_ptr<HostLookup> _lookup = std::make_shared<HostLookup>();

    void SendUnicastSync(const std::string& ip, const std::string& item, size_t msec, size_t frameMS, int action) const;
    virtual void SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const override;

public:
    SyncUnicastCSVFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP);
    SyncUnicastCSVFPP(SyncUnicastCSVFPP&& from) noexcept;
    virtual ~SyncUnicastCSVFPP();
    virtual std::string GetType() const override {
        return "FPPCSV";
    }
};

class SyncMulticastFPP : public SyncFPP {
    wxDatagramSocket* _fppMulticastSocket = nullptr;
    wxIPV4address _remoteAddr;

    virtual void SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const override;

public:
    SyncMulticastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP);
    SyncMulticastFPP(SyncMulticastFPP&& from) noexcept;
    virtual ~SyncMulticastFPP();
    virtual std::string GetType() const override {
        return "FPPMULTICAST";
    }
};
