/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "Schedule.h"
#include <wx/xml/xml.h>
#include <wx/tokenzr.h>
#include <algorithm>
#include <log.h>
#include "City.h"
#include "Holidays.h"

int __scheduleid = 0;
bool Schedule::__locationKnown = true;
double Schedule::__latitude = -33.861481; // Sydney, the default city
double Schedule::__longitude = 151.205475;

namespace {
// dawn and dusk are saved as sunrise and sunset plus a twilight flag, so older versions read them as sunrise and sunset
std::string LoadTime(wxXmlNode* node, const char* attribute, const char* defaultTime, const char* twilightAttribute, wxDateTime& time)
{
    std::string s = node->GetAttribute(attribute, defaultTime).Lower().ToStdString();
    if (node->GetAttribute(twilightAttribute, "FALSE") == "TRUE") {
        if (s == "sunrise" || s == "sunup") s = "dawn";
        else if (s == "sunset" || s == "sundown") s = "dusk";
    }
    if (Schedule::IsSunTime(s)) {
        time.ParseTime("0:00");
        return s;
    }
    time.ParseTime(s);
    return "";
}

void SaveTime(wxXmlNode* node, const char* attribute, const char* twilightAttribute, const std::string& sunTime, const wxDateTime& time)
{
    if (sunTime == "dawn" || sunTime == "dusk") {
        node->AddAttribute(attribute, sunTime == "dawn" ? "sunrise" : "sunset");
        node->AddAttribute(twilightAttribute, "TRUE");
    } else if (sunTime != "") {
        node->AddAttribute(attribute, sunTime);
    } else {
        node->AddAttribute(attribute, time.Format("%H:%M:%S"));
    }
}
}

bool Schedule::IsSunTime(const std::string& time)
{
    return time == "sunrise" || time == "sunset" || time == "sunup" || time == "sundown" || time == "dawn" || time == "dusk";
}

Schedule::Schedule(wxXmlNode* node)
{
    _lastFired = wxDateTime::Now() - wxTimeSpan(2, 0, 0, 0); // set the last time at least 2 hours ago
    _id = __scheduleid++;
    _changeCount = 0;
    _lastSavedChangeCount = 0;
    Schedule::Load(node);
}

wxDateTime Schedule::GetStartTime() const
{
    wxDateTime res = wxDateTime::Now();
    SetTime(res, _startTime, _startTimeString, _onOffsetMins);
    return res;
}

wxDateTime Schedule::GetNextFireTime() const
{
    // note if the next fire time is outside the current active window this isnt going to return a valid answer
    wxDateTime start = GetStartTime();
    wxDateTime now = wxDateTime::Now();
    wxDateTime res = now;

    // int minute = now.GetMinute();

    res.SetMinute(start.GetMinute());

    if (_fireFrequency == "Fire every 90 minutes") {
        res.SetHour(start.GetHour());
        while (res <= now) {
            res.Add(wxTimeSpan(1, 30));
        }
    } else {
        res.Subtract(wxTimeSpan(1));
    }

    if (_fireFrequency == "Fire every hour")
    {
        while (res <= now) {
            res.Add(wxTimeSpan(1));
        }
    }
    else if (_fireFrequency == "Fire every 30 minutes")
    {
        while (res <= now)
        {
            res.Add(wxTimeSpan(0, 30));
        }
    }
    else if (_fireFrequency == "Fire every 20 minutes")
    {
        while (res <= now)
        {
            res.Add(wxTimeSpan(0, 20));
        }
    }
    else if (_fireFrequency == "Fire every 15 minutes")
    {
        while (res <= now)
        {
            res.Add(wxTimeSpan(0, 15));
        }
    }
    else if (_fireFrequency == "Fire every 10 minutes")
    {
        while (res <= now)
        {
            res.Add(wxTimeSpan(0, 10));
        }
    }
    else if (_fireFrequency == "Fire every 5 minutes") {
        while (res <= now) {
            res.Add(wxTimeSpan(0, 5));
        }
    }
    else if (_fireFrequency == "Fire every 2 minutes")
    {
        while (res <= now)
        {
            res.Add(wxTimeSpan(0, 2));
        }
    }

    return res;
}

wxTimeSpan Schedule::GetTimeSinceStartTime() const
{
    wxDateTime now = wxDateTime::Now();
    wxDateTime st = GetStartTime();
    wxDateTime start = wxDateTime(now.GetDay(), now.GetMonth(), now.GetYear(), st.GetHour(), st.GetMinute(), 0);

    spdlog::debug("last start time {}.", (const char*)start.FormatISOCombined().c_str());
    spdlog::debug("now {}.", (const char*)now.FormatISOCombined().c_str());

    if (start > now)
    {
        start -= wxDateSpan::Day();
        spdlog::debug("last start time adjusted by 24 hrs {}.", (const char*)start.FormatISOCombined().c_str());
    }

    return now - start;
}

void Schedule::Load(wxXmlNode* node)
{
    _changeCount = 0;
    _lastSavedChangeCount = 0;

    _name = node->GetAttribute("Name", "<unnamed>");
    _dow = node->GetAttribute("DOW", "MonTueWedThuFriSatSun");
    _startDate.ParseDate(node->GetAttribute("StartDate", "01/01/2017"));
    _endDate.ParseDate(node->GetAttribute("EndDate", "01/01/2099"));
    _startTimeString = LoadTime(node, "StartTime", "19:00", "StartTwilight", _startTime);
    _endTimeString = LoadTime(node, "EndTime", "22:00", "EndTwilight", _endTime);
    _loop = node->GetAttribute("Loop", "FALSE") == "TRUE";
    _random = node->GetAttribute("Random", "FALSE") == "TRUE";
    _everyYear = node->GetAttribute("EveryYear", "FALSE") == "TRUE";
    _gracefullyInterrupt = node->GetAttribute("GracefullyInterrupt", "FALSE") == "TRUE";
    _enabled = node->GetAttribute("Enabled", "FALSE") == "TRUE";
    _loops = wxAtoi(node->GetAttribute("Loops", "0"));
    _onOffsetMins = wxAtoi(node->GetAttribute("OnOffsetMins", "0"));
    _offOffsetMins = wxAtoi(node->GetAttribute("OffOffsetMins", "0"));
    _hardStop = node->GetAttribute("HardStop", "FALSE") == "TRUE";
    _priority = wxAtoi(node->GetAttribute("Priority", "0"));
    _nthDay = wxAtoi(node->GetAttribute("NthDay", "1"));
    _nthDayOffset = wxAtoi(node->GetAttribute("NthDayOffset", "0"));
    _fireFrequency = node->GetAttribute("FireFrequency", "Fire once");
    _stopAtEndOfLoop = node->GetAttribute("StopAtEndOfLoop", "FALSE") == "TRUE";
    _startHoliday = node->GetAttribute("StartHoliday", "").ToStdString();
    _startHolidayOffset = wxAtoi(node->GetAttribute("StartHolidayOffset", "0"));
    _endHoliday = node->GetAttribute("EndHoliday", "").ToStdString();
    _endHolidayOffset = wxAtoi(node->GetAttribute("EndHolidayOffset", "0"));
    _skipDates.clear();
    ParseSkipDates(node->GetAttribute("SkipDates", "").ToStdString(), _skipDates);
}

wxXmlNode* Schedule::Save()
{
    wxXmlNode * node = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "Schedule");

    node->AddAttribute("Name", _name);
    node->AddAttribute("DOW", _dow);
    node->AddAttribute("StartDate", GetEffectiveStartDate().Format("%Y-%m-%d"));
    node->AddAttribute("EndDate", GetEffectiveEndDate().Format("%Y-%m-%d"));
    SaveTime(node, "StartTime", "StartTwilight", _startTimeString, _startTime);
    SaveTime(node, "EndTime", "EndTwilight", _endTimeString, _endTime);
    node->AddAttribute("Priority", wxString::Format(wxT("%i"), _priority));
    node->AddAttribute("NthDay", wxString::Format(wxT("%i"), _nthDay));
    node->AddAttribute("NthDayOffset", wxString::Format(wxT("%i"), _nthDayOffset));
    node->AddAttribute("OnOffsetMins", wxString::Format(wxT("%i"), _onOffsetMins));
    node->AddAttribute("OffOffsetMins", wxString::Format(wxT("%i"), _offOffsetMins));
    if (_hardStop) node->AddAttribute("HardStop", "TRUE");
    node->AddAttribute("FireFrequency", _fireFrequency);
    if (_loop)
    {
        node->AddAttribute("Loop", "TRUE");
        node->AddAttribute("Loops", wxString::Format(wxT("%i"), _loops));
    }
    if (_random)
    {
        node->AddAttribute("Random", "TRUE");
    }
    if (_enabled)
    {
        node->AddAttribute("Enabled", "TRUE");
    }
    if (_everyYear)
    {
        node->AddAttribute("EveryYear", "TRUE");
    }
    if (_gracefullyInterrupt)
    {
        node->AddAttribute("GracefullyInterrupt", "TRUE");
    }
    // newer settings are only written when used, so files that do not use them are unchanged
    if (_stopAtEndOfLoop) node->AddAttribute("StopAtEndOfLoop", "TRUE");
    if (_startHoliday != "") {
        node->AddAttribute("StartHoliday", _startHoliday);
        if (_startHolidayOffset != 0) node->AddAttribute("StartHolidayOffset", wxString::Format(wxT("%i"), _startHolidayOffset));
    }
    if (_endHoliday != "") {
        node->AddAttribute("EndHoliday", _endHoliday);
        if (_endHolidayOffset != 0) node->AddAttribute("EndHolidayOffset", wxString::Format(wxT("%i"), _endHolidayOffset));
    }
    if (!_skipDates.empty()) node->AddAttribute("SkipDates", GetSkipDatesAsString());

    return node;
}

Schedule::Schedule()
{
    _active = false;
    _enabled = true;
    _id = __scheduleid++;
    _changeCount = 1;
    _lastSavedChangeCount = 0;
    _loop = true;
    _endDate.ParseDate("2099-01-01");
    _startDate.ParseDate("2017-01-01");
    _startTime.ParseTime("19:00");
    _endTime.ParseTime("22:00");
    _startTimeString = "";
    _endTimeString = "";
    _name = "<unnamed>";
    _random = false;
    _loops = 0;
    _onOffsetMins = 0;
    _offOffsetMins = 0;
    _hardStop = false;
    _everyYear = false;
    _gracefullyInterrupt = false;
    _priority = 5;
    _dow = "MonTueWedThuFriSatSun";
    _nthDay = 1;
    _nthDayOffset = 0;
    _fireFrequency = "Fire once";
}

// same as JSONSafe in UtilFunctions; kept here so the schedule logic builds on its own
static std::string EscapeJSON(const std::string& s)
{
    std::string res;
    for (char c : s) {
        if (c == '\\' || c == '"') res += '\\';
        res += c;
    }
    return res;
}

std::string Schedule::GetJSON(const std::string& reference)
{
    std::string res = "{\"name\":\"" + EscapeJSON(_name) +
        "\",\"id\":\"" + wxString::Format(wxT("%i"), _id).ToStdString() +
        "\",\"enabled\":\"" + std::string(_enabled ? "TRUE" : "FALSE") +
        "\",\"active\":\"" + std::string(CheckActive() ? "TRUE" : "FALSE") +
        "\",\"looping\":\"" + std::string(_loop ? "TRUE" : "FALSE") +
        "\",\"loops\":\"" + wxString::Format(wxT("%i"), _loops).ToStdString() +
        "\",\"random\":\"" + std::string(_random ? "TRUE" : "FALSE") +
        "\",\"nextactive\":\"" + GetNextTriggerTime();
 
    if (reference != "")
    {
        res += "\",\"reference\":\"" + EscapeJSON(reference);
    }

    res += "\",\"scheduleend\":\"" + (CheckActive() ? GetNextEndTime() : "N/A") +
        "\"}";

    return res;
}

Schedule::Schedule(const Schedule& schedule, bool newid)
{
    _active = schedule._active;
    _enabled = schedule._enabled;
    if (newid)
    {
        _id = __scheduleid++;
    }
    else
    {
        _id = schedule._id;
    }
    _changeCount = schedule._changeCount;
    _lastSavedChangeCount = schedule._lastSavedChangeCount;
    _loop = schedule._loop;
    _endDate = schedule._endDate;
    _startDate = schedule._startDate;
    _startTimeString = schedule._startTimeString;
    _startTime = schedule._startTime;
    _endTime = schedule._endTime;
    _endTimeString = schedule._endTimeString;
    _name = schedule._name;
    _random = schedule._random;
    _loops = schedule._loops;
    _onOffsetMins = schedule._onOffsetMins;
    _offOffsetMins = schedule._offOffsetMins;
    _hardStop = schedule._hardStop;
    _everyYear = schedule._everyYear;
    _gracefullyInterrupt = schedule._gracefullyInterrupt;
    _priority = schedule._priority;
    _dow = schedule._dow;
    _nthDay = schedule._nthDay;
    _nthDayOffset = schedule._nthDayOffset;
    _fireFrequency = schedule._fireFrequency;
    _stopAtEndOfLoop = schedule._stopAtEndOfLoop;
    _startHoliday = schedule._startHoliday;
    _startHolidayOffset = schedule._startHolidayOffset;
    _endHoliday = schedule._endHoliday;
    _endHolidayOffset = schedule._endHolidayOffset;
    _skipDates = schedule._skipDates;
    _lastFired = schedule._lastFired;
}


bool Schedule::IsOnDOW(const std::string& dow) const
{
    return wxString(_dow).Contains(dow);
}

void Schedule::SetDOW(bool mon, bool tue, bool wed, bool thu, bool fri, bool sat, bool sun)
{
    _dow = "";
    if (mon) _dow += "Mon";
    if (tue) _dow += "Tue";
    if (wed) _dow += "Wed";
    if (thu) _dow += "Thu";
    if (fri) _dow += "Fri";
    if (sat) _dow += "Sat";
    if (sun) _dow += "Sun";
}

void Schedule::SetTime(wxDateTime& toset, wxDateTime time, std::string timeString, int offset) const
{
    if (IsSunTime(timeString))
    {
        City::SunEvent event = City::SunEvent::Sunset;
        if (timeString == "sunrise" || timeString == "sunup") event = City::SunEvent::Sunrise;
        else if (timeString == "dawn") event = City::SunEvent::Dawn;
        else if (timeString == "dusk") event = City::SunEvent::Dusk;

        if (__locationKnown)
        {
            wxDateTime sun = City::GetSunTime(__latitude, __longitude, toset, event);
            sun += wxTimeSpan(0, offset);
            toset.SetHour(sun.GetHour());
            toset.SetMinute(sun.GetMinute());
        }
        else
        {
            bool morning = event == City::SunEvent::Sunrise || event == City::SunEvent::Dawn;
            toset.SetHour(morning ? 6 : 20);
            toset.SetMinute(0);
        }
    }
    else
    {
        toset.SetHour(time.GetHour());
        toset.SetMinute(time.GetMinute());
    }
}

void Schedule::Test()
{
    spdlog::warn("Running Schedule tests.");

    Schedule s;

    s._startDate = wxDateTime(5, (wxDateTime::Month)11, 2016);
    s._endDate = wxDateTime(12, (wxDateTime::Month)0, 2017);
    s._startTime = wxDateTime((wxDateTime::wxDateTime_t)22);
    s._endTime = wxDateTime((wxDateTime::wxDateTime_t)2);
    s._dow = "MonTue";
    wxASSERT(s.IsOkDOW(wxDateTime(9, (wxDateTime::Month)0, 2017)));
    wxASSERT(s.IsOkDOW(wxDateTime(10, (wxDateTime::Month)0, 2017)));
    wxASSERT(!s.IsOkDOW(wxDateTime(11, (wxDateTime::Month)0, 2017)));
    wxASSERT(!s.IsOkDOW(wxDateTime(12, (wxDateTime::Month)0, 2017)));

    wxASSERT(!s.CheckActiveAt(wxDateTime(5, (wxDateTime::Month)11, 2016, 21, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(5, (wxDateTime::Month)11, 2016, 22, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(6, (wxDateTime::Month)11, 2016, 1, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(6, (wxDateTime::Month)11, 2016, 3, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(16, (wxDateTime::Month)0, 2017, 23, 0)));

    s._startDate = wxDateTime(26, (wxDateTime::Month)11, 2017);
    s._endDate = wxDateTime(1, (wxDateTime::Month)0, 2018);
    s._startTime = wxDateTime((wxDateTime::wxDateTime_t)19);
    s._endTime = wxDateTime((wxDateTime::wxDateTime_t)22);
    s._dow = "MonTueWedThuFriSatSun";
    wxASSERT(s.IsOkDOW(wxDateTime(1, (wxDateTime::Month)0, 2018)));
    wxASSERT(s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2018, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2019, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(2, (wxDateTime::Month)0, 2018, 19, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2017, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2018, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(25, (wxDateTime::Month)11, 2017, 19, 0)));

    s._startTime = wxDateTime((wxDateTime::wxDateTime_t)19);
    s._endTime = wxDateTime((wxDateTime::wxDateTime_t)19);
    wxASSERT(!s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2017, 16, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2017, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2018, 19, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(27, (wxDateTime::Month)11, 2017, 1, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2018, 1, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2019, 1, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2018, 20, 0)));

    s._everyYear = true;
    wxASSERT(s.CheckActiveAt(wxDateTime(1, (wxDateTime::Month)0, 2019, 1, 0)));
    wxASSERT(s.CheckActiveAt(wxDateTime(26, (wxDateTime::Month)11, 2019, 19, 0)));
    wxASSERT(!s.CheckActiveAt(wxDateTime(2, (wxDateTime::Month)0, 2018, 19, 0)));

    spdlog::warn("    Schedule tests done.");
}

bool Schedule::IsOkDOW(const wxDateTime& date) const
{
    return (_dow.find(wxDateTime::GetEnglishWeekDayName(date.GetWeekDay(), wxDateTime::Name_Abbr).ToStdString()) != std::string::npos);
}

bool Schedule::CheckActive()
{
    return CheckActiveAt(wxDateTime::Now());
}

bool Schedule::ShouldFire() const
{
    return ShouldFireAt(wxDateTime::Now(), _lastFired);
}

bool Schedule::ShouldFireAt(const wxDateTime& now, const wxDateTime& lastFired) const
{
    bool fire = true;
    wxDateTime start = now;
    SetTime(start, _startTime, _startTimeString, _onOffsetMins);
    wxTimeSpan gap = now - lastFired;
    wxTimeSpan sinceStart = now - start;
    // the first fire is at the start time itself; the interval checks below only cover the later ones
    bool atStart = !sinceStart.IsNegative() && sinceStart < wxTimeSpan(0, 1, 0, 0);

    int minute = now.GetMinute();

    if (_fireFrequency == "Fire every hour") {
        fire = false;
        if (minute == start.GetMinute() &&
            gap.IsLongerThan(wxTimeSpan(0, 59, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0,59,59,0)))) {
            fire = true;
        }
    } 
    else if (_fireFrequency == "Fire every 90 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 90) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 89, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 89, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 30 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 30) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 29, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 29, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 20 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 20) % 60 ||
            minute == (start.GetMinute() + 40) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 19, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 19, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 15 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 15) % 60 ||
            minute == (start.GetMinute() + 30) % 60 || minute == (start.GetMinute() + 45) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 14, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 14, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 10 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 10) % 60 ||
            minute == (start.GetMinute() + 20) % 60 || minute == (start.GetMinute() + 30) % 60 ||
            minute == (start.GetMinute() + 40) % 60 || minute == (start.GetMinute() + 50) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 9, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 9, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 5 minutes") {
        fire = false;
        if ((minute == start.GetMinute() || minute == (start.GetMinute() + 5) % 60 ||
            minute == (start.GetMinute() + 10) % 60 || minute == (start.GetMinute() + 15) % 60 ||
            minute == (start.GetMinute() + 20) % 60 || minute == (start.GetMinute() + 25) % 60 ||
            minute == (start.GetMinute() + 30) % 60 || minute == (start.GetMinute() + 35) % 60 ||
            minute == (start.GetMinute() + 40) % 60 || minute == (start.GetMinute() + 45) % 60 ||
            minute == (start.GetMinute() + 50) % 60 || minute == (start.GetMinute() + 55) % 60) &&
            gap.IsLongerThan(wxTimeSpan(0, 4, 59, 0)) &&
            gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 4, 59, 0)))) {
            fire = true;
        }
    }
    else if (_fireFrequency == "Fire every 2 minutes") {
        fire = false;
        if (gap.GetMinutes() > 0 && (atStart || sinceStart.IsLongerThan(wxTimeSpan(0, 1, 59, 0)))) {
            for (int i = 0; i < 60; i += 2) {
                if (minute == (start.GetMinute() + i) % 60 ||
                    gap.IsLongerThan(wxTimeSpan(0, 1, 59, 0))) {
                    fire = true;
                    break;
                }
            }
        }
    }

    if (fire) {
        spdlog::debug("Schedule {} should fire now.", (const char*)_name.c_str());
    }

    return fire;
}

void Schedule::DidFire()
{
    spdlog::debug("Schedule {} did fire.", (const char*)_name.c_str());
    _lastFired = wxDateTime::Now();
}

//#define LOGCALCNEXTTRIGGERTIME

bool Schedule::IsOkNthDay(const wxDateTime& date) const
{
    if (_nthDay <= 1) return _nthDayOffset == 0 || _nthDay < 1; // every day; also guards a bad value from the file
    return ((date.GetDayOfYear() % _nthDay) - _nthDayOffset == 0);
}

bool Schedule::IsSkipDate(const wxDateTime& date) const
{
    for (const auto& d : _skipDates) {
        if (_everyYear) {
            if (d.GetMonth() == date.GetMonth() && d.GetDay() == date.GetDay()) return true;
        } else if (d.IsSameDate(date)) {
            return true;
        }
    }
    return false;
}

void Schedule::SetSkipDates(const std::vector<wxDateTime>& dates)
{
    std::vector<wxDateTime> clean;
    for (const auto& d : dates) {
        clean.push_back(d.GetDateOnly());
    }
    std::sort(clean.begin(), clean.end());
    clean.erase(std::unique(clean.begin(), clean.end()), clean.end());
    if (clean != _skipDates) {
        _skipDates = clean;
        _changeCount++;
    }
}

std::string Schedule::GetSkipDatesAsString() const
{
    std::string res;
    for (const auto& d : _skipDates) {
        if (!res.empty()) res += ", ";
        res += d.Format("%Y-%m-%d").ToStdString();
    }
    return res;
}

bool Schedule::ParseSkipDates(const std::string& text, std::vector<wxDateTime>& dates)
{
    bool ok = true;
    wxStringTokenizer tokens(text, ",; \t", wxTOKEN_STRTOK);
    while (tokens.HasMoreTokens()) {
        wxString token = tokens.GetNextToken();
        wxDateTime d;
        wxString::const_iterator end;
        if (d.ParseFormat(token, "%Y-%m-%d", &end) && end == token.end()) {
            dates.push_back(d.GetDateOnly());
        } else {
            ok = false;
        }
    }
    return ok;
}

wxDateTime Schedule::DateFor(bool start, int year) const
{
    const std::string& holiday = start ? _startHoliday : _endHoliday;
    if (!holiday.empty()) {
        wxDateTime d = Holidays::DateFor(holiday, year);
        if (d.IsValid()) {
            return d + wxDateSpan::Days(start ? _startHolidayOffset : _endHolidayOffset);
        }
    }
    wxDateTime d = start ? _startDate : _endDate;
    d.SetYear(year);
    return d;
}

// the year whose holiday a stored date was resolved from (the stored date includes the offset,
// which can carry it into the next or previous year)
int Schedule::HolidayYear(bool start) const
{
    const wxDateTime& stored = start ? _startDate : _endDate;
    return (stored - wxDateSpan::Days(start ? _startHolidayOffset : _endHolidayOffset)).GetYear();
}

// for a schedule that repeats every year: this year's season if it is not over yet, otherwise next year's
void Schedule::GetCurrentOrNextSeason(wxDateTime& start, wxDateTime& end) const
{
    const wxDateTime today = wxDateTime::Today();
    GetDateRange(today, start, end);
    if (end >= today) return;
    for (int year = today.GetYear(); year <= today.GetYear() + 2; ++year) {
        wxDateTime s = DateFor(true, year);
        wxDateTime e = DateFor(false, year);
        if (e < s) e = DateFor(false, year + 1);
        if (s > today) {
            start = s;
            end = e;
            return;
        }
    }
}

wxDateTime Schedule::GetEffectiveStartDate() const
{
    if (_startHoliday.empty()) return _startDate;
    if (_everyYear) {
        wxDateTime start, end;
        GetCurrentOrNextSeason(start, end);
        return start;
    }
    return DateFor(true, HolidayYear(true));
}

wxDateTime Schedule::GetEffectiveEndDate() const
{
    if (_endHoliday.empty()) return _endDate;
    if (_everyYear) {
        wxDateTime start, end;
        GetCurrentOrNextSeason(start, end);
        return end;
    }
    wxDateTime end = DateFor(false, HolidayYear(false));
    // a holiday end that lands before the start belongs to the following year
    if (end < GetEffectiveStartDate()) end = DateFor(false, HolidayYear(false) + 1);
    return end;
}

// the date range (dates only) that applies at 'now', honoring every year and holidays
void Schedule::GetDateRange(const wxDateTime& now, wxDateTime& start, wxDateTime& end) const
{
    if (_everyYear) {
        // the most recent season that has started (whether or not it is over: the window
        // check needs it for the hours after midnight of its last night). An offset can put
        // a season's start in the year before its holiday, so look one year either side.
        wxDateTime n = now.GetDateOnly();
        for (int year : { n.GetYear() + 1, n.GetYear(), n.GetYear() - 1 }) {
            start = DateFor(true, year);
            end = DateFor(false, year);
            if (end < start) end = DateFor(false, year + 1); // the season runs past New Year
            if (start <= n) break;
        }
    } else {
        start = GetEffectiveStartDate();
        end = GetEffectiveEndDate();
    }
}

void Schedule::SetStartTime(const std::string& start)
{
    wxString s = start;
    s = s.Lower();

    if (s != GetStartTimeAsString())
    {
        if (IsSunTime(s.ToStdString()))
        {
            _startTimeString = s;
        }
        else
        {
            _startTimeString = "";
            //_startTime.ParseTime(s);
            _startTime.ParseFormat(s, "%H:%M");
            _startTime.SetSecond(0);
        }
        _changeCount++;
    }
}

void Schedule::SetEndTime(const std::string& end)
{
    wxString e = end;
    e = e.Lower();

    if (e != GetEndTimeAsString())
    {
        if (IsSunTime(e.ToStdString()))
        {
            _endTimeString = e;
        }
        else
        {
            _endTimeString = "";
            //_endTime.ParseTime(e);
            _endTime.ParseFormat(e, "%H:%M");
            _endTime.SetSecond(0);
        }
        _changeCount++;
    }
}

bool Schedule::IsActiveAt(const wxDateTime& now) const
{
    return IsActiveAt(now, true);
}

bool Schedule::IsActiveAt(const wxDateTime& now, bool honorSkipAndEnabled) const
{
#ifdef LOGCALCNEXTTRIGGERTIME
    spdlog::debug("   Checking {}.", (const char *)now.Format("%Y-%m-%d %H:%M").c_str());
#endif

    if (honorSkipAndEnabled && !_enabled) return false;

    wxDateTime s = now;
    wxDateTime e = now;

    SetTime(s, _startTime, _startTimeString, _onOffsetMins);
    SetTime(e, _endTime, _endTimeString, _offOffsetMins);

    // a window that runs past midnight belongs to the night it started on, for the day of week too: a Friday and
    // Saturday 17:00-00:30 show runs into early Sunday, not early Friday
    wxDateTime night = now;
    if (e < s && now < e) night -= wxDateSpan::Day();

    if (!IsOkDOW(night) || !IsOkNthDay(night))
    {
#ifdef LOGCALCNEXTTRIGGERTIME
        spdlog::debug("       Wrong day of week.");
#endif

        return false;
    }
    if (honorSkipAndEnabled && IsSkipDate(night)) return false;

    wxDateTime start;
    wxDateTime end;
    GetDateRange(now, start, end);

    start.SetHour(s.GetHour());
    start.SetMinute(s.GetMinute());
    end.SetHour(e.GetHour());
    end.SetMinute(e.GetMinute());

#ifdef LOGCALCNEXTTRIGGERTIME
    spdlog::debug("       Now {}. Start {}. End {}", (const char *)now.Format("%Y-%m-%d %H:%M").c_str(), (const char *)start.Format("%Y-%m-%d %H:%M").c_str(), (const char *)end.Format("%Y-%m-%d %H:%M").c_str());
#endif

    if (e < s)
    {
        end.Add(wxDateSpan::Day());
    }

    // handle the 24 hours a day case
    if (s == e)
    {
        bool active = now >= start && now < end;

#ifdef LOGCALCNEXTTRIGGERTIME
        if (!active) spdlog::debug("       24 hrs a day but not within dates. {}-{}", (const char *)start.Format("%Y-%m-%d %H:%M").c_str(), (const char *)end.Format("%Y-%m-%d %H:%M").c_str());
#endif

        return active;
    }

    bool active = false;
    if (now >= start && now <= end)
    {
        // dates are ok ... now check the times
        start.SetYear(now.GetYear());
        start.SetDay(1); // do this to ensure day is valid within month
        start.SetMonth(now.GetMonth());
        start.SetDay(now.GetDay());
        end.SetYear(now.GetYear());
        end.SetDay(1); // do this to ensure day is valid within month
        end.SetMonth(now.GetMonth());
        end.SetDay(now.GetDay());

        if (e < s && now.FormatISOTime() < end.FormatISOTime())
        {
            start -= wxDateSpan::Day();
        }
        else if (e < s)
        {
            end.Add(wxDateSpan::Day());
        }

        active = now >= start && now < end;

#ifdef LOGCALCNEXTTRIGGERTIME
        if (!active) spdlog::debug("       Valid dates but not at this time {}-{}.", (const char *)start.Format("%Y-%m-%d %H:%M").c_str(), (const char *)end.Format("%Y-%m-%d %H:%M").c_str());
#endif
    }
    else
    {
#ifdef LOGCALCNEXTTRIGGERTIME
        spdlog::debug("       Not valid on this date {}-{}.", (const char *)start.Format("%Y-%m-%d %H:%M").c_str(), (const char *)end.Format("%Y-%m-%d %H:%M").c_str());
#endif
        // outside date range
        return false;
    }

    return active;
}

bool Schedule::CheckActiveAt(const wxDateTime& now)
{
    _active = IsActiveAt(now);
    return _active;
}

std::vector<std::pair<wxDateTime, wxDateTime>> Schedule::GetUpcomingWindows(const wxDateTime& from, size_t count, int maxDays) const
{
    std::vector<std::pair<wxDateTime, wxDateTime>> res;
    if (!_enabled || count == 0) return res;

    if (_dow.empty()) return res;

    wxDateTime day = from.GetDateOnly();
    wxDateTime last = day + wxDateSpan::Days(maxDays);
    if (!_everyYear) {
        wxDateTime first = GetEffectiveStartDate().GetDateOnly();
        if (first > day) day = first;
        wxDateTime rangeEnd = GetEffectiveEndDate().GetDateOnly();
        if (rangeEnd < last) last = rangeEnd;
    }
    for (; day <= last && res.size() < count; day += wxDateSpan::Day()) {
        wxDateTime start = day;
        SetTime(start, _startTime, _startTimeString, _onOffsetMins);
        start.SetSecond(0);
        if (start <= from || !IsActiveAt(start)) continue;

        wxDateTime end = day;
        SetTime(end, _endTime, _endTimeString, _offOffsetMins);
        end.SetSecond(0);
        if (end.FormatISOTime() == start.FormatISOTime()) {
            // 24 hours a day: one window that runs to the end of the date range
            wxDateTime rangeStart;
            wxDateTime rangeEnd;
            GetDateRange(start, rangeStart, rangeEnd);
            SetTime(rangeEnd, _endTime, _endTimeString, _offOffsetMins);
            rangeEnd.SetSecond(0);
            res.push_back({ start, rangeEnd });
            break;
        }
        if (end < start) end += wxDateSpan::Day(); // runs past midnight
        res.push_back({ start, end });
    }
    return res;
}

bool Schedule::GetWindowOn(const wxDateTime& night, wxDateTime& start, wxDateTime& end, bool& skipped) const
{
    if (_dow.empty()) return false;

    wxDateTime day = night.GetDateOnly();
    start = day;
    SetTime(start, _startTime, _startTimeString, _onOffsetMins);
    start.SetSecond(0);
    end = day;
    SetTime(end, _endTime, _endTimeString, _offOffsetMins);
    end.SetSecond(0);
    if (end <= start) end += wxDateSpan::Day(); // past midnight, or the same time: all day

    if (!IsActiveAt(start, false)) return false;
    skipped = IsSkipDate(day);
    return true;
}

wxDateTime Schedule::GetNextTriggerDateTime()
{
    wxDateTime now = wxDateTime::Now();
    if (CheckActive()) return now;

    auto next = GetUpcomingWindows(now, 1);
    if (next.empty()) return wxDateTime((time_t)0);
    return next.front().first;
}

std::string Schedule::GetNextNthDay(int nthDay, int nthDayOffset)
{
    wxDateTime now = wxDateTime::Now();

    for (int i = 0; i < 15; i++)
    {
        if ((now.GetDayOfYear() % nthDay) - nthDayOffset == 0)
        {
            return now.FormatISODate().ToStdString();
        }
        now += wxDateSpan::Day();
    }

    return "Unknown";
}

std::string Schedule::GetNextTriggerTime()
{
    if (CheckActive())
    {
        if (GetFireFrequency() == "Fire once")
        {
            return "NOW!";
        }
        else
        {
            wxDateTime nextFire = GetNextFireTime();
            if (!IsActiveAt(nextFire))
            {
                return "Done";
            }
            return nextFire.Format("%Y-%m-%d %H:%M").ToStdString();
        }
    }

    wxDateTime next = GetNextTriggerDateTime();
    if (next == wxDateTime((time_t)0))
    {
        return "Never";
    }
    return next.Format("%Y-%m-%d %H:%M").ToStdString();
}

void Schedule::AddMinsToEndTime(int mins)
{
    // a sun based end time is worked out again each time it is used, so its offset is what has to move
    if (_endTimeString != "")
    {
        _offOffsetMins += mins;
    }
    else
    {
        _endTime += wxTimeSpan(0, mins);
    }
}

std::string Schedule::GetNextEndTime()
{
    if (!_active) return "N/A";

    wxDateTime e1 = wxDateTime::Now();
    SetTime(e1, _endTime, _endTimeString, _offOffsetMins);
    wxDateTime s1 = wxDateTime::Now();
    SetTime(s1, _startTime, _startTimeString, _onOffsetMins);

    // when end and start are the same we play 24 hours a day
    if (s1 == e1)
    {
        wxDateTime rangeStart;
        wxDateTime end;
        GetDateRange(wxDateTime::Now(), rangeStart, end);

        SetTime(end, _endTime, _endTimeString, _offOffsetMins);
        end.SetSecond(0);

        return end.Format("%Y-%m-%d %H:%M").ToStdString();
    }
    else
    {
        wxDateTime end = wxDateTime::Now();
        SetTime(end, _endTime, _endTimeString, _offOffsetMins);
        wxDateTime start = wxDateTime::Now();
        SetTime(start, _startTime, _startTimeString, _onOffsetMins);

        if (end < start)
        {
            end += wxDateSpan::Day();
        }

        SetTime(end, _endTime, _endTimeString, _offOffsetMins);
        end.SetSecond(0);

        return end.Format("%Y-%m-%d %H:%M").ToStdString();
    }
}
