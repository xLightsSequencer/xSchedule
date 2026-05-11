#pragma once

#include <string>
#include "../xlights/xLights/xLightsVersion.h"

static const std::string xschedule_version_string = "2026.05";
static const std::string xschedule_build_date     = __DATE__;

inline std::string GetXScheduleDisplayVersionString() {
#ifndef __WXOSX__
    return xschedule_version_string + " " + GetBitness();
#else
    return xschedule_version_string;
#endif
}
