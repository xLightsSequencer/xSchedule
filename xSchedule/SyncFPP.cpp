/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "SyncFPP.h"
#include "ScheduleOptions.h"
#include "events/ListenerManager.h"

#include <log.h>
#include "../xlights/src-core/utils/UtilFunctions.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"
#include <wx/filename.h>
#include "Control.h"
#include "../xlights/src-core/outputs/IPOutput.h"
#include "../xlights/src-core/utils/ip_utils.h"
#include "xScheduleVersion.h"
#include "Diagnostics.h"

#define FPP_MEDIA_SYNC_INTERVAL_MS 500
#define FPP_SEQ_SYNC_INTERVAL_FRAMES 16
#define FPP_SEQ_SYNC_INTERVAL_INITIAL_FRAMES 4
#define FPP_SEQ_SYNC_INITIAL_NUMBER_OF_FRAMES 32

void SyncFPP::Ping(bool remote, const std::string& localIP)
{
    wxIPV4address remoteAddr;
    remoteAddr.Hostname("255.255.255.255");
    remoteAddr.Service(FPP_CTRL_PORT);

    wxIPV4address localaddr;
    if (localIP == "")
    {
        // this runs on the UI thread every minute, and looking up our own name can wait on DNS
        static wxString ownAddress;
        static time_t lookedUp = 0;
        if (ownAddress.empty() || time(nullptr) - lookedUp > 600) {
            wxIPV4address own;
            own.Hostname(wxGetFullHostName());
            ownAddress = own.IPAddress();
            lookedUp = time(nullptr);
        }
        localaddr.Hostname(ownAddress);
    }
    else
    {
        localaddr.Hostname(localIP);
    }
    wxString ipAddr = localaddr.IPAddress();

    wxDatagramSocket* fppBroadcastSocket = new wxDatagramSocket(localaddr, wxSOCKET_NOWAIT | wxSOCKET_BROADCAST);
    if (fppBroadcastSocket == nullptr)
    {
        spdlog::error("Error opening datagram for FPP ping. {}", localaddr.IPAddress().ToStdString());
        return;
    }
    else if (!fppBroadcastSocket->IsOk())
    {
        spdlog::error("Error opening datagram for FPP ping. {} OK : FALSE", localaddr.IPAddress().ToStdString());
        delete fppBroadcastSocket;
        return;
    }
    else if (fppBroadcastSocket->Error())
    {
        spdlog::error("Error opening datagram for FPP ping. {} : {}", (int)fppBroadcastSocket->LastError(), DecodeIPError(fppBroadcastSocket->LastError()));
        delete fppBroadcastSocket;
        return;
    }

    int bufsize = sizeof(ControlPkt) + 294;
    std::vector<uint8_t> buffer(bufsize);

    ControlPkt* cp = reinterpret_cast<ControlPkt*>(&buffer[0]);
    memcpy(cp->fppd, "FPPD", 4);
    cp->pktType = CTRL_PKT_PING;
    cp->extraDataLen = 294; // v3 ping length

    uint8_t* ed = (uint8_t*)(&buffer[7]);
    memset(ed, 0, cp->extraDataLen - 7);

    auto v = wxSplit(xschedule_version_string, '.');
    int majorVersion = wxAtoi(v[0]);
    int minorVersion = wxAtoi(v[1]);

    ed[0] = 3; // ping version 3
    ed[1] = 0; // 0 = ping, 1 = discover
    ed[2] = 0xC1;
    ed[3] = (majorVersion & 0xFF00) >> 8;
    ed[4] = (majorVersion & 0x00FF);
    ed[5] = (minorVersion & 0xFF00) >> 8;
    ed[6] = (minorVersion & 0x00FF);
    ed[7] = remote ? 0x08 : 0x06;

    wxArrayString ip = wxSplit(ipAddr, '.');
    ed[8] = wxAtoi(ip[0]);
    ed[9] = wxAtoi(ip[1]);
    ed[10] = wxAtoi(ip[2]);
    ed[11] = wxAtoi(ip[3]);

    strncpy((char*)(ed + 12), wxGetHostName().c_str(), 65);
    strncpy((char*)(ed + 77), xschedule_version_string.c_str(), 41);
    strncpy((char*)(ed + 118), "xSchedule", 41);
    
    fppBroadcastSocket->SendTo(remoteAddr, &buffer[0], bufsize);
    delete fppBroadcastSocket;
}

void SyncFPP::SendSync(uint32_t frameMS, uint32_t stepLengthMS, uint32_t stepMS, uint32_t playlistMS, const std::string& fseq, const std::string& media, const std::string& step, const std::string& timeItem, uint32_t stepno, int overridetimeSecs) const {
    if (frameMS == 0) frameMS = 50;

    if (stepMS == 0xFFFFFFFF) {
        for (Tracked* t : { &_seq, &_media }) {
            if (!t->item.empty()) {
                SendFPPSync(t->item, SYNC_PKT_STOP, 0, frameMS);
                t->item.clear();
            }
        }
        _lastStepMS = 0;
        _lastStepNo = 0xFFFFFFFF;
        return;
    }

    // A new step, or the same one starting over (looped, repeated, or the same file in the next step). FPP ignores a
    // START for a file it is already playing, so the remotes need a STOP first.
    bool restart = stepno != _lastStepNo || stepMS + frameMS < _lastStepMS;
    _lastStepMS = stepMS;
    _lastStepNo = stepno;

    std::string seq = wxFileName(fseq).GetExt().Lower() == "fseq" ? fseq : "";
    Track(_seq, seq, stepMS, frameMS, restart, true);
    // media is synced on its own, so audio and video only steps reach the remotes too
    Track(_media, media, stepMS, frameMS, restart, false);
}

void SyncFPP::Track(Tracked& tracked, const std::string& item, uint32_t stepMS, uint32_t frameMS, bool restart, bool isSeq) const {
    if (item != tracked.item || (restart && !item.empty())) {
        if (!tracked.item.empty()) {
            SendFPPSync(tracked.item, SYNC_PKT_STOP, 0, frameMS);
        }
        tracked.item = item;
        tracked.lastSyncMS = stepMS;
        if (!item.empty()) {
            SendFPPSync(item, SYNC_PKT_START, stepMS, frameMS);
        }
        return;
    }
    if (item.empty()) return;

    uint32_t interval = FPP_MEDIA_SYNC_INTERVAL_MS;
    if (isSeq) {
        interval = (stepMS <= FPP_SEQ_SYNC_INITIAL_NUMBER_OF_FRAMES * frameMS ? FPP_SEQ_SYNC_INTERVAL_INITIAL_FRAMES : FPP_SEQ_SYNC_INTERVAL_FRAMES) * frameMS;
    }
    if (stepMS - tracked.lastSyncMS >= interval) {
        SendFPPSync(item, SYNC_PKT_SYNC, stepMS, frameMS);
        tracked.lastSyncMS = stepMS;
    }
}

std::vector<uint8_t> SyncFPP::MakeSyncPacket(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) {
    wxFileName fn(item);
    // FPP looks the file up by name, in UTF-8
    std::string name = fn.GetFullName().ToUTF8().data();
    bool seq = fn.GetExt().Lower() == "fseq";

    std::vector<uint8_t> buffer(sizeof(ControlPkt) + sizeof(SyncPkt) + name.size());
    ControlPkt* cp = reinterpret_cast<ControlPkt*>(&buffer[0]);
    memcpy(cp->fppd, "FPPD", 4);
    cp->pktType = CTRL_PKT_SYNC;
    cp->extraDataLen = buffer.size() - sizeof(ControlPkt);

    SyncPkt* sp = reinterpret_cast<SyncPkt*>(&buffer[0] + sizeof(ControlPkt));
    sp->pktType = pktType;
    sp->fileType = seq ? SYNC_FILE_SEQ : SYNC_FILE_MEDIA;
    // a START carries the position as well: FPP 10 starts media there, so a remote that joins part way through a
    // step lines up straight away
    bool hasPosition = pktType != SYNC_PKT_STOP;
    sp->frameNumber = hasPosition && seq && frameMS != 0 ? positionMS / frameMS : 0;
    sp->secondsElapsed = hasPosition ? positionMS / 1000.0f : 0.0f;
    memcpy(&sp->filename[0], name.c_str(), name.size() + 1);
    return buffer;
}

void SyncFPP::SendStop() const
{
    SendSync(50, 0, 0xFFFFFFFF, 0, "", "", "", "", 0, 0);
}

void SyncBroadcastFPP::SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const
{
    if (_fppBroadcastSocket == nullptr) return;
    auto packet = MakeSyncPacket(item, pktType, positionMS, frameMS);
    _fppBroadcastSocket->SendTo(_remoteAddr, packet.data(), packet.size());
}

void SyncUnicastFPP::SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const
{
    if (_fppUnicastSocket == nullptr) return;
    auto packet = MakeSyncPacket(item, pktType, positionMS, frameMS);
    for (const auto& it : _remotes) {
        wxIPV4address address;
        if (_lookup->Get(it, FPP_CTRL_PORT, address)) {
            _fppUnicastSocket->SendTo(address, packet.data(), packet.size());
        }
    }
}

bool HostLookup::Get(const std::string& host, unsigned short port, wxIPV4address& address)
{
    std::string ip;
    if (ip_utils::IsIPValid(host)) {
        ip = host;
    } else {
        std::lock_guard<std::mutex> lock(_lock);
        auto found = _found.find(host);
        if (found != _found.end()) {
            ip = found->second;
        } else {
            auto pending = _pending.find(host);
            if (pending != _pending.end()) {
                if (pending->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
                ip = pending->second.get();
                _pending.erase(pending);
                if (ip.empty()) {
                    spdlog::warn("FPP remote {} could not be found; trying again in 30 seconds.", host);
                    _failed[host] = time(nullptr);
                    return false;
                }
                _found[host] = ip;
                _failed.erase(host);
            } else {
                auto failed = _failed.find(host);
                if (failed != _failed.end() && time(nullptr) - failed->second < 30) return false;
                _pending[host] = std::async(std::launch::async, [host]() {
                    Diagnostics::ThreadName name("host lookup");
                    wxIPV4address a;
                    return a.Hostname(host) ? a.IPAddress().ToStdString() : std::string();
                });
                return false;
            }
        }
    }
    // a numeric address needs no lookup, so this doesn't block
    address.Hostname(ip);
    address.Service(port);
    return true;
}

void SyncUnicastCSVFPP::SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const
{
    wxFileName fn(item);
    if (fn.GetExt().Lower() != "fseq") return;

    for (auto it : _remotes)
    {
        SendUnicastSync(it, fn.GetFullName().ToStdString(), positionMS, frameMS, pktType);
    }
}

void SyncMulticastFPP::SendFPPSync(const std::string& item, uint8_t pktType, uint32_t positionMS, uint32_t frameMS) const
{
    if (_fppMulticastSocket == nullptr) return;
    auto packet = MakeSyncPacket(item, pktType, positionMS, frameMS);
    _fppMulticastSocket->SendTo(_remoteAddr, packet.data(), packet.size());
}

SyncBroadcastFPP::SyncBroadcastFPP(SyncBroadcastFPP&& from) noexcept : SyncFPP(from)
{
    _fppBroadcastSocket = from._fppBroadcastSocket;
    from._fppBroadcastSocket = nullptr; // this is a transfer of ownership
    _remoteAddr = from._remoteAddr;
}

SyncUnicastFPP::SyncUnicastFPP(SyncUnicastFPP&& from) noexcept : SyncFPP(from)
{
    _fppUnicastSocket = from._fppUnicastSocket;
    from._fppUnicastSocket = nullptr; // this is a transfer of ownership
    _remotes = from._remotes;
    _lookup = from._lookup;
}

SyncUnicastCSVFPP::SyncUnicastCSVFPP(SyncUnicastCSVFPP&& from) noexcept : SyncFPP(from)
{
    _fppUnicastSocket = from._fppUnicastSocket;
    from._fppUnicastSocket = nullptr; // this is a transfer of ownership
    _remotes = from._remotes;
    _lookup = from._lookup;
}

SyncMulticastFPP::SyncMulticastFPP(SyncMulticastFPP&& from) noexcept : SyncFPP(from)
{
    _fppMulticastSocket = from._fppMulticastSocket;
    from._fppMulticastSocket = nullptr; // this is a transfer of ownership
    _remoteAddr = from._remoteAddr;
}

SyncBroadcastFPP::~SyncBroadcastFPP()
{
    if (_fppBroadcastSocket != nullptr) {
        _fppBroadcastSocket->Close();
        delete _fppBroadcastSocket;
        _fppBroadcastSocket = nullptr;
    }
}

SyncUnicastFPP::~SyncUnicastFPP()
{
    if (_fppUnicastSocket != nullptr) {
        _fppUnicastSocket->Close();
        delete _fppUnicastSocket;
        _fppUnicastSocket = nullptr;
    }
}

SyncUnicastCSVFPP::~SyncUnicastCSVFPP()
{
    if (_fppUnicastSocket != nullptr) {
        _fppUnicastSocket->Close();
        delete _fppUnicastSocket;
        _fppUnicastSocket = nullptr;
    }
}

SyncMulticastFPP::~SyncMulticastFPP()
{
    if (_fppMulticastSocket != nullptr) {
        _fppMulticastSocket->Close();
        delete _fppMulticastSocket;
        _fppMulticastSocket = nullptr;
    }
}

SyncBroadcastFPP::SyncBroadcastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP) :
    SyncFPP(sm, rm, options, schm)
{
    if (sm == SYNCMODE::FPPBROADCASTMASTER)
    {
        _remoteAddr.Hostname("255.255.255.255");
        _remoteAddr.Service(FPP_CTRL_PORT);

        wxIPV4address localaddr;
        if (localIP == "")
        {
            localaddr.AnyAddress();
        }
        else
        {
            localaddr.Hostname(localIP);
        }

        _fppBroadcastSocket = new wxDatagramSocket(localaddr, wxSOCKET_NOWAIT | wxSOCKET_BROADCAST);
        if (_fppBroadcastSocket == nullptr)
        {
            spdlog::error("Error opening datagram for FPP Sync as master. {}", localaddr.IPAddress().ToStdString());
        }
        else if (!_fppBroadcastSocket->IsOk())
        {
            spdlog::error("Error opening datagram for FPP Sync as master. {} OK : FALSE", localaddr.IPAddress().ToStdString());
            delete _fppBroadcastSocket;
            _fppBroadcastSocket = nullptr;
        }
        else if (_fppBroadcastSocket->Error())
        {
            spdlog::error("Error opening datagram for FPP Sync as master. {} : {}", (int)_fppBroadcastSocket->LastError(), DecodeIPError(_fppBroadcastSocket->LastError()));
            delete _fppBroadcastSocket;
            _fppBroadcastSocket = nullptr;
        }
        else
        {
            spdlog::info("FPP Sync as master datagram opened successfully.");
        }
    }

    if (rm == REMOTEMODE::FPPBROADCASTSLAVE || rm == REMOTEMODE::FPPUNICASTSLAVE || rm == REMOTEMODE::FPPSLAVE)
    {
        listenerManager->SetRemoteFPP();
    }
}

SyncUnicastFPP::SyncUnicastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP) :
    SyncFPP(sm, rm, options, schm)
{
    if (sm == SYNCMODE::FPPUNICASTMASTER)
    {
        wxIPV4address localaddr;
        if (localIP == "")
        {
            localaddr.AnyAddress();
        }
        else
        {
            localaddr.Hostname(localIP);
        }

        _remotes = options.GetFPPRemotes();
        // start looking the remotes up now, so their addresses are known before a show starts
        for (const auto& it : _remotes) {
            wxIPV4address unused;
            _lookup->Get(it, FPP_CTRL_PORT, unused);
        }
        if (_remotes.size() > 0)
        {
            _fppUnicastSocket = new wxDatagramSocket(localaddr, wxSOCKET_NOWAIT);
            if (_fppUnicastSocket == nullptr)
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master {}.", localaddr.IPAddress().ToStdString());
            }
            else if (!_fppUnicastSocket->IsOk())
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master {}. OK : FALSE", localaddr.IPAddress().ToStdString());
                delete _fppUnicastSocket;
                _fppUnicastSocket = nullptr;
            }
            else if (_fppUnicastSocket->Error())
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master. {} : {} {}", (int)_fppUnicastSocket->LastError(), DecodeIPError(_fppUnicastSocket->LastError()), localaddr.IPAddress().ToStdString());
                delete _fppUnicastSocket;
                _fppUnicastSocket = nullptr;
            }
            else
            {
                spdlog::info("FPP Sync as master unicast datagram opened successfully.");
            }
        }
    }

    if (rm == REMOTEMODE::FPPUNICASTSLAVE || rm == REMOTEMODE::FPPBROADCASTSLAVE || rm == REMOTEMODE::FPPSLAVE)
    {
        listenerManager->SetRemoteFPP();
    }
}

SyncUnicastCSVFPP::SyncUnicastCSVFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP) :
    SyncFPP(sm, rm, options, schm) {
    if (sm == SYNCMODE::FPPUNICASTCSVMASTER)
    {
        wxIPV4address localaddr;
        if (localIP == "")
        {
            localaddr.AnyAddress();
        }
        else
        {
            localaddr.Hostname(localIP);
        }

        _remotes = options.GetFPPRemotes();
        // start looking the remotes up now, so their addresses are known before a show starts
        for (const auto& it : _remotes) {
            wxIPV4address unused;
            _lookup->Get(it, FPP_CTRL_PORT, unused);
        }
        if (_remotes.size() > 0)
        {
            _fppUnicastSocket = new wxDatagramSocket(localaddr, wxSOCKET_NOWAIT);
            if (_fppUnicastSocket == nullptr)
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master {}.", localaddr.IPAddress().ToStdString());
            }
            else if (!_fppUnicastSocket->IsOk())
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master {}. OK : FALSE", localaddr.IPAddress().ToStdString());
                delete _fppUnicastSocket;
                _fppUnicastSocket = nullptr;
            }
            else if (_fppUnicastSocket->Error())
            {
                spdlog::error("Error opening unicast datagram for FPP Sync as master. {} : {} {}", (int)_fppUnicastSocket->LastError(), DecodeIPError(_fppUnicastSocket->LastError()), localaddr.IPAddress().ToStdString());
                delete _fppUnicastSocket;
                _fppUnicastSocket = nullptr;
            }
            else
            {
                spdlog::info("FPP Sync as master unicast datagram opened successfully.");
            }
        }
    }

    if (rm == REMOTEMODE::FPPCSVSLAVE)
    {
        listenerManager->SetRemoteCSVFPP();
    }
}

SyncMulticastFPP::SyncMulticastFPP(SYNCMODE sm, REMOTEMODE rm, const ScheduleOptions& options, ScheduleManager* schm, ListenerManager* listenerManager, const std::string& localIP) :
    SyncFPP(sm, rm, options, schm) {
    if (sm == SYNCMODE::FPPMULTICASTMASTER)
    {
        _remoteAddr.Hostname(MULTISYNC_MULTICAST_ADDRESS);
        _remoteAddr.Service(FPP_CTRL_PORT);

        wxIPV4address localaddr;
        if (localIP == "")
        {
            localaddr.AnyAddress();
        }
        else
        {
            localaddr.Hostname(localIP);
        }

        _fppMulticastSocket = new wxDatagramSocket(localaddr, wxSOCKET_NOWAIT);
        if (_fppMulticastSocket == nullptr)
        {
            spdlog::error("Error opening multicast datagram for FPP Sync as master {}.", localaddr.IPAddress().ToStdString());
        }
        else if (!_fppMulticastSocket->IsOk())
        {
            spdlog::error("Error opening multicast datagram for FPP Sync as master {}. OK : FALSE", localaddr.IPAddress().ToStdString());
            delete _fppMulticastSocket;
            _fppMulticastSocket = nullptr;
        }
        else if (_fppMulticastSocket->Error())
        {
            spdlog::error("Error opening multicast datagram for FPP Sync as master. {} : {} {}", (int)_fppMulticastSocket->LastError(), DecodeIPError(_fppMulticastSocket->LastError()), localaddr.IPAddress().ToStdString());
            delete _fppMulticastSocket;
            _fppMulticastSocket = nullptr;
        }
        else
        {
            spdlog::info("FPP Sync as master multicast datagram opened successfully.");
        }
    }

    if (rm == REMOTEMODE::FPPUNICASTSLAVE || rm == REMOTEMODE::FPPBROADCASTSLAVE || rm == REMOTEMODE::FPPSLAVE)
    {
        listenerManager->SetRemoteFPP();
    }
}

void SyncUnicastCSVFPP::SendUnicastSync(const std::string& ip, const std::string& item, size_t msec, size_t frameMS, int action) const
{
    wxIPV4address remoteAddr;
    if (!_lookup->Get(ip, FPP_CTRL_CSV_PORT, remoteAddr)) return;

    std::string buffer;

    switch (action)
    {
    case SYNC_PKT_SYNC:
        buffer = wxString::Format("FPP,%d,%d,%d,%s,%d,%d\n", CTRL_PKT_SYNC, SYNC_FILE_SEQ, action, item.c_str(), static_cast<int>(msec / 1000), static_cast<int>(msec) % 1000).ToStdString();
        //logger_base.debug("Sending remote sync unicast packet to %s.", (const char*)ip.c_str());
        break;
    case SYNC_PKT_STOP:
        buffer = wxString::Format("FPP,%d,%d,%d,%s\n", CTRL_PKT_SYNC, SYNC_FILE_SEQ, action, item.c_str()).ToStdString();
        spdlog::debug("Sending remote stop unicast packet to {} : {}.", ip, buffer);
        break;
    case SYNC_PKT_START:
        buffer = wxString::Format("FPP,%d,%d,%d,%s\n", CTRL_PKT_SYNC, SYNC_FILE_SEQ, action, item.c_str()).ToStdString();
        spdlog::debug("Sending remote start unicast packet to {} : {}.", ip, buffer);
        break;
    case CTRL_PKT_BLANK:
        buffer = wxString::Format("FPP,%d\n", CTRL_PKT_BLANK).ToStdString();
        break;
    default:
        break;
    }

    if (_fppUnicastSocket != nullptr)
    {
        _fppUnicastSocket->SendTo(remoteAddr, static_cast<const char*>(buffer.c_str()), buffer.size());
    }
}
