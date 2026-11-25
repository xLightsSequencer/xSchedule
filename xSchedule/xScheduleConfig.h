#pragma once

/***************************************************************
 * This source file is part of xSchedule
 **************************************************************/

#include <string>

// Reads a setting from xLights' own configuration (not xSchedule's).
//
// From 2026.xx xLights stores its settings in a JSON file (settings.json in the
// platform AppData dir) rather than in wxConfig. Settings are looked up there
// first, falling back to the legacy wxConfig("xLights") store for users still
// running an older xLights.
std::string ReadXLightsSetting(const std::string& key, const std::string& def = "");
