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

// Holidays a schedule's start or end date can follow, and the calendar shows. The ids are stored in show files and
// must never change; the names are only for display.

#include <wx/datetime.h>

#include <string>
#include <utility>
#include <vector>

namespace Holidays {

struct Holiday {
    std::string id;
    std::string name;
    std::string countries; // country codes that keep it, space separated; empty for everywhere
};

const std::vector<Holiday>& All();

// The holiday's date in the given year (time 00:00), or an invalid date for an unknown id.
wxDateTime DateFor(const std::string& id, int year);
std::string NameFor(const std::string& id);

// (code, name) of the countries the holidays cover
const std::vector<std::pair<std::string, std::string>>& Countries();
// the code for a country as the city list names it, or "" for one not covered
std::string CountryCode(const std::string& countryName);
std::string CountryName(const std::string& code);

// the country whose holidays are shown; "" shows every country's
void SetCountry(const std::string& code);
const std::string& GetCountry();
bool IsShown(const Holiday& holiday);

} // namespace Holidays
