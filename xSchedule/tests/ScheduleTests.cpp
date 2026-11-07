/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

// Unit tests for the schedule logic (Schedule, Holidays). Builds as a small
// console program; see build_tests_msw.cmd and Makefile in this folder.

#include <wx/init.h>
#include <wx/xml/xml.h>

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <utility>

#include "../City.h"
#include "../Holidays.h"
#include "../Schedule.h"
#include "../SchedulePreview.h"
#include "../SettingsFiles.h"
#include "../ShowBackups.h"

#include <wx/config.h>

#include <fstream>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                 \
    } while (0)

wxDateTime D(int y, int m, int d, int h = 0, int mi = 0) {
    return wxDateTime((wxDateTime::wxDateTime_t)d, (wxDateTime::Month)(m - 1), y, h, mi);
}

using Attrs = std::initializer_list<std::pair<const char*, const char*>>;

Schedule Make(Attrs attrs) {
    wxXmlNode node(nullptr, wxXML_ELEMENT_NODE, "Schedule");
    node.AddAttribute("Enabled", "TRUE");
    for (const auto& a : attrs) {
        node.AddAttribute(a.first, a.second);
    }
    return Schedule(&node);
}

bool HasAttribute(wxXmlNode* node, const char* name) {
    return node->HasAttribute(name);
}

void TestHolidays() {
    CHECK(Holidays::DateFor("thanksgiving_us", 2026).IsSameDate(D(2026, 11, 26)));
    CHECK(Holidays::DateFor("thanksgiving_us", 2027).IsSameDate(D(2027, 11, 25)));
    CHECK(Holidays::DateFor("thanksgiving_us", 2028).IsSameDate(D(2028, 11, 23)));
    CHECK(Holidays::DateFor("thanksgiving_ca", 2026).IsSameDate(D(2026, 10, 12)));
    CHECK(Holidays::DateFor("easter", 2025).IsSameDate(D(2025, 4, 20)));
    CHECK(Holidays::DateFor("easter", 2026).IsSameDate(D(2026, 4, 5)));
    CHECK(Holidays::DateFor("easter", 2027).IsSameDate(D(2027, 3, 28)));
    CHECK(Holidays::DateFor("mothersday_us", 2026).IsSameDate(D(2026, 5, 10)));
    CHECK(Holidays::DateFor("memorialday_us", 2026).IsSameDate(D(2026, 5, 25)));
    CHECK(Holidays::DateFor("fathersday_us", 2026).IsSameDate(D(2026, 6, 21)));
    CHECK(Holidays::DateFor("laborday_us", 2026).IsSameDate(D(2026, 9, 7)));
    CHECK(Holidays::DateFor("halloween", 2026).IsSameDate(D(2026, 10, 31)));
    CHECK(Holidays::DateFor("christmas", 2026).IsSameDate(D(2026, 12, 25)));
    CHECK(!Holidays::DateFor("not_a_holiday", 2026).IsValid());
    for (const auto& h : Holidays::All()) {
        CHECK(Holidays::DateFor(h.id, 2026).IsValid());
    }
}

// the cases from the original Schedule::Test, through the public interface
void TestOriginalBehavior() {
    {
        Schedule s = Make({ { "StartDate", "2016-12-05" }, { "EndDate", "2017-01-12" }, { "StartTime", "22:00" }, { "EndTime", "02:00" }, { "DOW", "MonTue" } });
        CHECK(!s.IsActiveAt(D(2016, 12, 5, 21, 0)));
        CHECK(s.IsActiveAt(D(2016, 12, 5, 22, 0)));
        CHECK(s.IsActiveAt(D(2016, 12, 6, 1, 0)));
        CHECK(!s.IsActiveAt(D(2016, 12, 6, 3, 0)));
        CHECK(!s.IsActiveAt(D(2017, 1, 16, 23, 0)));
        CHECK(!s.IsActiveAt(D(2017, 1, 11, 22, 30))); // a Wednesday
    }
    {
        Schedule s = Make({ { "StartDate", "2017-12-26" }, { "EndDate", "2018-01-01" }, { "StartTime", "19:00" }, { "EndTime", "22:00" } });
        CHECK(s.IsActiveAt(D(2018, 1, 1, 19, 0)));
        CHECK(!s.IsActiveAt(D(2019, 1, 1, 19, 0)));
        CHECK(!s.IsActiveAt(D(2018, 1, 2, 19, 0)));
        CHECK(s.IsActiveAt(D(2017, 12, 26, 19, 0)));
        CHECK(!s.IsActiveAt(D(2018, 12, 26, 19, 0)));
        CHECK(!s.IsActiveAt(D(2017, 12, 25, 19, 0)));
    }
    {
        Schedule s = Make({ { "StartDate", "2017-12-26" }, { "EndDate", "2018-01-01" }, { "StartTime", "19:00" }, { "EndTime", "19:00" } });
        CHECK(!s.IsActiveAt(D(2017, 12, 26, 16, 0)));
        CHECK(s.IsActiveAt(D(2017, 12, 26, 19, 0)));
        CHECK(!s.IsActiveAt(D(2018, 12, 26, 19, 0)));
        CHECK(s.IsActiveAt(D(2017, 12, 27, 1, 0)));
        CHECK(s.IsActiveAt(D(2018, 1, 1, 1, 0)));
        CHECK(!s.IsActiveAt(D(2019, 1, 1, 1, 0)));
        CHECK(!s.IsActiveAt(D(2018, 1, 1, 20, 0)));
    }
    {
        Schedule s = Make({ { "StartDate", "2017-12-26" }, { "EndDate", "2018-01-01" }, { "StartTime", "19:00" }, { "EndTime", "19:00" }, { "EveryYear", "TRUE" } });
        CHECK(s.IsActiveAt(D(2019, 1, 1, 1, 0)));
        CHECK(s.IsActiveAt(D(2019, 12, 26, 19, 0)));
        CHECK(!s.IsActiveAt(D(2018, 1, 2, 19, 0)));
    }
    {
        Schedule s = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "19:00" }, { "EndTime", "22:00" } });
        s.SetEnabled(false);
        CHECK(!s.IsActiveAt(D(2026, 6, 1, 20, 0)));
        CHECK(s.GetUpcomingWindows(D(2026, 6, 1), 3).empty());
    }
}

void TestHolidaySeason() {
    // the day after Thanksgiving through Epiphany, every year
    Schedule s = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-01-06" }, { "EveryYear", "TRUE" }, { "StartTime", "17:00" }, { "EndTime", "22:00" },
                        { "StartHoliday", "thanksgiving_us" }, { "StartHolidayOffset", "1" }, { "EndHoliday", "epiphany" } });
    CHECK(s.GetStartHoliday() == "thanksgiving_us");
    CHECK(s.GetStartHolidayOffset() == 1);
    CHECK(s.GetEndHoliday() == "epiphany");
    CHECK(!s.IsActiveAt(D(2026, 11, 26, 18, 0)));
    CHECK(s.IsActiveAt(D(2026, 11, 27, 18, 0)));
    CHECK(s.IsActiveAt(D(2026, 12, 31, 18, 0)));
    CHECK(s.IsActiveAt(D(2027, 1, 6, 18, 0)));
    CHECK(!s.IsActiveAt(D(2027, 1, 7, 18, 0)));
    CHECK(!s.IsActiveAt(D(2027, 11, 25, 18, 0))); // Thanksgiving 2027 itself
    CHECK(s.IsActiveAt(D(2027, 11, 26, 18, 0)));
    CHECK(!s.IsActiveAt(D(2026, 11, 27, 23, 0))); // after the end time
    // every year: the current or next season, so the year depends on today
    wxDateTime season = s.GetEffectiveStartDate();
    CHECK(season.IsSameDate(Holidays::DateFor("thanksgiving_us", season.GetYear()) + wxDateSpan::Day()));
    CHECK(s.GetEffectiveEndDate().IsSameDate(D(season.GetYear() + 1, 1, 6)));
    CHECK(s.GetEffectiveEndDate() >= wxDateTime::Today());

    // more than a week ahead is found
    auto windows = s.GetUpcomingWindows(D(2026, 10, 6, 12, 0), 3);
    CHECK(windows.size() == 3);
    if (windows.size() == 3) {
        CHECK(windows[0].first == D(2026, 11, 27, 17, 0));
        CHECK(windows[0].second == D(2026, 11, 27, 22, 0));
        CHECK(windows[1].first == D(2026, 11, 28, 17, 0));
        CHECK(windows[2].first == D(2026, 11, 29, 17, 0));
    }
}

void TestSkipDates() {
    {
        Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "EveryYear", "TRUE" }, { "StartTime", "17:00" }, { "EndTime", "22:00" },
                            { "SkipDates", "2026-12-24" } });
        CHECK(s.IsActiveAt(D(2026, 12, 23, 18, 0)));
        CHECK(!s.IsActiveAt(D(2026, 12, 24, 18, 0)));
        CHECK(!s.IsActiveAt(D(2027, 12, 24, 18, 0))); // every year: month and day only
        CHECK(s.IsActiveAt(D(2026, 12, 25, 18, 0)));
        auto w = s.GetUpcomingWindows(D(2026, 12, 23, 18, 0), 1);
        CHECK(w.size() == 1 && w[0].first == D(2026, 12, 25, 17, 0));
    }
    {
        Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2027-12-31" }, { "StartTime", "17:00" }, { "EndTime", "22:00" }, { "SkipDates", "2026-12-24" } });
        CHECK(!s.IsActiveAt(D(2026, 12, 24, 18, 0)));
        CHECK(s.IsActiveAt(D(2027, 12, 24, 18, 0))); // one off: only that year
    }

    std::vector<wxDateTime> dates;
    CHECK(Schedule::ParseSkipDates("2026-12-24, 2026-12-31", dates));
    CHECK(dates.size() == 2);
    dates.clear();
    CHECK(!Schedule::ParseSkipDates("2026-13-01", dates));
    dates.clear();
    CHECK(!Schedule::ParseSkipDates("Dec 24", dates));
    dates.clear();
    CHECK(Schedule::ParseSkipDates("", dates) && dates.empty());

    Schedule s = Make({});
    s.SetSkipDates({ D(2026, 12, 31), D(2026, 12, 24), D(2026, 12, 24) });
    CHECK(s.GetSkipDatesAsString() == "2026-12-24, 2026-12-31");
}

void TestWindows() {
    Schedule overnight = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "22:00" }, { "EndTime", "02:00" }, { "DOW", "FriSat" } });
    auto w = overnight.GetUpcomingWindows(D(2026, 10, 6, 12, 0), 2); // a Tuesday
    CHECK(w.size() == 2);
    if (w.size() == 2) {
        CHECK(w[0].first == D(2026, 10, 9, 22, 0));
        CHECK(w[0].second == D(2026, 10, 10, 2, 0));
        CHECK(w[1].first == D(2026, 10, 10, 22, 0));
    }

    Schedule allDay = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-10" }, { "StartTime", "00:00" }, { "EndTime", "00:00" } });
    auto a = allDay.GetUpcomingWindows(D(2026, 11, 1), 5);
    CHECK(a.size() == 1);
    if (a.size() == 1) {
        CHECK(a[0].first == D(2026, 12, 1, 0, 0));
        CHECK(a[0].second == D(2026, 12, 10, 0, 0));
    }

    // a bad nth day in a file must not divide by zero
    Schedule nth = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "17:00" }, { "EndTime", "22:00" }, { "NthDay", "0" } });
    CHECK(nth.IsActiveAt(D(2026, 6, 1, 18, 0)));
}

void TestFireOnceIsNotShared() {
    // each schedule keeps its own fire state
    wxDateTime start = wxDateTime::Now() + wxTimeSpan(0, 30);
    std::string startText = start.Format("%H:%M").ToStdString();
    Schedule hourly = Make({ { "StartTime", startText.c_str() }, { "EndTime", "23:59" }, { "FireFrequency", "Fire every hour" } });
    Schedule once = Make({ { "StartTime", startText.c_str() }, { "EndTime", "23:59" } });
    CHECK(!hourly.ShouldFire());
    CHECK(once.ShouldFire());
}

void TestFileCompatibility() {
    // a schedule that uses none of the new settings saves exactly the old attributes
    Schedule plain = Make({ { "Name", "Nightly" }, { "StartDate", "2026-11-01" }, { "EndDate", "2027-01-06" }, { "StartTime", "17:00" }, { "EndTime", "22:00" }, { "HardStop", "TRUE" } });
    wxXmlNode* n = plain.Save();
    CHECK(n->GetAttribute("StartDate") == "2026-11-01");
    CHECK(n->GetAttribute("EndDate") == "2027-01-06");
    CHECK(n->GetAttribute("HardStop") == "TRUE");
    for (const char* attr : { "StopAtEndOfLoop", "StartHoliday", "StartHolidayOffset", "EndHoliday", "EndHolidayOffset", "SkipDates" }) {
        CHECK(!HasAttribute(n, attr));
    }
    delete n;

    // the new settings round trip, and the resolved dates are written for older versions
    Schedule holiday = Make({ { "Name", "Season" }, { "StartDate", "2026-01-01" }, { "EndDate", "2026-01-06" }, { "EveryYear", "TRUE" }, { "StartHoliday", "thanksgiving_us" },
                              { "StartHolidayOffset", "1" }, { "EndHoliday", "epiphany" }, { "SkipDates", "2026-12-24" }, { "StopAtEndOfLoop", "TRUE" } });
    n = holiday.Save();
    CHECK(n->GetAttribute("StartDate") == holiday.GetEffectiveStartDate().Format("%Y-%m-%d"));
    CHECK(n->GetAttribute("EndDate") == wxString::Format("%d-01-06", holiday.GetEffectiveStartDate().GetYear() + 1));
    CHECK(n->GetAttribute("StartHoliday") == "thanksgiving_us");
    CHECK(n->GetAttribute("StartHolidayOffset") == "1");
    CHECK(n->GetAttribute("EndHoliday") == "epiphany");
    CHECK(!HasAttribute(n, "EndHolidayOffset"));
    CHECK(n->GetAttribute("SkipDates") == "2026-12-24");
    CHECK(n->GetAttribute("StopAtEndOfLoop") == "TRUE");
    Schedule reloaded(n);
    CHECK(reloaded.IsStopAtEndOfLoop());
    CHECK(reloaded.GetSkipDates().size() == 1);
    CHECK(reloaded.IsActiveAt(D(2027, 11, 26, 19, 0)) == holiday.IsActiveAt(D(2027, 11, 26, 19, 0)));
    delete n;

    // copies keep the new settings
    Schedule copy(holiday, true);
    CHECK(copy.GetStartHoliday() == "thanksgiving_us" && copy.IsStopAtEndOfLoop() && copy.GetSkipDates().size() == 1);
}


// holiday offsets that cross New Year, and overnight windows with skip dates
void TestReviewFindings() {
    {
        // Christmas + 10 days lands in the next year; saving and reloading must not move it again
        Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-25" }, { "StartTime", "17:00" }, { "EndTime", "22:00" },
                            { "EndHoliday", "christmas" }, { "EndHolidayOffset", "10" } });
        CHECK(s.GetEffectiveEndDate().IsSameDate(D(2027, 1, 4)));
        wxXmlNode* n = s.Save();
        CHECK(n->GetAttribute("EndDate") == "2027-01-04");
        Schedule reloaded(n);
        CHECK(reloaded.GetEffectiveEndDate().IsSameDate(D(2027, 1, 4)));
        wxXmlNode* n2 = reloaded.Save();
        CHECK(n2->GetAttribute("EndDate") == "2027-01-04");
        delete n2;
        delete n;
    }
    {
        // New Year's Day - 5 days lands in the previous year
        Schedule s = Make({ { "StartDate", "2027-01-01" }, { "EndDate", "2027-01-10" }, { "StartTime", "17:00" }, { "EndTime", "22:00" },
                            { "StartHoliday", "newyearsday" }, { "StartHolidayOffset", "-5" } });
        CHECK(s.GetEffectiveStartDate().IsSameDate(D(2026, 12, 27)));
        wxXmlNode* n = s.Save();
        Schedule reloaded(n);
        CHECK(reloaded.GetEffectiveStartDate().IsSameDate(D(2026, 12, 27)));
        delete n;
    }
    {
        // every year, from 5 days before New Year's Day to Epiphany
        Schedule s = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-01-06" }, { "EveryYear", "TRUE" }, { "StartTime", "17:00" }, { "EndTime", "22:00" },
                            { "StartHoliday", "newyearsday" }, { "StartHolidayOffset", "-5" }, { "EndHoliday", "epiphany" } });
        CHECK(!s.IsActiveAt(D(2026, 12, 26, 18, 0)));
        CHECK(s.IsActiveAt(D(2026, 12, 27, 18, 0)));
        CHECK(s.IsActiveAt(D(2026, 12, 28, 18, 0)));
        CHECK(s.IsActiveAt(D(2027, 1, 3, 18, 0)));
        CHECK(!s.IsActiveAt(D(2027, 1, 7, 18, 0)));
        CHECK(s.IsActiveAt(D(2025, 12, 30, 18, 0)));
    }
    {
        // the last night of an every year season still runs past midnight
        Schedule s = Make({ { "StartDate", "2026-11-26" }, { "EndDate", "2027-01-06" }, { "EveryYear", "TRUE" }, { "StartTime", "18:00" }, { "EndTime", "02:00" } });
        CHECK(s.IsActiveAt(D(2027, 1, 6, 23, 0)));
        CHECK(s.IsActiveAt(D(2027, 1, 7, 1, 0)));
        CHECK(!s.IsActiveAt(D(2027, 1, 7, 3, 0)));
        CHECK(!s.IsActiveAt(D(2027, 1, 7, 19, 0)));
        CHECK(s.IsActiveAt(D(2027, 11, 26, 19, 0)));
    }
    {
        // a skipped night includes the part of its window after midnight, and only that night
        Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "18:00" }, { "EndTime", "02:00" }, { "SkipDates", "2026-12-24" } });
        CHECK(s.IsActiveAt(D(2026, 12, 24, 1, 0)));   // still the night of the 23rd
        CHECK(!s.IsActiveAt(D(2026, 12, 24, 19, 0)));
        CHECK(!s.IsActiveAt(D(2026, 12, 25, 0, 30)));
        CHECK(s.IsActiveAt(D(2026, 12, 25, 19, 0)));
    }
    {
        // a date range that is over needs no long search
        Schedule s = Make({ { "StartDate", "2020-01-01" }, { "EndDate", "2020-12-31" }, { "StartTime", "17:00" }, { "EndTime", "22:00" } });
        CHECK(s.GetUpcomingWindows(D(2026, 1, 1), 1).empty());
        Schedule later = Make({ { "StartDate", "2030-06-01" }, { "EndDate", "2030-06-30" }, { "StartTime", "17:00" }, { "EndTime", "22:00" } });
        auto w = later.GetUpcomingWindows(D(2026, 1, 1), 1, 5000);
        CHECK(w.size() == 1 && w[0].first == D(2030, 6, 1, 17, 0));
    }
}

void TestOvernightDays() {
    // Friday and Saturday shows that run past midnight: the hours after midnight belong to the night before
    Schedule s = Make({ { "DOW", "FriSat" }, { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "17:00" }, { "EndTime", "00:30" } });
    CHECK(!s.IsActiveAt(D(2026, 10, 8, 23, 0)));   // Thursday night
    CHECK(!s.IsActiveAt(D(2026, 10, 9, 0, 15)));   // still Thursday night
    CHECK(s.IsActiveAt(D(2026, 10, 9, 18, 0)));    // Friday
    CHECK(s.IsActiveAt(D(2026, 10, 10, 0, 15)));   // Friday night after midnight
    CHECK(s.IsActiveAt(D(2026, 10, 11, 0, 15)));   // Saturday night after midnight
    CHECK(!s.IsActiveAt(D(2026, 10, 11, 0, 45)));
    CHECK(!s.IsActiveAt(D(2026, 10, 11, 18, 0)));  // Sunday

    // the Upcoming tab agrees with what plays
    auto w = s.GetUpcomingWindows(D(2026, 10, 8, 12, 0), 2);
    CHECK(w.size() == 2);
    if (w.size() == 2) {
        CHECK(w[0].first == D(2026, 10, 9, 17, 0) && w[0].second == D(2026, 10, 10, 0, 30));
        CHECK(w[1].first == D(2026, 10, 10, 17, 0) && w[1].second == D(2026, 10, 11, 0, 30));
    }

    // every other day counts the night too
    Schedule n = Make({ { "NthDay", "2" }, { "NthDayOffset", "0" }, { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "20:00" }, { "EndTime", "01:00" } });
    wxDateTime on = D(2026, 3, 1, 21, 0);
    if (!n.IsActiveAt(on)) on += wxDateSpan::Day();
    CHECK(n.IsActiveAt(on));
    wxDateTime after = on + wxDateSpan::Day();
    after.SetHour(0);
    after.SetMinute(30);
    CHECK(n.IsActiveAt(after));                    // the same night, after midnight
}

void TestJSONAndState() {
    // names with quotes or backslashes still produce valid JSON
    Schedule s = Make({ { "Name", R"(Kids "Request" Hour \ 2)" }, { "StartTime", "17:00" }, { "EndTime", "22:00" } });
    // a schedule loaded from a file starts inactive (checked before GetJSON, which evaluates the live state)
    CHECK(!s.IsActive());
    std::string json = s.GetJSON("ref\"1");
    CHECK(json.find(R"("name":"Kids \"Request\" Hour \\ 2")") != std::string::npos);
    CHECK(json.find("\"reference\":\"ref\\\"1\"") != std::string::npos);
}

// minutes past midnight UTC of a local time of day on the given date
int UtcMinutes(const wxDateTime& local) {
    wxDateTime noon = local.GetDateOnly();
    noon.SetHour(12);
    int offset = (noon.FromUTC() - noon).GetMinutes();
    return ((local.GetHour() * 60 + local.GetMinute() - offset) % 1440 + 1440) % 1440;
}

// a longitude where the sun keeps this computer's clock time, so sun times land in the evening whatever its time zone
double LocalLongitude() {
    wxDateTime noon = D(2026, 12, 21, 12, 0);
    return (noon.FromUTC() - noon).GetMinutes() / 4.0;
}

void TestSunTimes() {
    // New York on 21 December 2026 (NOAA): civil dawn 11:46, sunrise 12:17, sunset 21:32, civil dusk 22:01 UTC
    const double lat = 40.7128;
    const double lon = -74.0060;
    wxDateTime day = D(2026, 12, 21);
    auto closeTo = [](int actual, int expected) { return std::abs(actual - expected) <= 3; };
    CHECK(closeTo(UtcMinutes(City::GetSunTime(lat, lon, day, City::SunEvent::Dawn)), 11 * 60 + 46));
    CHECK(closeTo(UtcMinutes(City::GetSunTime(lat, lon, day, City::SunEvent::Sunrise)), 12 * 60 + 17));
    CHECK(closeTo(UtcMinutes(City::GetSunTime(lat, lon, day, City::SunEvent::Sunset)), 21 * 60 + 32));
    CHECK(closeTo(UtcMinutes(City::GetSunTime(lat, lon, day, City::SunEvent::Dusk)), 22 * 60 + 1));

    // where it never gets dark enough for dusk there is still a time
    wxDateTime tromso = City::GetSunTime(69.65, 18.96, D(2026, 6, 21), City::SunEvent::Dusk);
    CHECK(tromso.IsValid());

    CHECK(City::GetNearestCity(40.6, -73.9) == "New York");
    CHECK(City::GetNearestCity(-33.0, 151.0) == "Sydney");

    // a schedule from dusk to sunrise
    double localLon = LocalLongitude();
    Schedule::SetLocation(true, 40.0, localLon);
    wxDateTime dusk = City::GetSunTime(40.0, localLon, day, City::SunEvent::Dusk);
    Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "Dusk" }, { "EndTime", "sunrise" } });
    CHECK(s.GetStartTimeAsString() == "dusk");
    CHECK(!s.IsActiveAt(dusk - wxTimeSpan(0, 5)));
    CHECK(s.IsActiveAt(dusk + wxTimeSpan(0, 5)));
    auto w = s.GetUpcomingWindows(D(2026, 12, 21, 12, 0), 1);
    CHECK(w.size() == 1 && w[0].first == dusk);

    // saved as sunset plus a flag, which older versions read as sunset
    wxXmlNode* n = s.Save();
    CHECK(n->GetAttribute("StartTime") == "sunset");
    CHECK(n->GetAttribute("StartTwilight") == "TRUE");
    CHECK(n->GetAttribute("EndTime") == "sunrise");
    CHECK(!HasAttribute(n, "EndTwilight"));
    Schedule reloaded(n);
    CHECK(reloaded.GetStartTimeAsString() == "dusk" && reloaded.GetEndTimeAsString() == "sunrise");
    delete n;

    Schedule dawn = Make({ { "StartTime", "sunup" }, { "EndTime", "SUNRISE" }, { "EndTwilight", "TRUE" } });
    CHECK(dawn.GetStartTimeAsString() == "sunup" && dawn.GetEndTimeAsString() == "dawn");

    Schedule plain = Make({ { "StartTime", "sunset" }, { "EndTime", "22:00" } });
    n = plain.Save();
    CHECK(!HasAttribute(n, "StartTwilight") && !HasAttribute(n, "EndTwilight"));
    delete n;

    // an unknown location falls back to fixed times
    Schedule::SetLocation(false, 0, 0);
    Schedule unknown = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "dusk" }, { "EndTime", "23:00" } });
    CHECK(!unknown.IsActiveAt(D(2026, 12, 21, 19, 55)));
    CHECK(unknown.IsActiveAt(D(2026, 12, 21, 20, 5)));
    Schedule::SetLocation(true, -33.861481, 151.205475);
}

void TestExtendEndTime() {
    double localLon = LocalLongitude();
    Schedule::SetLocation(true, 40.0, localLon);
    wxDateTime sunset = City::GetSunTime(40.0, localLon, D(2026, 12, 21), City::SunEvent::Sunset);

    Schedule s = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "12:00" }, { "EndTime", "sunset" }, { "OffOffsetMins", "15" } });
    wxDateTime after = sunset + wxTimeSpan(0, 25);
    CHECK(!s.IsActiveAt(after));
    s.AddMinsToEndTime(30);
    CHECK(s.IsActiveAt(after));
    CHECK(!s.IsActiveAt(sunset + wxTimeSpan(0, 50)));

    Schedule fixed = Make({ { "StartDate", "2026-12-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "17:00" }, { "EndTime", "22:00" } });
    CHECK(!fixed.IsActiveAt(D(2026, 12, 21, 22, 15)));
    fixed.AddMinsToEndTime(30);
    CHECK(fixed.IsActiveAt(D(2026, 12, 21, 22, 15)));

    Schedule::SetLocation(true, -33.861481, 151.205475);
}

// ---- SchedulePreview: the night played out by the scheduler's rules ----

namespace SP = SchedulePreview;
constexpr long long SEC = 1000;
constexpr long long MIN = 60 * SEC;
constexpr long long SIX_PM = 18 * 60 * MIN;

SP::PlayList List(const std::string& name, std::initializer_list<std::pair<const char*, int>> steps, bool firstOnce = false, bool lastOnce = false) {
    SP::PlayList p;
    p.name = name;
    p.firstOnce = firstOnce;
    p.lastOnce = lastOnce;
    for (const auto& st : steps) {
        SP::Step s;
        s.name = st.first;
        s.lengthMS = st.second * SEC;
        p.steps.push_back(s);
    }
    return p;
}

SP::Window Win(int schedule, int playlist, int priority, long long start, long long end, bool loop = true) {
    SP::Window w;
    w.schedule = schedule;
    w.name = "S" + std::to_string(schedule);
    w.playlist = playlist;
    w.priority = priority;
    w.start = start;
    w.end = end;
    w.loop = loop;
    return w;
}

SP::Plan Play(const SP::Input& in) {
    return SP::Run(in, in.windows.front().start, in.windows.front().start + 24 * 60 * MIN);
}

std::string Steps(const SP::Plan& plan) {
    std::string res;
    for (const auto& p : plan.plays) res += (res.empty() ? "" : " ") + p.step;
    return res;
}

const SP::Finding* FindingOf(const SP::Plan& plan, SP::Kind kind) {
    for (const auto& f : plan.findings) {
        if (f.kind == kind) return &f;
    }
    return nullptr;
}

void TestPreviewEndOfWindow() {
    SP::Input in;
    in.playlists.push_back(List("Main", { { "Intro", 45 }, { "S1", 100 }, { "S2", 100 }, { "Outro", 40 } }, true, true));
    in.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + 10 * MIN));

    // the end comes during a step: it finishes, then the last step plays
    auto plan = Play(in);
    CHECK(Steps(plan) == "Intro S1 S2 S1 S2 S1 S2 Outro");
    CHECK(plan.plays.front().start == SIX_PM);
    CHECK(plan.plays.back().start == SIX_PM + 645 * SEC);
    CHECK(plan.plays.back().end == SIX_PM + 685 * SEC);
    CHECK(plan.plays[3].pass == 2);
    const auto* late = FindingOf(plan, SP::Kind::RunsLate);
    CHECK(late != nullptr && late->amount == 85 * SEC && late->play == 7);
    CHECK(plan.plays.back().flag == SP::Flag::Late);

    // a hard stop cuts the step and skips the last one
    in.windows[0].hardStop = true;
    plan = Play(in);
    CHECK(Steps(plan) == "Intro S1 S2 S1 S2 S1 S2");
    CHECK(plan.plays.back().end == SIX_PM + 10 * MIN && plan.plays.back().ending == SP::Ending::Cut);
    const auto* cut = FindingOf(plan, SP::Kind::CutOff);
    CHECK(cut != nullptr && cut->amount == 45 * SEC);
    CHECK(FindingOf(plan, SP::Kind::RunsLate) == nullptr);

    // stop at the end of the loop finishes the pass
    SP::Input loop;
    loop.playlists.push_back(List("ABC", { { "A", 100 }, { "B", 100 }, { "C", 100 } }));
    loop.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + 2 * MIN + 30 * SEC));
    loop.windows[0].end = SIX_PM + 3 * MIN; // windows end on a minute
    plan = Play(loop);
    CHECK(Steps(plan) == "A B");
    loop.windows[0].stopAtEndOfLoop = true;
    plan = Play(loop);
    CHECK(Steps(plan) == "A B C");

    // loops: 2 passes, then the playlist is done and the lights go dark until the window ends
    SP::Input twice;
    twice.playlists.push_back(List("AB", { { "A", 60 }, { "B", 60 } }));
    twice.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + 10 * MIN));
    twice.windows[0].loops = 2;
    plan = Play(twice);
    CHECK(Steps(plan) == "A B A B");
    const auto* dark = FindingOf(plan, SP::Kind::GoesDark);
    CHECK(dark != nullptr && dark->at == SIX_PM + 4 * MIN && dark->amount == 6 * MIN);
}

void TestPreviewPriorities() {
    SP::Input in;
    in.playlists.push_back(List("Low", { { "L", 100 } }));
    in.playlists.push_back(List("High", { { "H", 30 } }));
    in.windows.push_back(Win(0, 0, 1, SIX_PM, SIX_PM + 10 * MIN));
    in.windows.push_back(Win(1, 1, 5, SIX_PM + 2 * MIN, SIX_PM + 4 * MIN, false));

    // a graceful interrupt waits for the step to end; the low one then starts its next step from the beginning
    in.windows[1].graceful = true;
    auto plan = Play(in);
    CHECK(plan.plays.size() >= 5);
    CHECK(plan.plays[1].step == "L" && plan.plays[1].end == SIX_PM + 200 * SEC && plan.plays[1].flag == SP::Flag::Wait);
    CHECK(plan.plays[2].step == "H" && plan.plays[2].start == SIX_PM + 200 * SEC && plan.plays[2].end == SIX_PM + 230 * SEC);
    CHECK(plan.plays[3].step == "L" && plan.plays[3].start == SIX_PM + 230 * SEC && plan.plays[3].offset == 0);
    const auto* late = FindingOf(plan, SP::Kind::StartsLate);
    CHECK(late != nullptr && late->amount == 80 * SEC && late->window == 1);

    // otherwise the low one stops mid-step and carries on from there
    in.windows[1].graceful = false;
    plan = Play(in);
    CHECK(plan.plays[1].start == SIX_PM + 100 * SEC && plan.plays[1].end == SIX_PM + 120 * SEC && plan.plays[1].ending == SP::Ending::Suspended);
    CHECK(plan.plays[2].step == "H" && plan.plays[2].start == SIX_PM + 120 * SEC);
    CHECK(plan.plays[3].step == "L" && plan.plays[3].start == SIX_PM + 150 * SEC && plan.plays[3].offset == 20 * SEC && plan.plays[3].end == SIX_PM + 230 * SEC);
    CHECK(FindingOf(plan, SP::Kind::StartsLate) == nullptr);

    // equal priority: the one that started first keeps the lights. The other is still waiting with its step started
    // when its window ends, so it finishes that step once the lights are free.
    in.windows[1].priority = 1;
    plan = Play(in);
    long long high = -1;
    for (const auto& p : plan.plays) {
        if (p.step == "H") high = p.start;
    }
    CHECK(high >= SIX_PM + 10 * MIN);
    CHECK(FindingOf(plan, SP::Kind::RunsLate) != nullptr && FindingOf(plan, SP::Kind::RunsLate)->window == 1);
}

void TestPreviewFireEvery() {
    Schedule s = Make({ { "StartDate", "2026-01-01" }, { "EndDate", "2026-12-31" }, { "StartTime", "18:00" }, { "EndTime", "18:10" },
                        { "FireFrequency", "Fire every 2 minutes" }, { "DOW", "MonTueWedThuFriSatSun" } });
    SP::Input in;
    in.playlists.push_back(List("Short", { { "Short", 20 } }));
    in.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + 10 * MIN, false));
    in.windows[0].fireOnce = false;
    const wxDateTime night = D(2026, 6, 1);
    in.shouldFire = [&](int, long long now, long long last) {
        return s.ShouldFireAt(night + wxTimeSpan::Milliseconds(now), night + wxTimeSpan::Milliseconds(last));
    };
    auto plan = Play(in);
    std::string starts;
    for (const auto& p : plan.plays) starts += std::to_string((p.start - SIX_PM) / MIN) + " ";
    CHECK(starts == "0 2 4 6 8 ");
    CHECK(s.ShouldFire() == s.ShouldFireAt(wxDateTime::Now(), wxDateTime::Now() - wxTimeSpan(2)));
}

void TestPreviewBranches() {
    // every 2nd loop from loop 2 plays the announcement
    SP::Input in;
    in.playlists.push_back(List("Loops", { { "Pick", 0 }, { "Announce", 2 }, { "Song", 2 } }));
    auto branch = std::make_shared<SP::Branch>();
    branch->isTrue = [](long long, int loop) { return loop >= 2 && (loop - 2) % 2 == 0; };
    branch->trueStep = "Announce";
    branch->falseStep = "Song";
    in.playlists[0].steps[0].branch = branch;
    in.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + MIN));
    auto plan = Play(in);
    CHECK(Steps(plan).rfind("Song Announce Song Song Announce Song", 0) == 0);

    // a branch that plays another playlist, then carries on
    SP::Input call;
    call.playlists.push_back(List("Call", { { "A", 2 }, { "Pick", 0 }, { "B", 3 } }));
    call.playlists.push_back(List("Insert", { { "Ins", 2 } }));
    auto always = std::make_shared<SP::Branch>();
    always->isTrue = [](long long, int) { return true; };
    always->truePlayList = "insert";
    call.playlists[0].steps[1].branch = always;
    call.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + MIN, false));
    plan = Play(call);
    CHECK(Steps(plan) == "A Ins B");
    CHECK(plan.plays.size() == 3 && plan.plays[1].called && plan.plays[1].start == SIX_PM + 2 * SEC && plan.plays[2].start == SIX_PM + 4 * SEC);

    // branches that point at each other stop the playlist instead of hanging
    SP::Input ring;
    ring.playlists.push_back(List("Ring", { { "P1", 0 }, { "P2", 0 }, { "Never", 2 } }));
    auto to2 = std::make_shared<SP::Branch>();
    to2->isTrue = [](long long, int) { return true; };
    to2->trueStep = "P2";
    auto to1 = std::make_shared<SP::Branch>();
    to1->isTrue = [](long long, int) { return true; };
    to1->trueStep = "P1";
    ring.playlists[0].steps[0].branch = to2;
    ring.playlists[0].steps[1].branch = to1;
    ring.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + MIN));
    plan = Play(ring);
    CHECK(plan.plays.empty());
}

void TestPreviewShuffle() {
    SP::Input in;
    in.playlists.push_back(List("Mix", { { "A", 30 }, { "B", 31 }, { "C", 32 }, { "D", 33 }, { "E", 34 } }));
    in.windows.push_back(Win(0, 0, 5, SIX_PM, SIX_PM + 10 * MIN));
    in.windows[0].random = true;
    in.seed = 7;
    auto plan = Play(in);
    CHECK(plan.plays.size() >= 15);
    bool estimated = true;
    bool distinct = true;
    for (size_t i = 0; i + 5 <= plan.plays.size(); i += 5) {
        std::string seen;
        for (size_t j = i; j < i + 5; ++j) {
            if (seen.find(plan.plays[j].step) != std::string::npos) distinct = false;
            seen += plan.plays[j].step;
        }
    }
    for (const auto& p : plan.plays) estimated = estimated && p.estimated;
    CHECK(distinct);
    CHECK(estimated);
    auto again = Play(in);
    CHECK(Steps(again) == Steps(plan)); // the same night shows the same order
    const auto* late = FindingOf(plan, SP::Kind::RunsLate);
    CHECK(late == nullptr || late->amount >= 0);
}

// a plugin's settings file: imported once from wxConfig, then the file wins; a damaged file starts empty
void TestPluginSettings() {
    const std::string name = "xScheduleTestsSettings";
    const auto file = XScheduleSettingsFolder() / (name + ".json");
    std::error_code ec;
    std::filesystem::remove(file, ec);
    {
        wxConfig legacy(name);
        legacy.DeleteAll();
    }
    {
        wxConfig legacy(name);
        legacy.Write("Long", 42L);
        legacy.Write("Text", "hello");
        legacy.Write("Flag", true);
        legacy.Flush();
    }
    {
        PluginSettings s(name, name);
        CHECK(std::filesystem::exists(file, ec));
        CHECK(s.ReadLong("Long", 0) == 42);
        CHECK(s.Read("Text", "") == "hello");
        CHECK(s.ReadBool("Flag", false));
        CHECK(s.ReadLong("Missing", 7) == 7);
        CHECK(!s.HasEntry("Missing"));
        s.Write("Long", 43);
        s.Write("Text", std::string("bye"));
        s.Write("Flag", false);
        s.Flush();
    }
    {
        wxConfig legacy(name);
        legacy.Write("Long", 99L);
        legacy.Flush();
    }
    {
        PluginSettings s(name, name);
        CHECK(s.ReadLong("Long", 0) == 43);
        CHECK(s.Read("Text", "") == "bye");
        CHECK(!s.ReadBool("Flag", true));
    }
    {
        std::ofstream out(file);
        out << "{ not json";
    }
    {
        PluginSettings s(name, name);
        CHECK(s.ReadLong("Long", 5) == 5);
        s.Write("Long", 6);
        s.Flush();
    }
    {
        PluginSettings s(name, name);
        CHECK(s.ReadLong("Long", 0) == 6);
        s.DeleteAll();
        s.Flush();
        CHECK(s.ReadLong("Long", 1) == 1);
    }
    CHECK(PluginSettings(name, name).ReadLong("Long", 2) == 2);
    std::filesystem::remove(file, ec);
    {
        wxConfig legacy(name);
        legacy.DeleteAll();
    }
}

std::string ReadAll(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteAll(const std::filesystem::path& file, const std::string& content) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << content;
}

std::tm At(int day, int hour, int min, int sec) {
    std::tm t{};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 11;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return t;
}

void TestShowBackups() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "xScheduleTestsBackups";
    const std::string show = "xlights.xschedule";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    std::string error;

    CHECK(ShowBackups::NameFor(show, At(16, 14, 30, 12)) == "xlights_2026-12-16_143012.xschedule");
    CHECK(ShowBackups::NameFor(show, At(16, 14, 30, 12), 2) == "xlights_2026-12-16_143012_2.xschedule");

    // no show file yet: nothing to keep
    CHECK(ShowBackups::Take(dir, show, At(1, 10, 0, 0), error));
    CHECK(!fs::exists(ShowBackups::Folder(dir)));

    WriteAll(dir / show, "v1");
    CHECK(ShowBackups::Take(dir, show, At(1, 10, 0, 0), error));
    auto list = ShowBackups::List(dir, show);
    CHECK(list.size() == 1 && list[0].file.filename() == "xlights_2026-12-01_100000.xschedule" && ReadAll(list[0].file) == "v1");
    CHECK(list.size() == 1 && list[0].when.tm_mday == 1 && list[0].when.tm_hour == 10 && list[0].size == 2);

    // the same show again is not kept twice
    CHECK(ShowBackups::Take(dir, show, At(1, 11, 0, 0), error));
    CHECK(ShowBackups::List(dir, show).size() == 1);

    // a change in the same second gets its own name, and lists as the newer
    WriteAll(dir / show, "v2");
    CHECK(ShowBackups::Take(dir, show, At(1, 10, 0, 0), error));
    list = ShowBackups::List(dir, show);
    CHECK(list.size() == 2 && list[0].file.filename() == "xlights_2026-12-01_100000_2.xschedule" && ReadAll(list[0].file) == "v2");

    // other files in the folder are left alone and not listed
    WriteAll(ShowBackups::Folder(dir) / "notes.txt", "keep me");
    WriteAll(ShowBackups::Folder(dir) / "xlights_old.xschedule", "keep me");
    CHECK(ShowBackups::List(dir, show).size() == 2);

    // only the newest are kept
    for (int i = 0; i < 60; ++i) {
        WriteAll(dir / show, "change " + std::to_string(i));
        CHECK(ShowBackups::Take(dir, show, At(2, 12, i / 60, i % 60), error));
    }
    list = ShowBackups::List(dir, show);
    CHECK(list.size() == ShowBackups::Keep);
    CHECK(ReadAll(list.front().file) == "change 59");
    CHECK(ReadAll(list.back().file) == "change 10");
    CHECK(fs::exists(ShowBackups::Folder(dir) / "notes.txt") && fs::exists(ShowBackups::Folder(dir) / "xlights_old.xschedule"));

    // restoring the oldest backup: the show as it was is kept first, which pushes the oldest out
    WriteAll(dir / show, "current");
    const auto oldest = list.back().file;
    CHECK(ShowBackups::Restore(dir, show, oldest, At(3, 9, 0, 0), error));
    CHECK(ReadAll(dir / show) == "change 10");
    list = ShowBackups::List(dir, show);
    CHECK(list.size() == ShowBackups::Keep && ReadAll(list.front().file) == "current");
    CHECK(!fs::exists(oldest));

    // a backup that has gone is reported, and the show is left as it is
    CHECK(!ShowBackups::Restore(dir, show, ShowBackups::Folder(dir) / "gone.xschedule", At(3, 9, 1, 0), error));
    CHECK(!error.empty() && ReadAll(dir / show) == "change 10");

    fs::remove_all(dir, ec);
}

} // namespace

int main(int argc, char** argv) {
    wxInitializer init(argc, argv);
    if (!init.IsOk()) {
        std::printf("could not initialize wxWidgets\n");
        return 2;
    }

    TestHolidays();
    TestOriginalBehavior();
    TestHolidaySeason();
    TestSkipDates();
    TestWindows();
    TestFireOnceIsNotShared();
    TestFileCompatibility();
    TestReviewFindings();
    TestOvernightDays();
    TestJSONAndState();
    TestSunTimes();
    TestExtendEndTime();
    TestPreviewEndOfWindow();
    TestPreviewPriorities();
    TestPreviewFireEvery();
    TestPreviewBranches();
    TestPreviewShuffle();
    TestPluginSettings();
    TestShowBackups();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
