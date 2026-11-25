// Schedule: when is a schedule active, and does it survive a save/load.

#include <catch2/catch_test_macros.hpp>

#include "Schedule.h"

#include <wx/xml/xml.h>

#include <memory>

// Friend of Schedule (see Schedule.h) so the date/time logic can be driven with a
// fixed "now" instead of the wall clock.
struct ScheduleTestAccess {
    static bool ActiveAt(Schedule& s, const wxDateTime& now) { return s.CheckActiveAt(now); }
    static bool OkDOW(Schedule& s, const wxDateTime& d) { return s.IsOkDOW(d); }
    static bool OkNthDay(Schedule& s, const wxDateTime& d) { return s.IsOkNthDay(d); }
    static void Dates(Schedule& s, const wxDateTime& start, const wxDateTime& end) {
        s._startDate = start;
        s._endDate = end;
    }
    static void Times(Schedule& s, int startHour, int endHour) {
        s._startTime = wxDateTime((wxDateTime::wxDateTime_t)startHour);
        s._endTime = wxDateTime((wxDateTime::wxDateTime_t)endHour);
        s._startTimeString.clear();
        s._endTimeString.clear();
    }
    static void DOW(Schedule& s, const std::string& dow) { s._dow = dow; }
};
using A = ScheduleTestAccess;

namespace {
// wxDateTime months are zero based; take them one based here so the tests read naturally.
wxDateTime D(int day, int month, int year, int hour = 0, int minute = 0) {
    return wxDateTime(day, (wxDateTime::Month)(month - 1), year, hour, minute);
}

std::unique_ptr<wxXmlNode> Node(std::initializer_list<std::pair<const char*, const char*>> attrs) {
    auto n = std::make_unique<wxXmlNode>(nullptr, wxXML_ELEMENT_NODE, "Schedule");
    for (const auto& [k, v] : attrs) {
        n->AddAttribute(k, v);
    }
    return n;
}
} // namespace

TEST_CASE("Day of week filter", "[schedule]") {
    Schedule s;
    A::DOW(s, "MonTue");
    CHECK(A::OkDOW(s, D(9, 1, 2017)));   // Monday
    CHECK(A::OkDOW(s, D(10, 1, 2017)));  // Tuesday
    CHECK_FALSE(A::OkDOW(s, D(11, 1, 2017)));
    CHECK_FALSE(A::OkDOW(s, D(12, 1, 2017)));

    s.SetDOW(false, false, false, false, false, true, true);
    CHECK(s.IsOnDOW("Sat"));
    CHECK(s.IsOnDOW("Sun"));
    CHECK_FALSE(s.IsOnDOW("Mon"));
    CHECK(A::OkDOW(s, D(14, 1, 2017)));  // Saturday
    CHECK_FALSE(A::OkDOW(s, D(13, 1, 2017)));
}

TEST_CASE("Schedule that runs past midnight", "[schedule]") {
    // 22:00 - 02:00, Mon/Tue only, 5 Dec 2016 - 12 Jan 2017
    Schedule s;
    A::Dates(s, D(5, 12, 2016), D(12, 1, 2017));
    A::Times(s, 22, 2);
    A::DOW(s, "MonTue");

    CHECK_FALSE(A::ActiveAt(s, D(5, 12, 2016, 21, 0)));  // before the start time
    CHECK(A::ActiveAt(s, D(5, 12, 2016, 22, 0)));        // at the start time
    CHECK(A::ActiveAt(s, D(6, 12, 2016, 1, 0)));         // after midnight
    CHECK_FALSE(A::ActiveAt(s, D(6, 12, 2016, 3, 0)));   // after the end time
    CHECK_FALSE(A::ActiveAt(s, D(16, 1, 2017, 23, 0)));  // after the end date
}

TEST_CASE("Schedule spanning new year", "[schedule]") {
    // 19:00 - 22:00 every day, 26 Dec 2017 - 1 Jan 2018
    Schedule s;
    A::Dates(s, D(26, 12, 2017), D(1, 1, 2018));
    A::Times(s, 19, 22);

    CHECK(A::OkDOW(s, D(1, 1, 2018)));
    CHECK(A::ActiveAt(s, D(1, 1, 2018, 19, 0)));
    CHECK(A::ActiveAt(s, D(26, 12, 2017, 19, 0)));
    CHECK_FALSE(A::ActiveAt(s, D(25, 12, 2017, 19, 0)));  // day before the start date
    CHECK_FALSE(A::ActiveAt(s, D(2, 1, 2018, 19, 0)));    // day after the end date
    CHECK_FALSE(A::ActiveAt(s, D(1, 1, 2019, 19, 0)));    // a year later
    CHECK_FALSE(A::ActiveAt(s, D(26, 12, 2018, 19, 0)));

    SECTION("every year repeats the date range") {
        s.SetEveryYear(true);
        CHECK(A::ActiveAt(s, D(26, 12, 2019, 19, 0)));
        CHECK(A::ActiveAt(s, D(1, 1, 2019, 19, 0)));
        CHECK_FALSE(A::ActiveAt(s, D(2, 1, 2019, 19, 0)));
        CHECK_FALSE(A::ActiveAt(s, D(25, 12, 2019, 19, 0)));
    }
}

TEST_CASE("Start time equal to end time runs all day", "[schedule]") {
    Schedule s;
    A::Dates(s, D(26, 12, 2017), D(1, 1, 2018));
    A::Times(s, 19, 19);

    CHECK_FALSE(A::ActiveAt(s, D(26, 12, 2017, 16, 0)));  // first day starts at the start time
    CHECK(A::ActiveAt(s, D(26, 12, 2017, 19, 0)));
    CHECK(A::ActiveAt(s, D(27, 12, 2017, 1, 0)));
    CHECK(A::ActiveAt(s, D(1, 1, 2018, 1, 0)));
    CHECK_FALSE(A::ActiveAt(s, D(1, 1, 2018, 20, 0)));    // last day ends at the end time
    CHECK_FALSE(A::ActiveAt(s, D(1, 1, 2019, 1, 0)));
    CHECK_FALSE(A::ActiveAt(s, D(26, 12, 2018, 19, 0)));

    SECTION("every year") {
        s.SetEveryYear(true);
        CHECK(A::ActiveAt(s, D(1, 1, 2019, 1, 0)));
        CHECK(A::ActiveAt(s, D(26, 12, 2019, 19, 0)));
        CHECK_FALSE(A::ActiveAt(s, D(2, 1, 2018, 19, 0)));
    }
}

TEST_CASE("Disabled schedule is never active", "[schedule]") {
    Schedule s;
    A::Dates(s, D(1, 1, 2020), D(31, 12, 2020));
    A::Times(s, 19, 22);
    REQUIRE(A::ActiveAt(s, D(15, 6, 2020, 20, 0)));

    s.SetEnabled(false);
    CHECK_FALSE(A::ActiveAt(s, D(15, 6, 2020, 20, 0)));
    CHECK_FALSE(s.IsActive());
}

TEST_CASE("Every Nth day", "[schedule]") {
    Schedule s;
    A::Dates(s, D(1, 1, 2020), D(31, 12, 2020));
    A::Times(s, 19, 22);
    s.SetNthDay(3);

    // day of year % 3 == offset
    CHECK(A::OkNthDay(s, D(3, 1, 2020)));       // day 3
    CHECK_FALSE(A::OkNthDay(s, D(4, 1, 2020)));
    CHECK_FALSE(A::OkNthDay(s, D(5, 1, 2020)));
    CHECK(A::OkNthDay(s, D(6, 1, 2020)));
    CHECK(A::ActiveAt(s, D(6, 1, 2020, 20, 0)));
    CHECK_FALSE(A::ActiveAt(s, D(7, 1, 2020, 20, 0)));

    s.SetNthDayOffset(1);
    CHECK(A::OkNthDay(s, D(4, 1, 2020)));
    CHECK_FALSE(A::OkNthDay(s, D(3, 1, 2020)));
}

TEST_CASE("Start and end times as strings", "[schedule]") {
    Schedule s;
    CHECK(s.GetStartTimeAsString() == "19:00");
    CHECK(s.GetEndTimeAsString() == "22:00");

    s.ClearDirty();
    s.SetStartTime("18:30");
    CHECK(s.GetStartTimeAsString() == "18:30");
    CHECK(s.IsDirty());

    s.SetEndTime("SunRise");  // sun based times are stored lower case
    CHECK(s.GetEndTimeAsString() == "sunrise");

    s.ClearDirty();
    s.SetStartTime("18:30");  // unchanged value doesn't dirty the schedule
    CHECK_FALSE(s.IsDirty());
}

TEST_CASE("Defaults when loading an empty node", "[schedule]") {
    auto n = Node({});
    Schedule s(n.get());
    CHECK(s.GetName() == "<unnamed>");
    CHECK(s.GetStartTimeAsString() == "19:00");
    CHECK(s.GetEndTimeAsString() == "22:00");
    CHECK(s.GetStartDate().Format("%Y-%m-%d") == "2017-01-01");
    CHECK(s.GetEndDate().Format("%Y-%m-%d") == "2099-01-01");
    CHECK(s.GetFireFrequency() == "Fire once");
    CHECK(s.GetNthDay() == 1);
    CHECK(s.GetPriority() == 0);
    CHECK_FALSE(s.GetEnabled());
    CHECK_FALSE(s.GetLoop());
    for (const char* d : { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" }) {
        CHECK(s.IsOnDOW(d));
    }
}

TEST_CASE("Save and load round trip", "[schedule]") {
    auto n = Node({ { "Name", "Christmas" },
                    { "DOW", "FriSat" },
                    { "StartDate", "2024-11-29" },
                    { "EndDate", "2025-01-06" },
                    { "StartTime", "sunset" },
                    { "EndTime", "23:30" },
                    { "Loop", "TRUE" },
                    { "Loops", "3" },
                    { "Random", "TRUE" },
                    { "EveryYear", "TRUE" },
                    { "GracefullyInterrupt", "TRUE" },
                    { "Enabled", "TRUE" },
                    { "OnOffsetMins", "-15" },
                    { "OffOffsetMins", "10" },
                    { "HardStop", "TRUE" },
                    { "Priority", "7" },
                    { "NthDay", "2" },
                    { "NthDayOffset", "1" },
                    { "FireFrequency", "Fire every 30 minutes" } });
    Schedule s(n.get());
    CHECK_FALSE(s.IsDirty());

    std::unique_ptr<wxXmlNode> saved(s.Save());
    Schedule r(saved.get());

    for (Schedule* x : { &s, &r }) {
        CAPTURE(x == &s ? "loaded" : "reloaded");
        CHECK(x->GetName() == "Christmas");
        CHECK(x->IsOnDOW("Fri"));
        CHECK(x->IsOnDOW("Sat"));
        CHECK_FALSE(x->IsOnDOW("Sun"));
        CHECK(x->GetStartDate().Format("%Y-%m-%d") == "2024-11-29");
        CHECK(x->GetEndDate().Format("%Y-%m-%d") == "2025-01-06");
        CHECK(x->GetStartTimeAsString() == "sunset");
        CHECK(x->GetEndTimeAsString() == "23:30");
        CHECK(x->GetLoop());
        CHECK(x->GetLoops() == 3);
        CHECK(x->GetRandom());
        CHECK(x->GetEveryYear());
        CHECK(x->GetGracefullyInterrupt());
        CHECK(x->GetEnabled());
        CHECK(x->GetOnOffsetMins() == -15);
        CHECK(x->GetOffOffsetMins() == 10);
        CHECK(x->IsHardStop());
        CHECK(x->GetPriority() == 7);
        CHECK(x->GetNthDay() == 2);
        CHECK(x->GetNthDayOffset() == 1);
        CHECK(x->GetFireFrequency() == "Fire every 30 minutes");
    }
}

TEST_CASE("Copying a schedule", "[schedule]") {
    Schedule s;
    s.SetName("orig");
    s.SetPriority(9);

    Schedule same(s);
    CHECK(same.GetId() == s.GetId());
    CHECK(same.GetName() == "orig");
    CHECK(same.GetPriority() == 9);

    Schedule fresh(s, true);
    CHECK(fresh.GetId() != s.GetId());
    CHECK(fresh.GetName() == "orig");
}
