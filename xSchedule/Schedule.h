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

#include <wx/wx.h>
#include <string>
#include <utility>
#include <vector>

class wxWindow;
class wxXmlNode;

class Schedule
{
    static bool __locationKnown;
    static double __latitude;
    static double __longitude;
    wxUint32 _id;
    std::string _name;
    std::string _dow;
    int _lastSavedChangeCount;
    int _changeCount;
	wxDateTime _startDate;
	wxDateTime _endDate;
	wxDateTime _startTime;
    std::string _startTimeString;
	wxDateTime _endTime;
    std::string _endTimeString;
	bool _loop;
	int _loops;
	bool _random;
    bool _everyYear;
    int _priority;
    bool _active = false;
    bool _enabled;
    bool _gracefullyInterrupt;
    int _nthDay;
    int _nthDayOffset;
    std::string _fireFrequency;
    int _onOffsetMins = 0;
    int _offOffsetMins = 0;
    bool _hardStop = false;
    bool _stopAtEndOfLoop = false;
    // a start/end date can follow a holiday (plus or minus some days) instead of a fixed date
    std::string _startHoliday;
    int _startHolidayOffset = 0;
    std::string _endHoliday;
    int _endHolidayOffset = 0;
    // nights the schedule does not run; month and day only when it repeats every year
    std::vector<wxDateTime> _skipDates;
    wxDateTime _lastFired;

    void SetTime(wxDateTime& toset, wxDateTime time, std::string timeString, int offset) const;
    bool IsActiveAt(const wxDateTime& now, bool honorSkipAndEnabled) const;
    bool IsOkDOW(const wxDateTime& date) const;
    bool IsOkNthDay(const wxDateTime& date) const;
    bool IsSkipDate(const wxDateTime& date) const;
    bool CheckActiveAt(const wxDateTime& now);
    wxDateTime DateFor(bool start, int year) const;
    int HolidayYear(bool start) const;
    void GetCurrentOrNextSeason(wxDateTime& start, wxDateTime& end) const;
    void GetDateRange(const wxDateTime& now, wxDateTime& start, wxDateTime& end) const;

    public:

        static int GetMaxSchedulePriority() { return 20; }
        static int GetMaxSchedulePriorityBelowQueued() { return 10; }

        static void Test();

        static void SetLocation(bool known, double latitude, double longitude) { __locationKnown = known; __latitude = latitude; __longitude = longitude; }
        // sunrise, sunset, dawn or dusk (sunup and sundown are older names)
        static bool IsSunTime(const std::string& time);
        static bool GetLocation(double& latitude, double& longitude) { latitude = __latitude; longitude = __longitude; return __locationKnown; }
        wxUint32 GetId() const { return _id; }
        bool operator<(const Schedule& rhs) const { return _priority < rhs._priority; }
        bool operator==(const Schedule& rhs) const { return _id == rhs._id; }
        int GetPriority() const { return _priority; }
        void SetPriority(int priority) { if (_priority != priority) { _priority = priority; _changeCount++; } }
        std::string GetName() const { return _name; }
        void SetName(const std::string& name) { if (_name != name) { _name = name; _changeCount++; } }
        std::string GetStartTimeAsString() const { if (_startTimeString == "") return _startTime.Format("%H:%M").ToStdString(); else return _startTimeString; }
        std::string GetEndTimeAsString() const { if (_endTimeString == "") return _endTime.Format("%H:%M").ToStdString(); else return _endTimeString; }
        void SetStartTime(const std::string& start);
        void SetEndTime(const std::string& end);
        void SetLoops(int loops) { if (_loops != loops) { _loops = loops; _changeCount++; } }
        void SetOnOffsetMins(int offsetMins) { if (_onOffsetMins != offsetMins) { _onOffsetMins = offsetMins; _changeCount++; } }
        void SetOffOffsetMins(int offsetMins) { if (_offOffsetMins != offsetMins) { _offOffsetMins = offsetMins; _changeCount++; } }
        int GetLoops() const { return _loops; }
        void SetLoop(bool loop) { if (_loop != loop) { _loop = loop; _changeCount++; } }
        int GetNthDay() const { return _nthDay; }
        void SetNthDay(int nthDay) { if (_nthDay != nthDay) { _nthDay = nthDay; _changeCount++; } }
        bool IsHardStop() const { return _hardStop; }
        void SetHardStop(bool hardStop) { if (_hardStop != hardStop) { _hardStop = hardStop; _changeCount++; } }
        // at the end time, finish the playlist's current loop rather than its current step
        bool IsStopAtEndOfLoop() const { return _stopAtEndOfLoop; }
        void SetStopAtEndOfLoop(bool stop) { if (_stopAtEndOfLoop != stop) { _stopAtEndOfLoop = stop; _changeCount++; } }
        std::string GetStartHoliday() const { return _startHoliday; }
        void SetStartHoliday(const std::string& holiday) { if (_startHoliday != holiday) { _startHoliday = holiday; _changeCount++; } }
        int GetStartHolidayOffset() const { return _startHolidayOffset; }
        void SetStartHolidayOffset(int days) { if (_startHolidayOffset != days) { _startHolidayOffset = days; _changeCount++; } }
        std::string GetEndHoliday() const { return _endHoliday; }
        void SetEndHoliday(const std::string& holiday) { if (_endHoliday != holiday) { _endHoliday = holiday; _changeCount++; } }
        int GetEndHolidayOffset() const { return _endHolidayOffset; }
        void SetEndHolidayOffset(int days) { if (_endHolidayOffset != days) { _endHolidayOffset = days; _changeCount++; } }
        const std::vector<wxDateTime>& GetSkipDates() const { return _skipDates; }
        void SetSkipDates(const std::vector<wxDateTime>& dates);
        std::string GetSkipDatesAsString() const;
        // parses comma separated YYYY-MM-DD dates; false if any entry is not a valid date
        static bool ParseSkipDates(const std::string& text, std::vector<wxDateTime>& dates);
        // start and end dates with any holiday resolved (for the year of the stored date)
        wxDateTime GetEffectiveStartDate() const;
        wxDateTime GetEffectiveEndDate() const;
        int GetNthDayOffset() const { return _nthDayOffset; }
        void SetNthDayOffset(int nthDayOffset) { if (_nthDayOffset != nthDayOffset) { _nthDayOffset = nthDayOffset; _changeCount++; } }
        void SetEnabled(bool enabled) { if (_enabled != enabled) { _enabled = enabled; _changeCount++; } }
        bool GetLoop() const { return _loop; }
        int GetOnOffsetMins() const { return _onOffsetMins; }
        int GetOffOffsetMins() const { return _offOffsetMins; }
        bool GetEnabled() const { return _enabled; }
        void SetRandom(bool random) { if (_random != random) { _random = random; _changeCount++; } }
        bool GetRandom() const { return _random; }
        void SetFireFrequency(std::string fireFrequency) { if (_fireFrequency != fireFrequency) { _fireFrequency = fireFrequency; _changeCount++; } }
        std::string GetFireFrequency() const { return _fireFrequency; }
        void SetEveryYear(bool everyYear) { if (_everyYear != everyYear) { _everyYear = everyYear; _changeCount++; } }
        bool GetEveryYear() const { return _everyYear; }
        void SetGracefullyInterrupt(bool gracefullyInterrupt) { if (_gracefullyInterrupt != gracefullyInterrupt) { _gracefullyInterrupt = gracefullyInterrupt; _changeCount++; } }
        bool GetGracefullyInterrupt() const { return _gracefullyInterrupt; }
        bool IsOnDOW(const std::string& dow) const;
        void SetDOW(bool mon, bool tue, bool wed, bool thu, bool fri, bool sat, bool sun);
        wxDateTime GetStartDate() const { return _startDate; }
        wxDateTime GetEndDate() const { return _endDate; }
        wxDateTime GetStartTime() const;
        wxDateTime GetNextFireTime() const;
        wxTimeSpan GetTimeSinceStartTime() const;
        void SetStartDate(const wxDateTime& start) { if (_startDate != start) { _startDate = start; _changeCount++; } }
        void SetEndDate(const wxDateTime& end) { if (_endDate != end) { _endDate = end; _changeCount++; } }
        bool IsDirty() const { return _lastSavedChangeCount != _changeCount; }
        int GetChangeCount() const { return _changeCount; }
        void ClearDirty() { _lastSavedChangeCount = _changeCount; }
        std::string GetJSON(const std::string& reference);
        Schedule();
        Schedule(wxXmlNode* node);
        Schedule(const Schedule& schedule, bool newid = false);
        virtual ~Schedule() {}
		void Load(wxXmlNode* node);
		wxXmlNode* Save();
		Schedule* Configure(wxWindow* parent);
        bool IsActive() const { return _active; }
        bool CheckActive();
        // whether the schedule would be active at the given time; unlike CheckActive this changes nothing
        bool IsActiveAt(const wxDateTime& when) const;
        // the next windows (start, end) that start after 'from', looking at most maxDays ahead
        std::vector<std::pair<wxDateTime, wxDateTime>> GetUpcomingWindows(const wxDateTime& from, size_t count, int maxDays = 400) const;
        // the window that starts on the given night, if there is one; skipped is set for a skip date, which (like an
        // inactive schedule) still gets its window so a calendar can show it
        bool GetWindowOn(const wxDateTime& night, wxDateTime& start, wxDateTime& end, bool& skipped) const;
        std::string GetNextTriggerTime();
        std::string GetNextEndTime();
        void AddMinsToEndTime(int mins);
        wxDateTime GetNextTriggerDateTime();
        static std::string GetNextNthDay(int nthDay, int nthDayOffset);
        bool ShouldFire() const;
        bool ShouldFireAt(const wxDateTime& now, const wxDateTime& lastFired) const;
        void DidFire();
};
