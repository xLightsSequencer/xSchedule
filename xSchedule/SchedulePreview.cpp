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

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <random>

namespace SchedulePreview {

namespace {

constexpr long long MINUTE = 60000;
constexpr long long FRAME = 50;       // a step lasts at least one frame
constexpr long long LATE = 60000;     // shorter delays are not findings
constexpr long long DARK = 120000;    // nor are shorter gaps
constexpr long long BLIP = 1000;      // a step suspended sooner than this is not shown
constexpr size_t MAX_PLAYS = 20000;

long long FloorMinute(long long ms)
{
    long long m = ms / MINUTE;
    if (ms % MINUTE != 0 && ms < 0) --m;
    return m * MINUTE;
}

bool SameName(const std::string& a, const std::string& b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower((unsigned char)x) == std::tolower((unsigned char)y); });
}

void AddNote(std::string& note, const std::string& text)
{
    if (text.empty()) return;
    note += (note.empty() ? "" : ". ") + text;
}

// what PlayList keeps while it plays
struct State {
    int pl = 0;
    int window = -1;
    bool called = false;
    int current = -1;
    long long remaining = 0;
    bool looping = false;
    bool lastLoop = false;
    bool random = false;
    bool noRandom = false;
    bool jumpToEnd = false;
    bool suspendAtEnd = false;
    bool restartOnResume = false;
    bool suspended = false;
    int loops = -1;
    int loopNumber = 1;
    std::vector<int> played;
    std::string pendingCall;
    int play = -1;    // the play shown for the current step, while it plays
    std::string note; // for the next play
};

// a RunningSchedule
struct Running {
    int schedule = 0;
    int window = 0;
    State pl;
    long long lastFired = 0;
    bool waiting = false;
    long long waitFrom = 0;
    int waitPlay = -1;
};

class Sim {
public:
    explicit Sim(const Input& in) : _in(in), _rng(in.seed) {}

    Plan Go(long long from, long long until)
    {
        _now = FloorMinute(from);
        long long lastEnd = LLONG_MIN;
        for (const auto& w : _in.windows) lastEnd = std::max(lastEnd, w.end);
        long long checked = LLONG_MIN;
        while (true) {
            if (_plan.plays.size() > MAX_PLAYS) {
                _plan.truncated = true;
                break;
            }
            if (FloorMinute(_now) == _now && checked != _now) {
                checked = _now;
                if (_now >= lastEnd && _active.empty() && !_immediate && _interrupted.empty() && _playNext < 0) break;
                CheckSchedule();
            }
            HandleCall();
            State* running = RunningState();
            long long next = FloorMinute(_now) + MINUTE;
            if (running != nullptr && _now + running->remaining <= next) {
                _now += running->remaining;
                running->remaining = 0;
                if (!MoveToNext(*running)) {
                    // a call from the step that just ended is dropped, as ScheduleManager::Frame does
                    running->pendingCall.clear();
                    if (!running->suspended) Stop(*running, false);
                    CheckSchedule();
                }
                continue;
            }
            if (running != nullptr) running->remaining -= next - _now;
            _now = next;
            if (_now > until) break;
        }
        for (auto& r : _active) Close(r->pl, Ending::Unfinished);
        if (_immediate) Close(*_immediate, Ending::Unfinished);
        for (auto& s : _interrupted) Close(*s, Ending::Unfinished);
        Finish(from, std::min(_now, until));
        return std::move(_plan);
    }

private:
    const PlayList& P(const State& s) const { return _in.playlists[s.pl]; }
    const Window& W(const Running& r) const { return _in.windows[r.window]; }
    bool IsRandom(const State& s) const { return !s.noRandom && (s.random || P(s).shuffle); }

    int FindStep(const State& s, const std::string& name) const
    {
        const auto& steps = P(s).steps;
        for (int i = 0; i < (int)steps.size(); ++i) {
            if (SameName(steps[i].name, name)) return i;
        }
        return -1;
    }

    int FindPlayList(const std::string& name) const
    {
        for (int i = 0; i < (int)_in.playlists.size(); ++i) {
            if (SameName(_in.playlists[i].name, name)) return i;
        }
        return -1;
    }

    bool SupportsRandom(const State& s) const
    {
        const auto& p = P(s);
        int n = (int)p.steps.size();
        int count = n;
        for (int i = 0; i < n; ++i) {
            if (i == 0 && p.firstOnce) --count;
            else if (i == n - 1 && p.lastOnce) --count;
            else if (p.steps[i].excludeFromRandom || p.steps[i].branch) --count;
        }
        return count > 3;
    }

    // PlayList::GetRandomStep
    int RandomStep(State& s, int depth = 0)
    {
        const auto& p = P(s);
        int n = (int)p.steps.size();
        int actual = n - (p.firstOnce ? 1 : 0) - (p.lastOnce ? 1 : 0);
        for (const auto& st : p.steps) {
            if (st.excludeFromRandom || st.branch) --actual;
        }
        actual = std::max(0, actual);
        int available = actual - (int)s.played.size();
        if (available <= 0) {
            s.played.clear();
            available = actual;
        }
        if (SupportsRandom(s) && available > 0) {
            _randomUsed = true;
            int selected = (int)(_rng() % (unsigned)available);
            int i = -1;
            int it = 0;
            do {
                const auto& st = p.steps[it];
                bool eligible = !st.excludeFromRandom && !st.branch && !(p.firstOnce && it == 0) && !(p.lastOnce && it == n - 1) &&
                                std::find(s.played.begin(), s.played.end(), it) == s.played.end();
                if (eligible) {
                    ++i;
                    if (i != selected) ++it;
                } else {
                    ++it;
                }
            } while (i != selected && it < n);
            if (it >= n) return -1;
            if (s.current >= 0 && it == s.current && depth < 8) return RandomStep(s, depth + 1);
            s.played.push_back(it);
            return it;
        }
        s.noRandom = true;
        bool didloop = false;
        int next = NextStep(s, didloop);
        s.noRandom = false;
        return next < 0 ? 0 : next;
    }

    // PlayList::GetNextStepIgnoringBranches
    int NextIgnoringBranches(State& s, bool& didloop)
    {
        didloop = false;
        if (s.current < 0) return -1;
        const auto& p = P(s);
        int n = (int)p.steps.size();
        int last = n - 1;
        if (p.lastOnce && s.current == last) return -1;
        if (IsRandom(s) && !s.lastLoop) return RandomStep(s);
        int idx = s.current + 1;
        if (s.jumpToEnd) {
            if (n == 1) return -1;
            return p.lastOnce ? last : -1;
        }
        if (s.looping && (idx == n || (p.lastOnce && idx == last)) && !s.lastLoop) {
            didloop = true;
            if (p.firstOnce) return n == 1 ? -1 : 1;
            return 0;
        }
        if (idx == n) return -1;
        return idx;
    }

    // PlayList::FollowBranches
    int FollowBranches(State& s, int step, bool& didloop)
    {
        const auto& p = P(s);
        int saved = s.current;
        int hops = 0;
        for (; step >= 0 && p.steps[step].branch && hops < 16; ++hops) {
            const Branch& b = *p.steps[step].branch;
            bool result = b.isTrue && b.isTrue(_now, s.loopNumber + (didloop ? 1 : 0));
            int target = -1;
            if (result && !b.truePlayList.empty()) {
                s.pendingCall = b.truePlayList;
            } else {
                const std::string& name = result ? b.trueStep : b.falseStep;
                if (!name.empty()) target = FindStep(s, name);
            }
            AddNote(s.note, "Branch " + p.steps[step].name + " is " + (result ? "true" : "false"));
            if (target < 0) {
                s.current = step;
                bool looped = false;
                target = NextIgnoringBranches(s, looped);
                didloop = didloop || looped;
            }
            step = target;
        }
        s.current = saved;
        if (step >= 0 && p.steps[step].branch) return -1;
        return step;
    }

    int NextStep(State& s, bool& didloop)
    {
        int next = NextIgnoringBranches(s, didloop);
        return FollowBranches(s, next, didloop);
    }

    void DoLoop(State& s)
    {
        ++s.loopNumber;
        if (s.loops <= 0) return;
        --s.loops;
        if (s.loops <= 1) {
            s.loops = -1;
            s.looping = false;
        }
    }

    void Open(State& s, long long offset)
    {
        Play p;
        p.start = _now;
        p.end = _now;
        p.window = s.window;
        p.playlist = s.pl;
        p.step = P(s).steps[s.current].name;
        p.pass = s.loopNumber;
        p.offset = offset;
        p.called = s.called;
        p.estimated = _randomUsed;
        p.ending = Ending::Unfinished;
        p.note = s.note;
        s.note.clear();
        s.play = (int)_plan.plays.size();
        _plan.plays.push_back(p);
    }

    void Close(State& s, Ending ending)
    {
        if (s.play < 0) return;
        _plan.plays[s.play].end = _now;
        _plan.plays[s.play].ending = ending;
        s.play = -1;
    }

    void BeginStep(State& s)
    {
        const auto& p = P(s);
        s.remaining = std::max(p.steps[s.current].lengthMS, FRAME);
        if (p.firstOnce && s.current == 0) AddNote(s.note, "First step, plays once");
        if (p.lastOnce && s.current == (int)p.steps.size() - 1) AddNote(s.note, "Last step, plays once");
        if (!s.suspended) Open(s, 0);
    }

    // PlayList::Start; StartSuspended when suspended is set
    void Start(State& s, bool loop, bool random, int loops, bool suspended)
    {
        if (s.current >= 0 || P(s).steps.empty()) return;
        s.played.clear();
        s.loopNumber = 1;
        s.pendingCall.clear();
        s.loops = loops;
        s.looping = loop;
        s.random = random;
        if (s.looping && s.loops == 1) {
            s.looping = false;
            s.loops = -1;
        }
        s.lastLoop = false;
        s.jumpToEnd = false;
        s.suspendAtEnd = false;
        s.restartOnResume = false;
        int first = IsRandom(s) && !P(s).firstOnce ? RandomStep(s) : 0;
        if (first >= 0 && P(s).steps[first].branch) {
            bool didloop = false;
            first = FollowBranches(s, first, didloop);
        }
        if (first < 0) return;
        s.current = first;
        if (suspended) s.suspended = true;
        BeginStep(s);
    }

    void Stop(State& s, bool cut)
    {
        if (s.current < 0) return;
        if (s.play >= 0 && cut) {
            auto& p = _plan.plays[s.play];
            AddNote(p.note, "Hard stop cuts this step with " + Duration(s.remaining) + " left");
            p.flag = Flag::Cut;
            Finding f;
            f.kind = Kind::CutOff;
            f.window = s.window;
            f.at = _now;
            f.amount = s.remaining;
            f.play = s.play;
            _plan.findings.push_back(f);
            Close(s, Ending::Cut);
        } else {
            Close(s, Ending::Finished);
        }
        s.current = -1;
    }

    // PlayList::Suspend
    void Suspend(State& s, bool suspend, const std::string& why)
    {
        if (s.current < 0) return;
        if (!suspend && s.suspended && s.restartOnResume) {
            s.suspended = false;
            s.restartOnResume = false;
            if (s.play < 0) AddNote(s.note, "Starts again from the beginning");
            BeginStep(s);
        } else if (!suspend && s.suspended) {
            s.suspended = false;
            long long length = std::max(P(s).steps[s.current].lengthMS, FRAME);
            long long offset = length - s.remaining;
            if (offset >= BLIP) AddNote(s.note, "Carries on " + Duration(offset) + " into the step");
            Open(s, offset);
        } else if (suspend && !s.suspended) {
            s.suspended = true;
            if (s.play >= 0) AddNote(_plan.plays[s.play].note, why);
            Close(s, Ending::Suspended);
        }
    }

    // the step ended: PlayList::MoveToNextStep, false when the playlist is done (or waits)
    bool MoveToNext(State& s)
    {
        Close(s, Ending::Finished);
        bool didloop = false;
        int next = NextStep(s, didloop);
        if (didloop) DoLoop(s);
        s.current = next;
        if (next < 0) return false;
        if (s.suspendAtEnd) {
            s.suspended = true;
            BeginStep(s);
            s.suspendAtEnd = false;
            s.restartOnResume = true;
            return false;
        }
        BeginStep(s);
        return true;
    }

    // ScheduleManager::GetRunningPlayList, without queued songs
    State* RunningState()
    {
        if (_immediate) return _immediate->current >= 0 ? _immediate.get() : nullptr;
        for (auto& r : _active) {
            if (r->pl.current >= 0 && !r->pl.suspended) return &r->pl;
        }
        return nullptr;
    }

    bool ScheduleActive(int schedule, long long t) const
    {
        for (const auto& w : _in.windows) {
            if (w.schedule == schedule && w.start <= t && t < w.end) return true;
        }
        return false;
    }

    bool Fires(const Window& w, long long lastFired) const
    {
        if (w.fireOnce) return true;
        return _in.shouldFire && _in.shouldFire(w.schedule, _now, lastFired);
    }

    bool ReleaseImmediate()
    {
        bool released = false;
        if (_immediate && _immediate->current < 0) {
            _immediate.reset();
            released = true;
        }
        if (!_immediate && !_interrupted.empty()) {
            _immediate = std::move(_interrupted.back());
            _interrupted.pop_back();
            Suspend(*_immediate, false, "");
            released = true;
        }
        return released;
    }

    // ScheduleManager::StartPlayNextIfReady and PlayPlayListThenResume
    void StartPlayNext()
    {
        if (_playNext < 0) return;
        State* running = RunningState();
        if (running != nullptr && !running->suspended) return;
        int p = _playNext;
        _playNext = -1;
        std::string why = "Stops here while " + _in.playlists[p].name + " plays";
        if (_immediate && _immediate->current >= 0) {
            if (!_immediate->suspended) Suspend(*_immediate, true, why);
            _interrupted.push_back(std::move(_immediate));
            if (_interrupted.size() > 10) {
                Close(*_interrupted.front(), Ending::Suspended);
                _interrupted.erase(_interrupted.begin());
            }
        }
        _immediate.reset();
        for (auto& r : _active) {
            if (!r->pl.suspended) Suspend(r->pl, true, why);
        }
        _immediate = std::make_unique<State>();
        _immediate->pl = p;
        _immediate->window = _playNextWindow;
        _immediate->called = true;
        _immediate->note = "Played by a branch in " + _playNextCaller;
        Start(*_immediate, false, false, -1, false);
    }

    // the running playlist's branch chose to play another playlist (ScheduleManager::Frame)
    void HandleCall()
    {
        State* running = RunningState();
        if (running == nullptr || running->pendingCall.empty()) return;
        std::string call = running->pendingCall;
        running->pendingCall.clear();
        int p = FindPlayList(call);
        if (p < 0 || p == running->pl) return;
        running->restartOnResume = true;
        Suspend(*running, true, "Stops here while " + _in.playlists[p].name + " plays");
        _playNext = p;
        _playNextWindow = running->window;
        _playNextCaller = P(*running).name;
        CheckSchedule();
    }

    // ScheduleManager::CheckSchedule
    void CheckSchedule()
    {
        ReleaseImmediate();
        StartPlayNext();

        for (int i = 0; i < (int)_in.windows.size(); ++i) {
            const auto& w = _in.windows[i];
            if (_now < w.start || _now >= w.end) continue;
            auto found = std::find_if(_active.begin(), _active.end(), [&w](const auto& r) { return r->schedule == w.schedule; });
            if (found != _active.end()) {
                // the same schedule's next window carries straight on, as an all day schedule does
                if ((*found)->window != i && _in.windows[(*found)->window].end <= _now) {
                    (*found)->window = i;
                    (*found)->pl.window = i;
                }
                continue;
            }
            // the scheduler asks the schedule itself, whose last fire time is from when the show loaded
            long long stale = _now - 2 * 60 * MINUTE;
            if (!Fires(w, stale)) continue;
            auto r = std::make_unique<Running>();
            r->schedule = w.schedule;
            r->window = i;
            r->lastFired = stale;
            r->pl.pl = w.playlist;
            r->pl.window = i;
            Start(r->pl, w.loop, w.random, w.loops, true);
            _active.push_back(std::move(r));
        }

        for (auto it = _active.begin(); it != _active.end();) {
            Running& r = **it;
            if (ScheduleActive(r.schedule, _now)) {
                ++it;
                continue;
            }
            const Window& w = W(r);
            if (r.pl.current < 0) {
                it = _active.erase(it);
                continue;
            }
            if (!r.pl.jumpToEnd) {
                if (w.hardStop) {
                    Stop(r.pl, !r.pl.suspended);
                    it = _active.erase(it);
                    continue;
                }
                if (w.stopAtEndOfLoop) {
                    r.pl.lastLoop = true;
                } else {
                    r.pl.jumpToEnd = true;
                    r.pl.lastLoop = true;
                }
            }
            ++it;
        }

        std::stable_sort(_active.begin(), _active.end(), [this](const auto& a, const auto& b) { return W(*a).priority > W(*b).priority; });

        if (_immediate) {
            for (auto& r : _active) {
                if (!r->pl.suspended) Suspend(r->pl, true, "Stops here while " + P(*_immediate).name + " plays");
            }
            return;
        }

        Running* toUnsuspend = nullptr;
        bool first = true;
        for (auto& rp : _active) {
            Running& r = *rp;
            const Window& w = W(r);
            if (first) {
                bool fire = !w.fireOnce && Fires(w, r.lastFired);
                if (r.pl.current < 0 && !fire) continue;
                first = false;
                State* actual = RunningState();
                if (actual != nullptr && actual != &r.pl && w.graceful) {
                    actual->suspendAtEnd = true;
                    if (!r.waiting) {
                        r.waiting = true;
                        r.waitFrom = _now;
                        r.waitPlay = actual->play;
                    }
                    if (r.pl.current < 0 && fire) Start(r.pl, false, false, -1, true);
                } else if (r.pl.current < 0 && fire) {
                    Start(r.pl, false, false, -1, true);
                    toUnsuspend = &r;
                } else if (r.pl.suspended) {
                    toUnsuspend = &r;
                }
            } else if (!r.pl.suspended && toUnsuspend != nullptr) {
                Suspend(r.pl, true, "Stops here for " + W(*toUnsuspend).name);
            }
        }
        if (toUnsuspend != nullptr) {
            Running& r = *toUnsuspend;
            long long delay = r.waiting ? _now - r.waitFrom : 0;
            if (r.waiting && delay >= 1000) {
                AddNote(r.pl.note, "Starts " + Duration(delay) + " late, waiting for a step to end");
                if (r.waitPlay >= 0) {
                    auto& waited = _plan.plays[r.waitPlay];
                    AddNote(waited.note, W(r).name + " is due and waits " + Duration(delay) + " for this step to end");
                    waited.flag = Flag::Wait;
                }
            }
            r.waiting = false;
            size_t before = _plan.plays.size();
            Suspend(r.pl, false, "");
            if (delay >= 1000 && _plan.plays.size() > before) {
                _plan.plays.back().flag = Flag::Wait;
                if (delay >= LATE) {
                    Finding f;
                    f.kind = Kind::StartsLate;
                    f.window = r.window;
                    f.at = r.waitFrom;
                    f.amount = delay;
                    f.play = (int)_plan.plays.size() - 1;
                    _plan.findings.push_back(f);
                }
            }
            r.lastFired = _now;
        }
    }

    long long LongestStep(const PlayList& p) const
    {
        long long longest = 0;
        for (const auto& s : p.steps) {
            if (!s.branch) longest = std::max(longest, s.lengthMS);
        }
        return longest + (p.lastOnce && !p.steps.empty() ? p.steps.back().lengthMS : 0);
    }

    void Finish(long long from, long long until)
    {
        // a step suspended as it started never really played
        std::vector<int> map(_plan.plays.size(), -1);
        std::vector<Play> kept;
        for (size_t i = 0; i < _plan.plays.size(); ++i) {
            const auto& p = _plan.plays[i];
            if (p.end - p.start < BLIP && p.ending == Ending::Suspended) continue;
            map[i] = (int)kept.size();
            kept.push_back(p);
        }
        _plan.plays = std::move(kept);
        for (auto& f : _plan.findings) f.play = f.play >= 0 ? map[f.play] : -1;

        for (int i = 0; i < (int)_in.windows.size(); ++i) {
            const auto& w = _in.windows[i];
            if (w.end > until || ScheduleActive(w.schedule, w.end)) continue;
            int lastPlay = -1;
            for (int j = 0; j < (int)_plan.plays.size(); ++j) {
                auto& p = _plan.plays[j];
                if (p.window != i || p.end <= w.end) continue;
                if (p.start < w.end && p.ending != Ending::Cut) AddNote(p.note, "The end time comes during this step, so it finishes");
                if (p.flag == Flag::None) p.flag = Flag::Late;
                if (lastPlay < 0 || p.end >= _plan.plays[lastPlay].end) lastPlay = j;
            }
            const auto& pl = _in.playlists[w.playlist];
            bool shuffled = (w.random || pl.shuffle) && !w.hardStop;
            long long amount = lastPlay >= 0 ? _plan.plays[lastPlay].end - w.end : 0;
            long long bound = shuffled ? LongestStep(pl) : 0;
            if (std::max(amount, bound) < LATE || (amount <= 0 && !shuffled)) continue;
            bool anyPlay = std::any_of(_plan.plays.begin(), _plan.plays.end(), [i](const Play& p) { return p.window == i; });
            if (!anyPlay) continue;
            Finding f;
            f.kind = Kind::RunsLate;
            f.window = i;
            f.at = w.end;
            f.amount = std::max(amount, bound);
            f.upTo = bound > amount;
            f.play = lastPlay;
            _plan.findings.push_back(f);
        }

        // gaps inside a competing window where nothing plays
        std::vector<std::pair<long long, long long>> lit;
        for (const auto& p : _plan.plays) {
            if (p.end > p.start) lit.push_back({ p.start, p.end });
        }
        std::sort(lit.begin(), lit.end());
        std::vector<std::pair<long long, long long>> windows;
        for (const auto& w : _in.windows) windows.push_back({ std::max(w.start, from), std::min(w.end, until) });
        std::sort(windows.begin(), windows.end());
        std::vector<std::pair<long long, long long>> open;
        for (const auto& w : windows) {
            if (w.second <= w.first) continue;
            if (!open.empty() && w.first <= open.back().second) open.back().second = std::max(open.back().second, w.second);
            else open.push_back(w);
        }
        for (const auto& o : open) {
            long long t = o.first;
            for (const auto& l : lit) {
                if (l.second <= t) continue;
                if (l.first >= o.second) break;
                if (l.first > t) AddDark(t, l.first);
                t = std::max(t, l.second);
                if (t >= o.second) break;
            }
            if (t < o.second) AddDark(t, o.second);
        }

        std::stable_sort(_plan.findings.begin(), _plan.findings.end(), [](const Finding& a, const Finding& b) { return a.at < b.at; });
    }

    void AddDark(long long start, long long end)
    {
        if (end - start < DARK) return;
        int best = -1;
        for (int i = 0; i < (int)_in.windows.size(); ++i) {
            const auto& w = _in.windows[i];
            if (w.start > start || start >= w.end) continue;
            if (best < 0 || w.priority > _in.windows[best].priority) best = i;
        }
        Finding f;
        f.kind = Kind::GoesDark;
        f.window = best;
        f.at = start;
        f.amount = end - start;
        _plan.findings.push_back(f);
    }

    const Input& _in;
    std::mt19937 _rng;
    Plan _plan;
    long long _now = 0;
    bool _randomUsed = false;
    std::vector<std::unique_ptr<Running>> _active;
    std::unique_ptr<State> _immediate;
    std::vector<std::unique_ptr<State>> _interrupted;
    int _playNext = -1;
    int _playNextWindow = -1;
    std::string _playNextCaller;
};

} // namespace

Plan Run(const Input& input, long long from, long long until)
{
    Sim sim(input);
    return sim.Go(from, until);
}

std::string Duration(long long ms)
{
    long long s = (std::max(0LL, ms) + 500) / 1000;
    char buf[32];
    if (s >= 3600) snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
    else snprintf(buf, sizeof(buf), "%lld:%02lld", s / 60, s % 60);
    return buf;
}

std::string Describe(const Finding& f, const std::vector<Window>& windows)
{
    std::string name = f.window >= 0 && f.window < (int)windows.size() ? windows[f.window].name : "The show";
    switch (f.kind) {
    case Kind::RunsLate:
        return name + (f.upTo ? " can run up to " : " runs ") + Duration(f.amount) + " past its end";
    case Kind::StartsLate:
        return name + " starts " + Duration(f.amount) + " late";
    case Kind::CutOff:
        return name + " cuts a step with " + Duration(f.amount) + " left";
    case Kind::GoesDark:
        return "Nothing plays for " + Duration(f.amount) + " while " + name + " is scheduled";
    }
    return "";
}

} // namespace SchedulePreview
