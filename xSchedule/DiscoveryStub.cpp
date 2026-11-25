/***************************************************************
 * This source file is part of xSchedule
 **************************************************************/

// xSchedule never runs controller discovery, but some xLights output classes it
// compiles (ArtNetOutput, DDPOutput, TwinklyOutput, ZCPPOutput) define static
// PrepareDiscovery() functions that reference Discovery. Their EXCLUDEDISCOVERY
// guards are incomplete, so those functions still need these symbols to link.
// This no-op stub provides them, so xSchedule does not build xLights' full
// Discovery.cpp, which pulls in Bonjour/dns_sd and breaks the build when
// Avahi's compatibility dns_sd.h is installed.
//
// If an xlights submodule bump adds new Discovery calls in those outputs, the
// link will fail on the missing symbols; add them here.

#include "discovery/Discovery.h"

void Discovery::AddBroadcast(int port, std::function<void(uint8_t* buffer, int len, const std::string& fromIP)>&& callback) {
}

void Discovery::SendBroadcastData(int port, uint8_t* buffer, int len) {
}

void Discovery::SendData(int port, const std::string& host, uint8_t* buffer, int len) {
}

DiscoveredData* Discovery::AddController(ControllerEthernet* c) {
    return nullptr;
}

DiscoveredData* Discovery::FindByIp(const std::string& ip, const std::string& hostname, bool create) {
    return nullptr;
}

void DiscoveredData::SetModel(const std::string& m) {
    model = m;
}

void DiscoveredData::SetVendor(const std::string& v) {
    vendor = v;
}
