#pragma once

#include <string>
#include "../xlights/src-core/xLightsVersion.h"

static const std::string xschedule_version_string = "2026.06";
static const std::string xschedule_build_date     = __DATE__;

inline const std::string& GetBitness() {
    static const std::string bits = sizeof(size_t) == 8 ? "64bit" : (sizeof(size_t) == 4 ? "32bit" : "");
    return bits;
}

inline std::string GetXScheduleDisplayVersionString() {
#ifndef __WXOSX__
    return xschedule_version_string + " " + GetBitness();
#else
    return xschedule_version_string;
#endif
}
