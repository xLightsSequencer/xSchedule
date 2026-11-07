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

#include "ScheduleCalendar.h"

#include <wx/datetime.h>
#include <wx/panel.h>
#include <wx/timer.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

class CalendarDay;
class CalendarMonth;
class Schedule;
class ScheduleManager;
class wxCheckBox;
class wxListView;
class wxStaticText;

// The Calendar tab: a month of nights, and for the chosen night what plays on the lights and what waits.
class CalendarPanel : public wxPanel {
public:
    using EditSchedule = std::function<void(Schedule*)>;
    using AddSchedule = std::function<void(class PlayList*, const wxDateTime&)>;

    CalendarPanel(wxWindow* parent, EditSchedule edit, AddSchedule add);

    // works the calendar out again when a schedule, the month or the chosen night has changed; cheap otherwise
    void RefreshFrom(ScheduleManager* manager, bool force = false);
    void Select(const wxDateTime& date);
    // lets go of the schedules, before they are deleted
    void Forget();

private:
    void Recompute();
    void ShowNight(bool night);
    void Step(int direction);
    bool Keyboard(class wxKeyEvent& event);
    void NightMenu(const wxDateTime& night);
    std::string Signature() const;
    void OnClock(wxTimerEvent& event);
    void SetPlayByPlay(bool list);
    void FillList();
    void ShowFinding(int finding);
    void ActivateRow(long row);
    void FitHeader();

    ScheduleManager* _manager = nullptr;
    EditSchedule _edit;
    AddSchedule _add;
    wxDateTime _month;
    wxDateTime _selected;
    bool _inactive = false;
    std::string _signature;
    wxStaticText* _title = nullptr;
    class wxButton* _back = nullptr;
    class wxButton* _prev = nullptr;
    class wxButton* _next = nullptr;
    class wxSimplebook* _book = nullptr;
    bool _night = false;
    wxCheckBox* _inactiveBox = nullptr;
    CalendarMonth* _monthView = nullptr;
    CalendarDay* _dayView = nullptr;
    wxTimer _clock;

    // the night's play-by-play
    struct Row {
        int play = -1;
        int fold = -1;
        int finding = -1;
    };
    class wxPanel* _nightPage = nullptr;
    wxListView* _list = nullptr;
    wxStaticText* _footer = nullptr;
    class wxButton* _timelineButton = nullptr;
    class wxButton* _listButton = nullptr;
    bool _playByPlay = false;
    ScheduleCalendar::Detail _detail;
    ScheduleCalendar::Preview _preview;
    std::vector<Row> _rows;
    std::vector<std::pair<int, int>> _folds; // first and last play of steps shown as one row
    std::set<int> _unfolded;                 // folds shown in full, by their first play
    wxDateTime _unfoldedNight;
};
