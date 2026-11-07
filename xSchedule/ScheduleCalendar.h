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

#include "SchedulePreview.h"

#include <wx/datetime.h>

#include <string>
#include <vector>

class PlayList;
class Schedule;
class ScheduleManager;

// What the scheduler will do night by night, worked out from the same schedule rules it fires by. Times within a
// night are minutes from that night's midnight on the wall clock, so 1470 is 12:30 AM the next morning.
namespace ScheduleCalendar {

struct Window {
    Schedule* schedule = nullptr;
    PlayList* playlist = nullptr;
    wxDateTime night;
    int start = 0;
    int end = 0;
    // when its playlist finishes: before end for one that plays once or a set number of times
    int playEnd = 0;
    bool skipped = false;
    bool enabled = true;
    // playlist order, then schedule order: between equal priorities that start together, the scheduler keeps the first
    int order = 0;

    bool Competes() const { return enabled && !skipped; }
};

struct Night {
    wxDateTime date;
    std::string holiday;
    int sunset = -1;
    int dusk = -1;
    std::vector<Window> windows; // starting this night, in start order
};

enum class State { Plays, Hidden, Clash, Done };

struct Segment {
    int start = 0;
    int end = 0;
    State state = State::Plays;
    int by = -1; // the window it is hidden by or clashes with; for onTheLights, the window playing
};

struct Lane {
    int window = 0;
    std::vector<Segment> segments; // empty for a skipped or inactive window; Done after its playlist finishes
};

struct Clash {
    int start = 0;
    int end = 0;
    int a = 0;
    int b = 0;
};

struct Detail {
    Night night;
    std::vector<Window> windows; // everything overlapping the axis, from the nights either side too
    int axisStart = 0;
    int axisEnd = 0;
    std::vector<Lane> lanes;           // one per window
    std::vector<Segment> onTheLights;  // what plays, minute by minute, merged
    std::vector<Clash> clashes;
};

// the night played out step by step. Plays and findings refer to windows by their index here, and detailWindow
// gives each one's index in Detail::windows.
struct Preview {
    std::vector<SchedulePreview::Window> windows;
    std::vector<int> detailWindow;
    std::vector<std::string> playlists; // by SchedulePreview::Play::playlist
    SchedulePreview::Plan plan;
    std::string background; // the background playlist, which plays alongside
};

Night NightOf(ScheduleManager& manager, const wxDateTime& date, bool includeInactive);
std::vector<Night> Nights(ScheduleManager& manager, const wxDateTime& from, const wxDateTime& to, bool includeInactive);
Detail DetailOf(ScheduleManager& manager, const wxDateTime& date, bool includeInactive);
// same-priority windows that overlap, among windows that compete
std::vector<Clash> ClashesOf(const std::vector<Window>& windows);
Preview PreviewOf(ScheduleManager& manager, const Detail& detail);

std::string FormatMinutes(int minutes); // "5:47 PM", in the computer's 12 or 24 hour style
std::string FormatRange(int start, int end);
std::string FormatTime(long long ms); // to the second, from the night's midnight

} // namespace ScheduleCalendar
