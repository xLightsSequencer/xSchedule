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

#include <functional>
#include <memory>
#include <string>
#include <vector>

// Plays schedules out step by step without playing anything, to show what will play when. It follows the rules
// ScheduleManager::CheckSchedule and PlayList use, so a change to those needs the same change here. Times are ms from
// the night's midnight.
namespace SchedulePreview {

struct Branch {
    std::function<bool(long long ms, int loop)> isTrue;
    std::string trueStep;
    std::string falseStep;
    std::string truePlayList;
};

struct Step {
    std::string name;
    long long lengthMS = 0;
    bool excludeFromRandom = false;
    std::shared_ptr<Branch> branch;
};

struct PlayList {
    std::string name;
    std::vector<Step> steps;
    bool firstOnce = false;
    bool lastOnce = false;
    bool shuffle = false;
};

struct Window {
    int schedule = 0; // the windows of one schedule share it: a schedule runs once at a time
    std::string name;
    int playlist = 0; // into Input::playlists
    int priority = 0;
    long long start = 0;
    long long end = 0;
    bool loop = false;
    int loops = 0;
    bool random = false;
    bool fireOnce = true;
    bool graceful = false;
    bool hardStop = false;
    bool stopAtEndOfLoop = false;
};

struct Input {
    std::vector<PlayList> playlists;
    std::vector<Window> windows; // in the scheduler's order: playlist order, then schedule order
    // the schedule's fire rule; only asked about schedules that do not fire once
    std::function<bool(int schedule, long long now, long long lastFired)> shouldFire;
    unsigned seed = 1;
};

enum class Ending { Finished, Suspended, Cut, Unfinished };
enum class Flag { None, Late, Wait, Cut };

struct Play {
    long long start = 0;
    long long end = 0;
    int window = -1; // the window whose schedule played it, or that called its playlist
    int playlist = 0;
    std::string step;
    int pass = 1;
    long long offset = 0; // into the step, when it carries on after waiting
    bool called = false;  // a branch called its playlist
    bool estimated = false; // follows a shuffled pick
    Ending ending = Ending::Finished;
    Flag flag = Flag::None;
    std::string note;
};

enum class Kind { RunsLate, StartsLate, CutOff, GoesDark };

struct Finding {
    Kind kind = Kind::RunsLate;
    int window = -1;
    long long at = 0;     // when it happens
    long long amount = 0; // how late, how much was cut, or how long it is dark
    int play = -1;
    bool upTo = false; // a shuffled playlist: the most it can be
};

struct Plan {
    std::vector<Play> plays;
    std::vector<Finding> findings;
    bool truncated = false; // too many steps to follow
};

// plays out the windows from 'from', stopping once nothing is left to play or at 'until'
Plan Run(const Input& input, long long from, long long until);

std::string Duration(long long ms); // "4:39", or "1:02:03"
std::string Describe(const Finding& finding, const std::vector<Window>& windows);

} // namespace SchedulePreview
