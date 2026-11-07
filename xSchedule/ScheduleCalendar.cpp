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

#include "City.h"
#include "Holidays.h"
#include "PlayList/PlayList.h"
#include "PlayList/PlayListItemBranch.h"
#include "PlayList/PlayListStep.h"
#include "Schedule.h"
#include "ScheduleManager.h"

#include <wx/uilocale.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>

namespace ScheduleCalendar {

namespace {

long DayNumber(const wxDateTime& date)
{
    return std::lround(date.GetDateOnly().GetJDN());
}

int Minutes(const wxDateTime& night, const wxDateTime& t)
{
    return (int)(DayNumber(t) - DayNumber(night)) * 1440 + t.GetHour() * 60 + t.GetMinute();
}

std::string HolidayOn(const wxDateTime& date)
{
    static std::map<int, std::map<long, std::string>> byYear;
    static std::string country = "-";
    if (country != Holidays::GetCountry()) {
        country = Holidays::GetCountry();
        byYear.clear();
    }
    int year = date.GetYear();
    auto it = byYear.find(year);
    if (it == byYear.end()) {
        std::map<long, std::string> days;
        for (const auto& h : Holidays::All()) {
            if (!Holidays::IsShown(h)) continue;
            wxDateTime d = Holidays::DateFor(h.id, year);
            if (!d.IsValid()) continue;
            // "Thanksgiving (US, 4th Thu in Nov)" -> "Thanksgiving (US)", or just "Thanksgiving" when showing one
            // country's; "Christmas Eve (Dec 24)" -> "Christmas Eve"
            std::string name = h.name;
            if (auto open = name.find(" ("); open != std::string::npos) {
                std::string region = name.substr(open + 2, name.find_first_of(",)", open) - open - 2);
                bool date = region.find_first_of("0123456789") != std::string::npos;
                name = name.substr(0, open) + (date || !country.empty() ? "" : " (" + region + ")");
            }
            auto& day = days[DayNumber(d)];
            day += (day.empty() ? "" : ", ") + name;
        }
        it = byYear.emplace(year, std::move(days)).first;
    }
    auto day = it->second.find(DayNumber(date));
    return day == it->second.end() ? std::string() : day->second;
}

std::vector<Window> WindowsOn(ScheduleManager& manager, const wxDateTime& night, bool includeInactive)
{
    std::vector<Window> res;
    int order = 0;
    for (const auto& pl : manager.GetPlayLists()) {
        for (const auto& s : pl->GetSchedules()) {
            ++order;
            if (!s->GetEnabled() && !includeInactive) continue;
            wxDateTime start;
            wxDateTime end;
            bool skipped = false;
            if (!s->GetWindowOn(night, start, end, skipped)) continue;
            Window w;
            w.schedule = s;
            w.playlist = pl;
            w.night = night.GetDateOnly();
            w.start = Minutes(night, start);
            w.end = Minutes(night, end);
            w.playEnd = w.end;
            // a playlist that does not loop forever stops before the window ends, and whatever it was covering
            // plays again; one fired repeatedly through the window starts again, so it covers all of it
            size_t length = pl->GetLengthMS();
            if (length > 0 && s->GetFireFrequency() == "Fire once" && (!s->GetLoop() || s->GetLoops() > 0)) {
                size_t passes = s->GetLoop() ? (size_t)s->GetLoops() : 1;
                int minutes = (int)((length * passes + 59999) / 60000);
                w.playEnd = std::min(w.end, w.start + minutes);
            }
            w.skipped = skipped;
            w.enabled = s->GetEnabled();
            w.order = order;
            res.push_back(w);
        }
    }
    std::stable_sort(res.begin(), res.end(), [](const Window& a, const Window& b) { return a.start < b.start; });
    return res;
}

// the window playing at a minute: highest priority, then the one that started first, then the scheduler's order
int Playing(const std::vector<Window>& windows, int minute)
{
    int best = -1;
    for (int i = 0; i < (int)windows.size(); ++i) {
        const auto& w = windows[i];
        if (!w.Competes() || minute < w.start || minute >= w.playEnd) continue;
        if (best < 0) {
            best = i;
            continue;
        }
        const auto& b = windows[best];
        int wp = w.schedule->GetPriority();
        int bp = b.schedule->GetPriority();
        if (wp > bp || (wp == bp && (w.start < b.start || (w.start == b.start && w.order < b.order)))) best = i;
    }
    return best;
}

} // namespace

Night NightOf(ScheduleManager& manager, const wxDateTime& date, bool includeInactive)
{
    Night n;
    n.date = date.GetDateOnly();
    n.holiday = HolidayOn(n.date);
    double latitude;
    double longitude;
    Schedule::GetLocation(latitude, longitude);
    n.sunset = Minutes(n.date, City::GetSunTime(latitude, longitude, n.date, City::SunEvent::Sunset));
    n.dusk = Minutes(n.date, City::GetSunTime(latitude, longitude, n.date, City::SunEvent::Dusk));
    n.windows = WindowsOn(manager, n.date, includeInactive);
    return n;
}

std::vector<Night> Nights(ScheduleManager& manager, const wxDateTime& from, const wxDateTime& to, bool includeInactive)
{
    std::vector<Night> res;
    for (wxDateTime d = from.GetDateOnly(); d <= to.GetDateOnly(); d += wxDateSpan::Day()) {
        res.push_back(NightOf(manager, d, includeInactive));
    }
    return res;
}

std::vector<Clash> ClashesOf(const std::vector<Window>& windows)
{
    std::vector<Clash> res;
    for (int a = 0; a < (int)windows.size(); ++a) {
        for (int b = a + 1; b < (int)windows.size(); ++b) {
            const auto& wa = windows[a];
            const auto& wb = windows[b];
            if (!wa.Competes() || !wb.Competes() || wa.schedule->GetPriority() != wb.schedule->GetPriority()) continue;
            int s = std::max(wa.start, wb.start);
            int e = std::min(wa.playEnd, wb.playEnd);
            if (s < e) res.push_back({ s, e, a, b });
        }
    }
    return res;
}

Detail DetailOf(ScheduleManager& manager, const wxDateTime& date, bool includeInactive)
{
    Detail d;
    d.night = NightOf(manager, date, includeInactive);

    if (d.night.windows.empty()) {
        d.axisStart = 16 * 60;
        d.axisEnd = 25 * 60;
    } else {
        int s = d.night.windows.front().start;
        int e = s;
        for (const auto& w : d.night.windows) {
            s = std::min(s, w.start);
            e = std::max(e, w.end);
        }
        d.axisStart = (int)std::floor(s / 60.0) * 60;
        d.axisEnd = (int)std::ceil(e / 60.0) * 60;
        if (d.axisEnd - d.axisStart < 6 * 60) d.axisEnd = d.axisStart + 6 * 60;
    }

    // a window from the night before can still be running, and one from the next night can start inside the axis
    for (int offset : { -1, 0, 1 }) {
        std::vector<Window> ws = offset == 0 ? d.night.windows : WindowsOn(manager, d.night.date + wxDateSpan::Days(offset), includeInactive);
        for (auto w : ws) {
            w.start += offset * 1440;
            w.end += offset * 1440;
            if (w.end > d.axisStart && w.start < d.axisEnd) d.windows.push_back(w);
        }
    }
    std::stable_sort(d.windows.begin(), d.windows.end(), [](const Window& a, const Window& b) { return a.start < b.start; });

    for (int i = 0; i < (int)d.windows.size(); ++i) {
        const auto& w = d.windows[i];
        Lane lane;
        lane.window = i;
        if (w.Competes()) {
            for (int m = std::max(w.start, d.axisStart); m < std::min(w.playEnd, d.axisEnd); ++m) {
                State state = State::Plays;
                int by = -1;
                for (int j = 0; j < (int)d.windows.size(); ++j) {
                    const auto& o = d.windows[j];
                    if (j == i || !o.Competes() || m < o.start || m >= o.playEnd) continue;
                    if (o.schedule->GetPriority() > w.schedule->GetPriority()) {
                        state = State::Hidden;
                        by = Playing(d.windows, m);
                        break;
                    }
                    if (o.schedule->GetPriority() == w.schedule->GetPriority()) {
                        state = State::Clash;
                        by = j;
                    }
                }
                if (!lane.segments.empty() && lane.segments.back().state == state && lane.segments.back().by == by && lane.segments.back().end == m) {
                    lane.segments.back().end = m + 1;
                } else {
                    lane.segments.push_back({ m, m + 1, state, by });
                }
            }
            if (w.playEnd < w.end && w.playEnd < d.axisEnd) {
                lane.segments.push_back({ std::max(w.playEnd, d.axisStart), std::min(w.end, d.axisEnd), State::Done, -1 });
            }
        }
        d.lanes.push_back(lane);
    }

    for (int m = d.axisStart; m < d.axisEnd; ++m) {
        int p = Playing(d.windows, m);
        if (p < 0) continue;
        if (!d.onTheLights.empty() && d.onTheLights.back().by == p && d.onTheLights.back().end == m) {
            d.onTheLights.back().end = m + 1;
        } else {
            d.onTheLights.push_back({ m, m + 1, State::Plays, p });
        }
    }

    d.clashes = ClashesOf(d.windows);
    return d;
}

Preview PreviewOf(ScheduleManager& manager, const Detail& detail)
{
    Preview res;
    SchedulePreview::Input in;
    const wxDateTime night = detail.night.date;
    auto at = [night](long long ms) { return night + wxTimeSpan::Milliseconds(ms); };

    std::map<PlayList*, int> index;
    for (const auto& pl : manager.GetPlayLists()) {
        SchedulePreview::PlayList p;
        p.name = pl->GetNameNoTime();
        p.firstOnce = pl->GetFirstOnce();
        p.lastOnce = pl->GetLastOnce();
        p.shuffle = pl->GetShuffle();
        for (const auto& st : pl->GetSteps()) {
            SchedulePreview::Step s;
            s.name = st->GetNameNoTime();
            // a step counts as done on the first frame within a frame of its length (PlayListStep::Frame)
            s.lengthMS = std::max(0LL, (long long)st->GetLengthMS() - (long long)st->GetFrameMS() / 2);
            s.excludeFromRandom = st->GetExcludeFromRandom();
            if (PlayListItemBranch* b = st->GetBranch(); b != nullptr) {
                auto branch = std::make_shared<SchedulePreview::Branch>();
                branch->isTrue = [b, at](long long ms, int loop) { return b->IsTrue(at(ms), loop); };
                branch->trueStep = b->GetTrueStep();
                branch->falseStep = b->GetFalseStep();
                branch->truePlayList = b->GetTruePlayList();
                s.branch = branch;
            }
            p.steps.push_back(s);
        }
        index[pl] = (int)in.playlists.size();
        res.playlists.push_back(p.name);
        in.playlists.push_back(std::move(p));
    }

    // the scheduler looks at schedules in playlist order, then schedule order
    std::vector<int> order;
    for (int i = 0; i < (int)detail.windows.size(); ++i) {
        if (detail.windows[i].Competes()) order.push_back(i);
    }
    std::stable_sort(order.begin(), order.end(), [&detail](int a, int b) {
        const auto& wa = detail.windows[a];
        const auto& wb = detail.windows[b];
        return wa.order < wb.order || (wa.order == wb.order && wa.start < wb.start);
    });
    std::vector<Schedule*> schedules;
    long long from = LLONG_MAX;
    for (int i : order) {
        const auto& w = detail.windows[i];
        auto known = std::find(schedules.begin(), schedules.end(), w.schedule);
        SchedulePreview::Window pw;
        pw.schedule = (int)(known - schedules.begin());
        if (known == schedules.end()) schedules.push_back(w.schedule);
        pw.name = w.schedule->GetName();
        pw.playlist = index[w.playlist];
        pw.priority = w.schedule->GetPriority();
        pw.start = w.start * 60000LL;
        pw.end = w.end * 60000LL;
        pw.loop = w.schedule->GetLoop();
        pw.loops = w.schedule->GetLoops();
        pw.random = w.schedule->GetRandom();
        pw.fireOnce = w.schedule->GetFireFrequency() == "Fire once";
        pw.graceful = w.schedule->GetGracefullyInterrupt();
        pw.hardStop = w.schedule->IsHardStop();
        pw.stopAtEndOfLoop = w.schedule->IsStopAtEndOfLoop();
        from = std::min(from, pw.start);
        in.windows.push_back(pw);
        res.detailWindow.push_back(i);
    }
    if (PlayList* bg = manager.GetBackgroundPlayList(); bg != nullptr) res.background = bg->GetNameNoTime();
    if (in.windows.empty()) return res;

    in.shouldFire = [&schedules, at](int s, long long now, long long last) { return schedules[s]->ShouldFireAt(at(now), at(last)); };
    in.seed = (unsigned)DayNumber(night);
    // a show can run on past its window, but not for ever
    res.plan = SchedulePreview::Run(in, from, (detail.axisEnd + 6 * 60) * 60000LL);
    res.windows = in.windows;
    return res;
}

namespace {
bool TwelveHour()
{
    static int twelveHour = -1;
    if (twelveHour < 0) {
        wxString fmt = wxUILocale(wxUILocale::GetSystemLocaleId()).GetInfo(wxLOCALE_TIME_FMT);
        twelveHour = fmt.Contains("%p") || fmt.Contains("%I") ? 1 : 0;
    }
    return twelveHour == 1;
}
} // namespace

std::string FormatMinutes(int minutes)
{
    int m = ((minutes % 1440) + 1440) % 1440;
    int h = m / 60;
    if (!TwelveHour()) return wxString::Format("%d:%02d", h, m % 60).ToStdString();
    return wxString::Format("%d:%02d %s", h % 12 == 0 ? 12 : h % 12, m % 60, h < 12 ? "AM" : "PM").ToStdString();
}

std::string FormatTime(long long ms)
{
    long long s = ms / 1000 - (ms % 1000 < 0 ? 1 : 0);
    int d = (int)(((s % 86400) + 86400) % 86400);
    int h = d / 3600;
    int m = (d / 60) % 60;
    if (!TwelveHour()) return wxString::Format("%d:%02d:%02d", h, m, d % 60).ToStdString();
    return wxString::Format("%d:%02d:%02d %s", h % 12 == 0 ? 12 : h % 12, m, d % 60, h < 12 ? "AM" : "PM").ToStdString();
}

std::string FormatRange(int start, int end)
{
    return FormatMinutes(start) + " - " + FormatMinutes(end);
}

} // namespace ScheduleCalendar
