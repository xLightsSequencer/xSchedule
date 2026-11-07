/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "CalendarPanel.h"

#include "DateField.h"
#include "Holidays.h"
#include "ModernUI.h"
#include "PlayList/PlayList.h"
#include "Schedule.h"
#include "ScheduleManager.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/graphics.h>
#include <wx/bmpbuttn.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/simplebook.h>
#include <wx/stattext.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>

namespace {

struct Colors {
    wxColour bar;
    wxColour soft;
    wxColour text;
};

// the same eight colors as the web calendar, by the schedule's place in the list
Colors PaletteFor(int index)
{
    static const char* light[8][3] = {
        { "#2563c9", "#e6eefb", "#1f56b0" }, { "#0f766e", "#ddf2ef", "#0b5d57" }, { "#7c3aed", "#efe7fd", "#5b21b6" },
        { "#c2410c", "#fdebdd", "#9a3412" }, { "#be185d", "#fce7f1", "#9d174d" }, { "#334155", "#e5e8ec", "#1e293b" },
        { "#15803d", "#dcf2e3", "#14532d" }, { "#92400e", "#f6e9d9", "#78350f" }
    };
    static const char* dark[8][3] = {
        { "#6ea8fe", "#22344f", "#a9cbff" }, { "#4fd1c5", "#17332f", "#8de5db" }, { "#b794f6", "#2e2445", "#cdb5fa" },
        { "#fb923c", "#3b2616", "#fdba74" }, { "#f472b6", "#3a1e2c", "#f9a8d4" }, { "#94a3b8", "#2c323a", "#cbd5e1" },
        { "#4ade80", "#173322", "#86efac" }, { "#f59e0b", "#3a2c14", "#fcd34d" }
    };
    const auto& p = ModernUI::GetTheme().dark ? dark[index % 8] : light[index % 8];
    return { wxColour(p[0]), wxColour(p[1]), wxColour(p[2]) };
}

Colors PaletteFor(const ScheduleCalendar::Window& w)
{
    return PaletteFor(std::max(0, w.order - 1));
}

wxColour OnBar()
{
    return ModernUI::GetTheme().dark ? wxColour(16, 21, 28) : *wxWHITE;
}

wxColour Mix(const wxColour& a, const wxColour& b, double t)
{
    return wxColour((unsigned char)(a.Red() + (b.Red() - a.Red()) * t), (unsigned char)(a.Green() + (b.Green() - a.Green()) * t),
                    (unsigned char)(a.Blue() + (b.Blue() - a.Blue()) * t));
}

long DayNumber(const wxDateTime& d)
{
    return std::lround(d.GetDateOnly().GetJDN());
}

// the computer's first day of the week: Monday in much of the world, Sunday in the US
int FirstWeekDay()
{
    wxDateTime::WeekDay day;
    return wxDateTime::GetFirstWeekDay(&day) ? (int)day : 0;
}

wxString LongDate(const wxDateTime& d)
{
    return d.Format("%A, %B ") + wxString::Format("%d", d.GetDay());
}

wxString ShortDate(const wxDateTime& d)
{
    return d.Format("%a, %b ") + wxString::Format("%d", d.GetDay());
}

void Text(wxGraphicsContext* gc, wxDC& dc, const wxFont& font, const wxColour& colour, const wxString& text, double x, double y, double maxWidth)
{
    if (maxWidth <= 4) return;
    dc.SetFont(font);
    wxString shown = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END, (int)maxWidth);
    gc->SetFont(font, colour);
    gc->DrawText(shown, x, y);
}

double TextHeight(wxGraphicsContext* gc, const wxFont& font)
{
    double w, h;
    gc->SetFont(font, *wxBLACK);
    gc->GetTextExtent("Ag", &w, &h);
    return h;
}

double TextWidth(wxGraphicsContext* gc, const wxFont& font, const wxString& text)
{
    double w, h;
    gc->SetFont(font, *wxBLACK);
    gc->GetTextExtent(text, &w, &h);
    return w;
}

void Fill(wxGraphicsContext* gc, const wxColour& fill, const wxPen& pen, double x, double y, double w, double h, double r)
{
    gc->SetBrush(fill.IsOk() ? wxBrush(fill) : *wxTRANSPARENT_BRUSH);
    gc->SetPen(pen);
    gc->DrawRoundedRectangle(x, y, w, h, r);
}

void Hatch(wxGraphicsContext* gc, const wxColour& colour, double x, double y, double w, double h, double step)
{
    gc->Clip(x, y, w, h);
    gc->SetPen(wxPen(colour, std::max(1, (int)(step / 2.5))));
    for (double sx = x - h; sx < x + w; sx += step) {
        gc->StrokeLine(sx, y + h, sx + h, y);
    }
    gc->ResetClip();
}

wxString StateText(const ScheduleCalendar::Detail& d, const ScheduleCalendar::Segment& g)
{
    if (g.state == ScheduleCalendar::State::Plays) return ScheduleCalendar::FormatRange(g.start, g.end);
    if (g.state == ScheduleCalendar::State::Done) return "Playlist finished";
    wxString by = g.by >= 0 ? wxString(d.windows[g.by].schedule->GetName()) : wxString();
    return (g.state == ScheduleCalendar::State::Hidden ? "Waiting: " : "Clash with ") + by;
}

int MinuteOf(long long ms)
{
    return (int)std::floor(ms / 60000.0);
}

wxString FindingText(const SchedulePreview::Finding& f, const ScheduleCalendar::Preview& p)
{
    using SchedulePreview::Kind;
    wxString name = f.window >= 0 ? wxString::FromUTF8(p.windows[f.window].name) : wxString("The show");
    wxString amount = SchedulePreview::Duration(f.amount);
    switch (f.kind) {
    case Kind::RunsLate:
        return name + (f.upTo ? " can run up to " : " runs ") + amount + " past " + ScheduleCalendar::FormatMinutes(MinuteOf(f.at));
    case Kind::StartsLate:
        return name + " starts " + amount + " late";
    case Kind::CutOff:
        return name + " cuts a step at " + ScheduleCalendar::FormatMinutes(MinuteOf(f.at));
    case Kind::GoesDark:
        return "Nothing plays " + ScheduleCalendar::FormatRange(MinuteOf(f.at), MinuteOf(f.at + f.amount));
    }
    return "";
}

wxString KindName(SchedulePreview::Kind kind)
{
    switch (kind) {
    case SchedulePreview::Kind::RunsLate: return "Runs late";
    case SchedulePreview::Kind::StartsLate: return "Starts late";
    case SchedulePreview::Kind::CutOff: return "Cut off";
    case SchedulePreview::Kind::GoesDark: return "Goes dark";
    }
    return "";
}

wxColour FindingColor(SchedulePreview::Kind kind)
{
    const auto& t = ModernUI::GetTheme();
    return kind == SchedulePreview::Kind::CutOff || kind == SchedulePreview::Kind::GoesDark ? t.badText : t.waitText;
}

// where a finding shows on the timeline: the moment it is seen on the lights
long long FindingTime(const SchedulePreview::Finding& f, const ScheduleCalendar::Preview& p)
{
    if (f.play >= 0 && f.kind == SchedulePreview::Kind::RunsLate) return p.plan.plays[f.play].end;
    if (f.play >= 0 && f.kind == SchedulePreview::Kind::StartsLate) return p.plan.plays[f.play].start;
    return f.at;
}

void Marker(wxGraphicsContext* gc, const wxColour& color, double x, double y, double size)
{
    wxGraphicsPath path = gc->CreatePath();
    path.MoveToPoint(x - size, y);
    path.AddLineToPoint(x + size, y);
    path.AddLineToPoint(x, y + size * 1.4);
    path.CloseSubpath();
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(color));
    gc->FillPath(path);
}

} // namespace

// a month of nights, one cell each: click a night to choose it, double-click it to open it, double-click a schedule
// to edit it
class CalendarMonth : public wxWindow {
public:
    CalendarMonth(wxWindow* parent, std::function<void(const wxDateTime&)> select, std::function<void(const wxDateTime&)> open, CalendarPanel::EditSchedule edit,
                  std::function<void(const wxDateTime&)> menu) :
        wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE), _select(select), _open(open), _edit(edit), _menu(menu) {
        SetName("Calendar month");
        Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent& e) {
            int c = CellAt(e.GetPosition());
            if (c >= 0) _menu(_nights[c].date);
        });
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(500, 150)));
        Bind(wxEVT_PAINT, &CalendarMonth::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, &CalendarMonth::OnClick, this);
        Bind(wxEVT_LEFT_DCLICK, &CalendarMonth::OnDoubleClick, this);
        Bind(wxEVT_MOTION, &CalendarMonth::OnMotion, this);
    }

    void Set(const std::vector<ScheduleCalendar::Night>& nights, const wxDateTime& month, const wxDateTime& selected) {
        _nights = nights;
        _month = month;
        _selected = selected;
        Refresh();
    }

private:
    struct Hit {
        wxRect rect;
        int night = 0;
        Schedule* schedule = nullptr;
        wxString tip;
    };

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const auto& t = ModernUI::GetTheme();
        dc.SetBackground(wxBrush(t.panel));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc || _nights.empty()) return;
        _hits.clear();
        _cells.clear();

        wxFont base = GetFont();
        wxFont bold = base.Bold();
        wxFont smallFont = base.Scaled(0.88f);
        wxFont smallBold = smallFont.Bold();
        const wxSize size = GetClientSize();
        const double gap = FromDIP(4);
        const double headH = TextHeight(gc.get(), smallFont) + FromDIP(6);
        const int rows = (int)(_nights.size() / 7);
        const double cellW = (size.x - gap * 6) / 7.0;
        const double cellH = (size.y - headH - gap * (rows - 1)) / rows;
        const double chipH = TextHeight(gc.get(), smallFont) + FromDIP(3);
        const double chipGap = FromDIP(1);
        // one look for the whole month: chips when two fit in a night, otherwise rows of colored bars
        const double chipRoom = cellH - FromDIP(5) * 2 - TextHeight(gc.get(), bold) - FromDIP(3);
        const bool compact = chipRoom < chipH * 2 + chipGap;
        const double pad = FromDIP(5);

        for (int c = 0; c < 7; ++c) {
            wxString name = wxDateTime::GetWeekDayName((wxDateTime::WeekDay)((FirstWeekDay() + c) % 7), wxDateTime::Name_Abbr);
            Text(gc.get(), dc, smallBold, t.muted, name, c * (cellW + gap) + pad, 0, cellW - pad);
        }

        const long today = DayNumber(wxDateTime::Today());
        for (int i = 0; i < (int)_nights.size(); ++i) {
            const auto& n = _nights[i];
            const double x = (i % 7) * (cellW + gap);
            const double y = headH + (i / 7) * (cellH + gap);
            _cells.push_back(wxRect((int)x, (int)y, (int)cellW, (int)cellH));
            const bool inMonth = n.date.GetMonth() == _month.GetMonth();
            const bool selected = DayNumber(n.date) == DayNumber(_selected);
            wxColour bg = !n.holiday.empty() ? t.waitBg : inMonth ? t.panel : Mix(t.panel, t.toolbar, t.dark ? 0.6 : 1.0);
            Fill(gc.get(), bg, selected ? wxPen(t.accent, FromDIP(2)) : wxPen(t.line), x + 0.5, y + 0.5, cellW - 1, cellH - 1, FromDIP(4));

            double cy = y + pad - FromDIP(1);
            wxString num = wxString::Format("%d", n.date.GetDay());
            double numW = TextWidth(gc.get(), bold, num);
            double numH = TextHeight(gc.get(), bold);
            if (DayNumber(n.date) == today) {
                double d = numH + FromDIP(2);
                double w = std::max(d, numW + FromDIP(8));
                Fill(gc.get(), t.accent, *wxTRANSPARENT_PEN, x + pad - FromDIP(2), cy - FromDIP(1), w, d, d / 2);
                Text(gc.get(), dc, bold, t.onAccent, num, x + pad - FromDIP(2) + (w - numW) / 2, cy, w);
                numW = w - FromDIP(4);
            } else {
                Text(gc.get(), dc, bold, inMonth ? t.text : t.muted, num, x + pad, cy, cellW);
            }
            double right = x + cellW - pad;
            if (!ScheduleCalendar::ClashesOf(n.windows).empty()) {
                wxString c = "Clash";
                double cw = TextWidth(gc.get(), smallBold, c) + FromDIP(10);
                double ch = TextHeight(gc.get(), smallBold) + FromDIP(1);
                Fill(gc.get(), wxNullColour, wxPen(t.badText, FromDIP(1)), right - cw, cy, cw, ch, ch / 2);
                Text(gc.get(), dc, smallBold, t.badText, c, right - cw + FromDIP(5), cy + FromDIP(0.5), cw);
                right -= cw + FromDIP(4);
            }
            if (!n.holiday.empty()) {
                double hx = x + pad + numW + FromDIP(8);
                Text(gc.get(), dc, smallBold, t.waitText, n.holiday, hx, cy + (numH - TextHeight(gc.get(), smallBold)) / 2, right - hx);
            }

            cy += numH + FromDIP(3);
            int fits = std::max(0, (int)((y + cellH - pad - cy + chipGap) / (chipH + chipGap)));
            int need = (int)n.windows.size();
            if (compact) {
                // too short for chips: the night's schedules side by side in one row of their colors, details in tooltips
                if (need == 0) continue;
                const double barH = std::min<double>(FromDIP(8), std::max(3.0, y + cellH - cy - FromDIP(4)));
                const double segGap = FromDIP(2);
                const double segW = (cellW - pad * 2 - segGap * (need - 1)) / need;
                double sx = x + pad;
                for (const auto& win : n.windows) {
                    Colors col = PaletteFor(win);
                    if (win.Competes()) Fill(gc.get(), col.bar, *wxTRANSPARENT_PEN, sx, cy, segW, barH, barH / 2);
                    else Fill(gc.get(), wxNullColour, wxPen(t.muted, 1, wxPENSTYLE_SHORT_DASH), sx, cy, segW, barH, barH / 2);
                    wxString tip = win.schedule->GetName() + ", " + ScheduleCalendar::FormatRange(win.start, win.end) + (win.skipped ? "\nSkipped this night" : "") +
                                   (!win.enabled ? "\nInactive" : "") + "\nDouble-click to edit";
                    _hits.push_back({ wxRect((int)sx, (int)cy - 2, (int)segW, (int)barH + 4), i, win.schedule, tip });
                    sx += segW + segGap;
                }
                continue;
            }
            int shown = need <= fits ? need : std::max(0, fits - 1);
            for (int w = 0; w < shown; ++w) {
                const auto& win = n.windows[w];
                Colors col = PaletteFor(win);
                double cw = cellW - pad * 2;
                bool dashed = win.skipped || !win.enabled;
                if (dashed) {
                    Fill(gc.get(), wxNullColour, wxPen(t.muted, 1, wxPENSTYLE_SHORT_DASH), x + pad, cy, cw, chipH, FromDIP(3));
                } else {
                    Fill(gc.get(), col.soft, *wxTRANSPARENT_PEN, x + pad, cy, cw, chipH, FromDIP(3));
                    Fill(gc.get(), col.bar, *wxTRANSPARENT_PEN, x + pad, cy, FromDIP(3), chipH, 0);
                }
                wxString time = ScheduleCalendar::FormatMinutes(win.start);
                double tw = TextWidth(gc.get(), smallBold, time);
                double ty = cy + (chipH - TextHeight(gc.get(), smallFont)) / 2;
                wxColour tc = win.enabled ? col.text : t.muted;
                Text(gc.get(), dc, smallBold, tc, time, x + pad + FromDIP(6), ty, cw - FromDIP(8));
                wxFont nameFont = win.skipped ? smallFont.Strikethrough() : smallFont;
                Text(gc.get(), dc, nameFont, tc, win.schedule->GetName(), x + pad + FromDIP(10) + tw, ty, cw - tw - FromDIP(14));
                wxString tip = win.schedule->GetName() + ", " + ScheduleCalendar::FormatRange(win.start, win.end) + "\nPlaylist " + win.playlist->GetNameNoTime() +
                               ", priority " + wxString::Format("%d", win.schedule->GetPriority()) + (win.skipped ? "\nSkipped this night" : "") + (!win.enabled ? "\nInactive" : "") +
                               "\nDouble-click to edit";
                _hits.push_back({ wxRect((int)(x + pad), (int)cy, (int)cw, (int)chipH), i, win.schedule, tip });
                cy += chipH + chipGap;
            }
            if (shown < (int)n.windows.size()) {
                Text(gc.get(), dc, smallFont, t.muted, wxString::Format("+%d more", (int)n.windows.size() - shown), x + pad + FromDIP(4), cy, cellW - pad * 2);
            }
        }
    }

    int CellAt(const wxPoint& p) const {
        for (size_t i = 0; i < _cells.size(); ++i) {
            if (_cells[i].Contains(p)) return (int)i;
        }
        return -1;
    }

    void OnClick(wxMouseEvent& event) {
        SetFocus();
        int c = CellAt(event.GetPosition());
        if (c >= 0) _select(_nights[c].date);
    }

    void OnDoubleClick(wxMouseEvent& event) {
        for (const auto& h : _hits) {
            if (h.rect.Contains(event.GetPosition())) {
                _edit(h.schedule);
                return;
            }
        }
        int c = CellAt(event.GetPosition());
        if (c >= 0) _open(_nights[c].date);
    }

    void OnMotion(wxMouseEvent& event) {
        wxString tip;
        for (const auto& h : _hits) {
            if (h.rect.Contains(event.GetPosition())) tip = h.tip;
        }
        if (tip != _tip) {
            _tip = tip;
            if (tip.empty()) UnsetToolTip(); else SetToolTip(tip);
        }
    }

    std::function<void(const wxDateTime&)> _select;
    std::function<void(const wxDateTime&)> _open;
    CalendarPanel::EditSchedule _edit;
    std::function<void(const wxDateTime&)> _menu;
    std::vector<ScheduleCalendar::Night> _nights;
    wxDateTime _month;
    wxDateTime _selected;
    std::vector<Hit> _hits;
    std::vector<wxRect> _cells;
    wxString _tip;
};

// the chosen night as a strip: what is on the lights, then a row per schedule
class CalendarDay : public wxWindow {
public:
    CalendarDay(wxWindow* parent, CalendarPanel::EditSchedule edit, std::function<void(const wxDateTime&)> menu, std::function<void(int)> finding) :
        wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE), _edit(edit), _finding(finding) {
        SetName("Calendar night");
        Bind(wxEVT_RIGHT_UP, [this, menu](wxMouseEvent&) {
            if (_has) menu(_detail.night.date);
        });
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
            SetFocus();
            for (const auto& c : _chips) {
                if (c.first.Contains(e.GetPosition())) {
                    _finding(c.second);
                    return;
                }
            }
            e.Skip();
        });
        SetBackgroundColour(ModernUI::GetTheme().panel);
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(500, 150)));
        Bind(wxEVT_PAINT, &CalendarDay::OnPaint, this);
        Bind(wxEVT_LEFT_DCLICK, &CalendarDay::OnDoubleClick, this);
        Bind(wxEVT_MOTION, &CalendarDay::OnMotion, this);
    }

    void Set(const ScheduleCalendar::Detail& detail, const ScheduleCalendar::Preview& preview) {
        _detail = detail;
        _preview = preview;
        _has = true;
        Refresh();
    }

    void Clear() {
        _detail = ScheduleCalendar::Detail();
        _preview = ScheduleCalendar::Preview();
        _has = false;
        Refresh();
    }

    // just the night's title and findings, above the play-by-play list
    void SetHeaderOnly(bool headerOnly) {
        _headerOnly = headerOnly;
        Refresh();
    }

    int HeaderHeight(int width) {
        wxClientDC dc(this);
        dc.SetFont(GetFont());
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc || !_has) return FromDIP(40);
        return (int)std::ceil(PaintHeader(gc.get(), dc, width));
    }

private:
    struct Hit {
        wxRect rect;
        Schedule* schedule = nullptr;
        wxString tip;
    };

    // the title row and the night's findings; returns where the timeline starts
    double PaintHeader(wxGraphicsContext* gc, wxDC& dc, double width) {
        const auto& t = ModernUI::GetTheme();
        const auto& d = _detail;
        wxFont base = GetFont();
        wxFont title = base.Scaled(1.15f).Bold();
        wxFont smallFont = base.Scaled(0.88f);
        wxFont smallBold = smallFont.Bold();
        const wxSize size((int)width, 0);
        _chips.clear();

        double y = FromDIP(8);
        wxString heading = LongDate(d.night.date);
        Text(gc, dc, title, t.text, heading, 0, y, size.x);
        double hx = TextWidth(gc, title, heading) + FromDIP(10);
        double titleH = TextHeight(gc, title);
        if (!d.night.holiday.empty()) {
            double pw = TextWidth(gc, smallBold, d.night.holiday) + FromDIP(16);
            double ph = TextHeight(gc, smallBold) + FromDIP(4);
            Fill(gc, t.waitBg, *wxTRANSPARENT_PEN, hx, y + (titleH - ph) / 2, pw, ph, ph / 2);
            Text(gc, dc, smallBold, t.waitText, d.night.holiday, hx + FromDIP(8), y + (titleH - ph) / 2 + FromDIP(2), pw);
            hx += pw + FromDIP(10);
        }
        wxString sun = "Sunset " + ScheduleCalendar::FormatMinutes(d.night.sunset) + wxString::FromUTF8("  \xC2\xB7  Dusk ") + ScheduleCalendar::FormatMinutes(d.night.dusk);
        Text(gc, dc, base, t.muted, sun, hx, y + (titleH - TextHeight(gc, base)) / 2, size.x - hx);
        wxString hint = _headerOnly ? "Click a finding to find its step" : "Double-click a bar to edit its schedule";
        double hintW = TextWidth(gc, smallFont, hint);
        if (hx + TextWidth(gc, base, sun) + hintW + FromDIP(30) < size.x) {
            Text(gc, dc, smallFont, t.muted, hint, size.x - hintW - FromDIP(4), y + (titleH - TextHeight(gc, smallFont)) / 2, hintW + 2);
        }
        y += titleH + FromDIP(12);

        // the findings, as chips that wrap
        const double chipH = TextHeight(gc, smallFont) + FromDIP(8);
        double cx = 0;
        bool any = false;
        for (int i = 0; i < (int)_preview.plan.findings.size(); ++i) {
            const auto& f = _preview.plan.findings[i];
            long long when = FindingTime(f, _preview);
            if (when < d.axisStart * 60000LL || when > d.axisEnd * 60000LL) continue;
            wxString text = FindingText(f, _preview);
            double cw = TextWidth(gc, smallFont, text) + FromDIP(30);
            if (cx > 0 && cx + cw > size.x) {
                cx = 0;
                y += chipH + FromDIP(6);
            }
            Fill(gc, t.panel, wxPen(t.line), cx + 0.5, y + 0.5, cw - 1, chipH - 1, chipH / 2);
            Marker(gc, FindingColor(f.kind), cx + FromDIP(12), y + chipH / 2 - FromDIP(3), FromDIP(4));
            Text(gc, dc, smallFont, t.text, text, cx + FromDIP(21), y + FromDIP(4), cw - FromDIP(24));
            _chips.push_back({ wxRect((int)cx, (int)y, (int)cw, (int)chipH), i });
            cx += cw + FromDIP(6);
            any = true;
        }
        if (any) y += chipH + FromDIP(10);
        return y;
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        const auto& t = ModernUI::GetTheme();
        dc.SetBackground(wxBrush(t.panel));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc || !_has) return;
        _hits.clear();

        const auto& d = _detail;
        wxFont base = GetFont();
        wxFont bold = base.Bold();
        wxFont smallFont = base.Scaled(0.88f);
        wxFont smallBold = smallFont.Bold();
        const wxSize size = GetClientSize();
        const double labelW = FromDIP(210);
        const double x0 = labelW + FromDIP(10);
        const double trackW = std::max(10.0, size.x - x0 - FromDIP(4));
        const double span = std::max(1, d.axisEnd - d.axisStart);
        auto X = [&](int m) { return x0 + trackW * std::clamp((m - d.axisStart) / span, 0.0, 1.0); };
        auto XMS = [&](long long ms) { return x0 + trackW * std::clamp((ms / 60000.0 - d.axisStart) / span, 0.0, 1.0); };

        double y = PaintHeader(gc.get(), dc, size.x);
        if (_headerOnly) return;

        if (d.windows.empty()) {
            Text(gc.get(), dc, base, t.muted, "Nothing is scheduled this night.", 0, y, size.x);
            return;
        }

        const double smallH = TextHeight(gc.get(), smallFont);
        for (int m = (d.axisStart + 59) / 60 * 60; m <= d.axisEnd; m += 60) {
            wxString label = ScheduleCalendar::FormatMinutes(m);
            if (label.EndsWith(":00 AM") || label.EndsWith(":00 PM")) label = label.Left(label.length() - 6) + label.Right(3);
            double lw = TextWidth(gc.get(), smallFont, label);
            double lx = std::clamp(X(m) - lw / 2, x0, x0 + trackW - lw);
            Text(gc.get(), dc, smallFont, t.muted, label, lx, y, lw + 2);
            gc->SetPen(wxPen(t.line));
            gc->StrokeLine(X(m), y + smallH + FromDIP(2), X(m), size.y);
        }
        y += smallH + FromDIP(6);

        const double barH = FromDIP(20);
        const double rowH = barH + FromDIP(2);
        Text(gc.get(), dc, bold, t.text, "On the lights", 0, y + (barH - TextHeight(gc.get(), bold)) / 2, labelW);
        Fill(gc.get(), t.track, *wxTRANSPARENT_PEN, x0, y, trackW, barH, FromDIP(4));
        for (const auto& g : d.onTheLights) {
            const auto& w = d.windows[g.by];
            Colors col = PaletteFor(w);
            double sx = X(g.start);
            double sw = X(g.end) - sx;
            Fill(gc.get(), col.bar, *wxTRANSPARENT_PEN, sx, y, sw, barH, 0);
            Text(gc.get(), dc, smallBold, OnBar(), w.schedule->GetName(), sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
            _hits.push_back({ wxRect((int)sx, (int)y, (int)sw, (int)barH), w.schedule, w.schedule->GetName() + ", " + ScheduleCalendar::FormatRange(g.start, g.end) });
        }
        const wxDateTime now = wxDateTime::Now();
        if (DayNumber(now) == DayNumber(d.night.date)) {
            int nm = now.GetHour() * 60 + now.GetMinute();
            if (nm >= d.axisStart && nm <= d.axisEnd) {
                gc->SetPen(wxPen(t.badText, FromDIP(2)));
                gc->StrokeLine(X(nm), y - FromDIP(3), X(nm), y + barH + FromDIP(3));
            }
        }
        y += rowH + FromDIP(6);

        // every step that plays, in its schedule's color, with the findings marked above
        const auto& plays = _preview.plan.plays;
        if (!plays.empty()) {
            y += FromDIP(4);
            Text(gc.get(), dc, bold, t.text, "Songs", 0, y + (barH - TextHeight(gc.get(), bold)) / 2, labelW);
            wxString count = wxString::Format("%d steps", (int)plays.size());
            double countW = TextWidth(gc.get(), smallFont, count);
            Text(gc.get(), dc, smallFont, t.muted, count, labelW - countW, y + (barH - smallH) / 2, countW + 2);
            Fill(gc.get(), t.track, *wxTRANSPARENT_PEN, x0, y, trackW, barH, FromDIP(4));
            std::vector<int> shade(d.windows.size(), 0);
            for (const auto& p : plays) {
                if (p.window < 0 || p.end <= p.start) continue;
                int dwi = _preview.detailWindow[p.window];
                const auto& w = d.windows[dwi];
                Colors col = PaletteFor(w);
                double sx = XMS(p.start);
                double ex = XMS(p.end);
                if (ex - sx < 1) ex = sx + 1;
                wxColour fill = shade[dwi]++ % 2 == 0 ? col.bar : Mix(col.bar, t.panel, 0.35);
                if (p.called) Fill(gc.get(), col.soft, wxPen(col.bar), sx, y + 0.5, ex - sx, barH - 1, 0);
                else Fill(gc.get(), fill, *wxTRANSPARENT_PEN, sx, y, ex - sx, barH, 0);
                wxString tip = wxString::FromUTF8(p.step) + "\n" + w.schedule->GetName() + (p.called ? ", playlist " + wxString::FromUTF8(_preview.playlists[p.playlist]) : wxString()) + "\n" +
                               ScheduleCalendar::FormatTime(p.start) + " - " + ScheduleCalendar::FormatTime(p.end) + " (" + SchedulePreview::Duration(p.end - p.start) + ")" +
                               (p.note.empty() ? wxString() : "\n" + wxString::FromUTF8(p.note)) + (p.estimated ? "\nShuffled: one possible order" : "");
                _hits.push_back({ wxRect((int)sx, (int)y, std::max(1, (int)(ex - sx)), (int)barH), w.schedule, tip });
            }
            for (const auto& f : _preview.plan.findings) {
                long long when = FindingTime(f, _preview);
                if (when < d.axisStart * 60000LL || when > d.axisEnd * 60000LL) continue;
                double mx = XMS(when);
                Marker(gc.get(), FindingColor(f.kind), mx, y - FromDIP(7), FromDIP(4));
                Schedule* s = f.window >= 0 ? d.windows[_preview.detailWindow[f.window]].schedule : nullptr;
                _hits.push_back({ wxRect((int)mx - FromDIP(5), (int)y - FromDIP(8), FromDIP(10), FromDIP(8)), s, FindingText(f, _preview) });
            }
            y += rowH + FromDIP(8);
        }

        const double legendH = TextHeight(gc.get(), smallFont) + FromDIP(10);
        int shownLanes = 0;
        for (const auto& lane : d.lanes) {
            if (y + rowH > size.y) break;
            const auto& w = d.windows[lane.window];
            Colors col = PaletteFor(w);
            ++shownLanes;
            double r = FromDIP(4);
            Fill(gc.get(), col.bar, *wxTRANSPARENT_PEN, FromDIP(2), y + barH / 2 - r, r * 2, r * 2, r);
            wxString from = DayNumber(w.night) != DayNumber(d.night.date) ? " (" + w.night.Format("%a") + ")" : "";
            wxString pri = wxString::Format("  P%d", w.schedule->GetPriority());
            double priW = TextWidth(gc.get(), smallFont, pri);
            Text(gc.get(), dc, smallBold, t.text, w.schedule->GetName() + from, FromDIP(14), y + (barH - smallH) / 2, labelW - FromDIP(14) - priW);
            Text(gc.get(), dc, smallFont, t.muted, pri, labelW - priW, y + (barH - smallH) / 2, priW + 2);
            wxString tipBase = w.schedule->GetName() + ", " + ScheduleCalendar::FormatRange(w.start, w.end) + "\nPlaylist " + w.playlist->GetNameNoTime() + ", priority " +
                               wxString::Format("%d", w.schedule->GetPriority());
            if (!w.Competes()) {
                double sx = X(w.start);
                double sw = X(w.end) - sx;
                Fill(gc.get(), wxNullColour, wxPen(t.muted, 1, wxPENSTYLE_SHORT_DASH), sx, y, sw, barH, FromDIP(4));
                wxString what = w.skipped ? "Skipped this night (skip date)" : "Inactive";
                Text(gc.get(), dc, smallFont, t.muted, what, sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
                _hits.push_back({ wxRect((int)sx, (int)y, (int)sw, (int)barH), w.schedule, tipBase + "\n" + what });
            }
            for (const auto& g : lane.segments) {
                double sx = X(g.start);
                double sw = X(g.end) - sx;
                wxString label = StateText(d, g);
                if (g.state == ScheduleCalendar::State::Plays) {
                    Fill(gc.get(), col.bar, *wxTRANSPARENT_PEN, sx, y, sw, barH, FromDIP(4));
                    Text(gc.get(), dc, smallBold, OnBar(), label, sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
                } else if (g.state == ScheduleCalendar::State::Done) {
                    Fill(gc.get(), wxNullColour, wxPen(t.muted, 1, wxPENSTYLE_SHORT_DASH), sx, y, sw, barH, FromDIP(4));
                    Text(gc.get(), dc, smallFont, t.muted, label, sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
                } else if (g.state == ScheduleCalendar::State::Hidden) {
                    Fill(gc.get(), wxNullColour, wxPen(col.soft), sx, y, sw, barH, FromDIP(4));
                    Hatch(gc.get(), col.soft, sx, y, sw, barH, FromDIP(9));
                    Text(gc.get(), dc, smallFont, col.text, label, sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
                } else {
                    Fill(gc.get(), col.soft, wxPen(t.badText, FromDIP(2)), sx + 1, y + 1, sw - 2, barH - 2, FromDIP(4));
                    Text(gc.get(), dc, smallBold, t.badText, label, sx + FromDIP(6), y + (barH - smallH) / 2, sw - FromDIP(10));
                }
                _hits.push_back({ wxRect((int)sx, (int)y, (int)sw, (int)barH), w.schedule, tipBase + "\n" + label });
            }
            _hits.push_back({ wxRect(0, (int)y, (int)labelW, (int)barH), w.schedule, tipBase });
            y += rowH + FromDIP(2);
        }
        if (shownLanes < (int)d.lanes.size()) {
            Text(gc.get(), dc, smallFont, t.muted, wxString::Format("+%d more schedules: make the window taller to see them", (int)d.lanes.size() - shownLanes), FromDIP(14), y, size.x);
        }

        // legend, when there is room under the lanes
        if (shownLanes < (int)d.lanes.size() || y + legendH > size.y) return;
        double ly = size.y - legendH + FromDIP(4);
        double lx = 0;
        auto key = [&](const wxString& text, std::function<void(double, double)> draw) {
            draw(lx, ly);
            lx += FromDIP(28);
            Text(gc.get(), dc, smallFont, t.muted, text, lx, ly, size.x - lx);
            lx += TextWidth(gc.get(), smallFont, text) + FromDIP(18);
        };
        const double kh = FromDIP(11);
        Colors grey{ t.muted, Mix(t.panel, t.muted, 0.45), t.muted };
        key("Plays", [&](double kx, double ky) { Fill(gc.get(), t.muted, *wxTRANSPARENT_PEN, kx, ky + FromDIP(2), FromDIP(22), kh, FromDIP(2)); });
        key("Waits for a higher priority", [&](double kx, double ky) { Hatch(gc.get(), grey.soft, kx, ky + FromDIP(2), FromDIP(22), kh, FromDIP(5)); });
        key("Same priority: a clash to fix", [&](double kx, double ky) { Fill(gc.get(), wxNullColour, wxPen(t.badText, FromDIP(2)), kx + 1, ky + FromDIP(3), FromDIP(20), kh - 2, FromDIP(2)); });
        key("Skipped, inactive or finished", [&](double kx, double ky) { Fill(gc.get(), wxNullColour, wxPen(t.muted, 1, wxPENSTYLE_SHORT_DASH), kx, ky + FromDIP(2), FromDIP(22), kh, FromDIP(2)); });
        if (!plays.empty()) {
            key("Song timing", [&](double kx, double ky) {
                Marker(gc.get(), t.waitText, kx + FromDIP(5), ky + FromDIP(2), FromDIP(5));
                Marker(gc.get(), t.badText, kx + FromDIP(17), ky + FromDIP(2), FromDIP(5));
            });
        }
    }

    void OnDoubleClick(wxMouseEvent& event) {
        for (const auto& h : _hits) {
            if (h.rect.Contains(event.GetPosition()) && h.schedule != nullptr) {
                _edit(h.schedule);
                return;
            }
        }
    }

    void OnMotion(wxMouseEvent& event) {
        wxString tip;
        for (const auto& h : _hits) {
            if (h.rect.Contains(event.GetPosition())) tip = h.tip;
        }
        for (const auto& c : _chips) {
            if (c.first.Contains(event.GetPosition())) tip = "Show this in the play-by-play";
        }
        if (tip != _tip) {
            _tip = tip;
            if (tip.empty()) UnsetToolTip(); else SetToolTip(tip);
        }
    }

    CalendarPanel::EditSchedule _edit;
    std::function<void(int)> _finding;
    ScheduleCalendar::Detail _detail;
    ScheduleCalendar::Preview _preview;
    std::vector<std::pair<wxRect, int>> _chips;
    bool _headerOnly = false;
    bool _has = false;
    std::vector<Hit> _hits;
    wxString _tip;
};

// nights with no show, clashes and skipped nights over a range, for checking a season before it starts
class SeasonCheckDialog : public wxDialog {
public:
    SeasonCheckDialog(wxWindow* parent, ScheduleManager* manager, const wxDateTime& from, const wxDateTime& to, CalendarPanel::EditSchedule edit) :
        wxDialog(parent, wxID_ANY, "Check the season", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), _manager(manager), _edit(edit) {
        auto* top = new wxBoxSizer(wxVERTICAL);
        auto* range = new wxBoxSizer(wxHORIZONTAL);
        range->Add(new wxStaticText(this, wxID_ANY, "From"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        _from = new DateField(this, wxID_ANY, from);
        range->Add(_from, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
        range->Add(new wxStaticText(this, wxID_ANY, "To"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        _to = new DateField(this, wxID_ANY, to);
        range->Add(_to, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
        const wxColour background = GetBackgroundColour();
        auto* check = new wxBitmapButton(this, wxID_ANY, ModernUI::MakeTextButton(this, "Check", true, 30, &background), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxBORDER_NONE);
        check->SetLabel("Check");
        range->Add(check, 0, wxALIGN_CENTER_VERTICAL);
        top->Add(range, 0, wxALL, FromDIP(12));

        _strip = new wxWindow(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(600, 34)));
        _strip->SetBackgroundStyle(wxBG_STYLE_PAINT);
        _strip->Bind(wxEVT_PAINT, &SeasonCheckDialog::OnPaintStrip, this);
        top->Add(_strip, 0, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(12));
        _summary = new wxStaticText(this, wxID_ANY, "");
        top->Add(_summary, 0, wxALL, FromDIP(12));

        _list = new wxListView(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(640, 300)), wxLC_REPORT | wxLC_SINGLE_SEL);
        _list->AppendColumn("Finding");
        _list->AppendColumn("When");
        _list->AppendColumn("Details");
        top->Add(_list, 1, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(12));
        top->Add(new wxStaticText(this, wxID_ANY, "Double-click a clash or a skipped night to edit its schedule, or a song timing finding to see its worst night."), 0, wxALL, FromDIP(12));
        top->Add(CreateButtonSizer(wxCLOSE), 0, wxALL | wxEXPAND, FromDIP(8));
        SetSizerAndFit(top);
        SetEscapeId(wxID_CLOSE);

        check->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Check(); });
        _list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& e) {
            long i = e.GetIndex();
            if (i < 0 || i >= (long)_rowSchedules.size()) return;
            if (_rowNights[i].IsValid()) {
                _chosen = _rowNights[i];
                EndModal(wxID_OK);
            } else if (_rowSchedules[i] != nullptr) {
                _edit(_rowSchedules[i]);
            }
        });
        Check();
    }

    // the night to open after a song timing finding was double-clicked
    wxDateTime Chosen() const { return _chosen; }

private:
    void Check() {
        wxDateTime from = _from->GetDate();
        wxDateTime to = _to->GetDate();
        if (!from.IsValid() || !to.IsValid() || to < from) return;
        if ((to - from).GetDays() > 400) to = from + wxDateSpan::Days(400);
        _nights = ScheduleCalendar::Nights(*_manager, from, to, false);

        _list->DeleteAllItems();
        _rowSchedules.clear();
        _rowNights.clear();
        const auto& t = ModernUI::GetTheme();
        int empty = 0;
        int clashes = 0;
        auto add = [this, &t](const wxString& finding, const wxString& when, const wxString& details, Schedule* s, bool bad, const wxDateTime& night = wxDateTime(), const wxColour& color = wxNullColour) {
            long i = _list->InsertItem(_list->GetItemCount(), finding);
            _list->SetItem(i, 1, when);
            _list->SetItem(i, 2, details);
            if (bad) _list->SetItemTextColour(i, t.badText);
            else if (color.IsOk()) _list->SetItemTextColour(i, color);
            _rowSchedules.push_back(s);
            _rowNights.push_back(night);
        };
        for (size_t i = 0; i < _nights.size();) {
            bool none = std::none_of(_nights[i].windows.begin(), _nights[i].windows.end(), [](const auto& w) { return w.Competes(); });
            if (!none) {
                ++i;
                continue;
            }
            size_t j = i;
            while (j + 1 < _nights.size() && std::none_of(_nights[j + 1].windows.begin(), _nights[j + 1].windows.end(), [](const auto& w) { return w.Competes(); })) ++j;
            empty += (int)(j - i + 1);
            wxString when = j == i ? ShortDate(_nights[i].date) : ShortDate(_nights[i].date) + " - " + ShortDate(_nights[j].date);
            add("No show", when, j == i ? "Nothing is scheduled." : wxString::Format("Nothing is scheduled for %d nights.", (int)(j - i + 1)), nullptr, true);
            i = j + 1;
        }
        for (const auto& n : _nights) {
            for (const auto& c : ScheduleCalendar::ClashesOf(n.windows)) {
                ++clashes;
                const auto& a = n.windows[c.a];
                const auto& b = n.windows[c.b];
                add("Clash", ShortDate(n.date) + ", " + ScheduleCalendar::FormatRange(c.start, c.end),
                    a.schedule->GetName() + " and " + b.schedule->GetName() + ", both priority " + wxString::Format("%d", a.schedule->GetPriority()),
                    b.schedule, true);
            }
        }
        for (const auto& n : _nights) {
            for (const auto& w : n.windows) {
                if (w.skipped) add("Skipped", ShortDate(n.date), w.schedule->GetName() + " skips this night (skip date).", w.schedule, false);
            }
        }
        int timing = SongTiming(add);
        if (_list->GetItemCount() == 0) add("All good", "", "A show every night, and no clashes.", nullptr, false);
        for (int c = 0; c < 3; ++c) _list->SetColumnWidth(c, wxLIST_AUTOSIZE);
        wxString summary = wxString::Format("%d nights checked: %d without a show, %d %s.", (int)_nights.size(), empty, clashes, clashes == 1 ? "clash" : "clashes");
        if (timing > 0) summary += wxString::Format(" Song timing findings on %d %s.", timing, timing == 1 ? "night" : "nights");
        _summary->SetLabel(summary);
        _strip->Refresh();
        Layout();
    }

    // plays every night out step by step; a finding that repeats becomes one row with its worst night. Returns the
    // number of nights with a finding.
    template <typename Add>
    int SongTiming(Add& add) {
        wxBusyCursor busy;
        struct Group {
            SchedulePreview::Kind kind;
            Schedule* schedule = nullptr;
            int nights = 0;
            int lastNight = -1;
            wxDateTime first;
            wxDateTime last;
            wxDateTime worstNight;
            long long worst = -1;
            wxString text;
        };
        std::vector<Group> groups;
        _timing.assign(_nights.size(), false);
        for (size_t n = 0; n < _nights.size(); ++n) {
            auto d = ScheduleCalendar::DetailOf(*_manager, _nights[n].date, false);
            auto p = ScheduleCalendar::PreviewOf(*_manager, d);
            for (const auto& f : p.plan.findings) {
                if (f.window < 0) continue;
                const auto& w = d.windows[p.detailWindow[f.window]];
                // the nights either side are checked on their own
                if (DayNumber(w.night) != DayNumber(_nights[n].date)) continue;
                _timing[n] = true;
                auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) { return x.kind == f.kind && x.schedule == w.schedule; });
                if (g == groups.end()) {
                    groups.push_back({ f.kind, w.schedule });
                    g = groups.end() - 1;
                    g->first = _nights[n].date;
                }
                if (g->lastNight != (int)n) {
                    ++g->nights;
                    g->lastNight = (int)n;
                }
                g->last = _nights[n].date;
                if (f.amount > g->worst) {
                    g->worst = f.amount;
                    g->worstNight = _nights[n].date;
                    g->text = FindingText(f, p);
                }
            }
        }
        for (const auto& g : groups) {
            wxString when = g.nights == 1 ? ShortDate(g.first) : wxString::Format("%d nights, ", g.nights) + ShortDate(g.first) + " - " + ShortDate(g.last);
            wxString details = g.text + (g.nights > 1 ? " (worst " + ShortDate(g.worstNight) + ")" : wxString());
            add(KindName(g.kind), when, details, g.schedule, false, g.worstNight, FindingColor(g.kind));
        }
        return (int)std::count(_timing.begin(), _timing.end(), true);
    }

    void OnPaintStrip(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(_strip);
        const auto& t = ModernUI::GetTheme();
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc || _nights.empty()) return;
        const wxSize size = _strip->GetClientSize();
        const double gap = _nights.size() > 120 ? 1 : FromDIP(2);
        const double w = std::max(2.0, (size.x - gap * (_nights.size() - 1)) / _nights.size());
        for (size_t i = 0; i < _nights.size(); ++i) {
            const auto& n = _nights[i];
            bool none = std::none_of(n.windows.begin(), n.windows.end(), [](const auto& win) { return win.Competes(); });
            bool clash = !ScheduleCalendar::ClashesOf(n.windows).empty();
            double x = i * (w + gap);
            if (none) Fill(gc.get(), t.badText, *wxTRANSPARENT_PEN, x, 0, w, size.y, FromDIP(2));
            else if (clash) Fill(gc.get(), wxNullColour, wxPen(t.badText, FromDIP(2)), x + 1, 1, w - 2, size.y - 2, FromDIP(2));
            else Fill(gc.get(), n.holiday.empty() ? t.accent : t.waitText, *wxTRANSPARENT_PEN, x, 0, w, size.y, FromDIP(2));
            if (i < _timing.size() && _timing[i]) Fill(gc.get(), ModernUI::GetTheme().dark ? wxColour("#f2b84b") : wxColour("#d18b00"), *wxTRANSPARENT_PEN, x, size.y - FromDIP(6), w, FromDIP(6), 0);
        }
    }

    ScheduleManager* _manager;
    CalendarPanel::EditSchedule _edit;
    DateField* _from = nullptr;
    DateField* _to = nullptr;
    wxWindow* _strip = nullptr;
    wxStaticText* _summary = nullptr;
    wxListView* _list = nullptr;
    std::vector<ScheduleCalendar::Night> _nights;
    std::vector<Schedule*> _rowSchedules;
    std::vector<wxDateTime> _rowNights;
    std::vector<bool> _timing;
    wxDateTime _chosen;
};

CalendarPanel::CalendarPanel(wxWindow* parent, EditSchedule edit, AddSchedule add) :
    wxPanel(parent, wxID_ANY), _edit(edit), _add(add), _clock(this)
{
    _selected = wxDateTime::Today();
    _month = wxDateTime(1, _selected.GetMonth(), _selected.GetYear());
    const auto& t = ModernUI::GetTheme();
    SetBackgroundColour(t.panel);

    auto icon = [this, &t](ModernUI::Glyph glyph, const wxString& label, const wxString& tip) {
        auto* b = new wxBitmapButton(this, wxID_ANY, ModernUI::MakeIconButton(this, glyph, false, 30, &t.panel), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxBORDER_NONE);
        b->SetLabel(label); // read out by screen readers; only the picture shows
        b->SetToolTip(tip);
        return b;
    };
    auto text = [this, &t](const wxString& label, const wxString& tip) {
        auto* b = new wxBitmapButton(this, wxID_ANY, ModernUI::MakeTextButton(this, label, false, 30, &t.panel), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW | wxBORDER_NONE);
        b->SetLabel(label);
        b->SetToolTip(tip);
        return b;
    };
    auto* top = new wxBoxSizer(wxVERTICAL);
    _back = text("Back to month", "Back to the month (Esc)");
    _prev = icon(ModernUI::Glyph::ChevronLeft, "Previous", "Previous month (Page Up)");
    _next = icon(ModernUI::Glyph::ChevronRight, "Next", "Next month (Page Down)");
    _title = new wxStaticText(this, wxID_ANY, "");
    _title->SetFont(GetFont().Scaled(1.3f).Bold());
    _title->SetForegroundColour(t.text);
    _title->SetBackgroundColour(t.panel);
    auto* today = text("Today", "Go to today (Home)");
    _inactiveBox = new wxCheckBox(this, wxID_ANY, "Show inactive");
    _inactiveBox->SetForegroundColour(t.text);
    auto* season = text("Check season...", "Nights with no show, clashes, skipped nights and song timing");
    _timelineButton = text("Timeline", "The night as a timeline");
    _listButton = text("Play-by-play", "Every step the night plays, with its times");
    auto* left = new wxBoxSizer(wxHORIZONTAL);
    left->Add(_back, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    left->Add(_prev, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    left->Add(_next, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    left->Add(_title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    left->Add(today, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(16));
    auto* right = new wxBoxSizer(wxHORIZONTAL);
    right->AddStretchSpacer();
    right->Add(_timelineButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    right->Add(_listButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(16));
    right->Add(_inactiveBox, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    right->Add(season, 0, wxALIGN_CENTER_VERTICAL);
    // in a narrow window the right-hand buttons move to a second line, still on the right
    auto* bar = new wxWrapSizer(wxHORIZONTAL, wxEXTEND_LAST_ON_EACH_LINE);
    bar->Add(left, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(8));
    bar->Add(right, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, FromDIP(8));
    top->Add(bar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));

    // the month uses the whole tab; a night opens in its place
    _book = new wxSimplebook(this, wxID_ANY);
    auto menu = [this](const wxDateTime& d) { NightMenu(d); };
    _monthView = new CalendarMonth(_book, [this](const wxDateTime& d) { Select(d); }, [this](const wxDateTime& d) { Select(d); ShowNight(true); }, _edit, menu);
    _nightPage = new wxPanel(_book);
    _nightPage->SetBackgroundColour(t.panel);
    _dayView = new CalendarDay(_nightPage, _edit, menu, [this](int f) { ShowFinding(f); });
    _list = new wxListView(_nightPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_SORT_HEADER);
    _list->SetName("Play-by-play");
    for (const char* c : { "Starts", "Ends", "Schedule", "Step", "Length", "Why" }) _list->AppendColumn(c);
    _footer = new wxStaticText(_nightPage, wxID_ANY, "");
    _footer->SetForegroundColour(t.muted);
    auto* night = new wxBoxSizer(wxVERTICAL);
    night->Add(_dayView, 1, wxEXPAND);
    night->Add(_list, 1, wxEXPAND);
    night->Add(_footer, 0, wxEXPAND | wxTOP, FromDIP(6));
    _nightPage->SetSizer(night);
    _list->Hide();
    _footer->Hide();
    _book->AddPage(_monthView, "Month");
    _book->AddPage(_nightPage, "Night");
    top->Add(_book, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8));
    SetSizer(top);

    _back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowNight(false); });
    _prev->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Step(-1); });
    _next->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Step(1); });
    today->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Select(wxDateTime::Today()); });
    for (wxWindow* view : { (wxWindow*)_monthView, (wxWindow*)_dayView }) {
        view->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
            if (!Keyboard(e)) e.Skip();
        });
    }
    _timelineButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPlayByPlay(false); });
    _listButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetPlayByPlay(true); });
    _list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& e) { ActivateRow(e.GetIndex()); });
    _list->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_ESCAPE) ShowNight(false);
        else e.Skip();
    });
    _nightPage->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        e.Skip();
        if (_playByPlay) CallAfter([this]() { FitHeader(); });
    });
    SetPlayByPlay(false);
    ShowNight(false);
    _inactiveBox->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) { _inactive = e.IsChecked(); RefreshFrom(_manager); });
    season->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (_manager == nullptr) return;
        // the season: from the first night with a show to the last, looking a year ahead
        wxDateTime from = _month;
        wxDateTime to = _month + wxDateSpan::Months(2) - wxDateSpan::Day();
        auto year = ScheduleCalendar::Nights(*_manager, wxDateTime::Today(), wxDateTime::Today() + wxDateSpan::Year(), false);
        auto plays = [](const ScheduleCalendar::Night& n) { return std::any_of(n.windows.begin(), n.windows.end(), [](const auto& w) { return w.Competes(); }); };
        auto first = std::find_if(year.begin(), year.end(), plays);
        auto last = std::find_if(year.rbegin(), year.rend(), plays);
        if (first != year.end()) {
            from = first->date;
            to = last->date;
        }
        SeasonCheckDialog dlg(this, _manager, from, to, _edit);
        dlg.ShowModal();
        if (dlg.Chosen().IsValid()) {
            Select(dlg.Chosen());
            ShowNight(true);
            SetPlayByPlay(true);
        }
    });
    Bind(wxEVT_TIMER, &CalendarPanel::OnClock, this);
    _clock.Start(30000);
}

void CalendarPanel::Select(const wxDateTime& date)
{
    _selected = date.GetDateOnly();
    _month = wxDateTime(1, _selected.GetMonth(), _selected.GetYear());
    RefreshFrom(_manager);
}

void CalendarPanel::ShowNight(bool night)
{
    _night = night;
    _book->ChangeSelection(night ? 1 : 0);
    _back->Show(night);
    _timelineButton->Show(night);
    _listButton->Show(night);
    _prev->SetToolTip(night ? "Previous night (Left)" : "Previous month (Page Up)");
    _next->SetToolTip(night ? "Next night (Right)" : "Next month (Page Down)");
    Layout();
    Refresh(); // the hidden Back button's area is left unpainted otherwise
    if (IsShownOnScreen()) (night ? (_playByPlay ? (wxWindow*)_list : (wxWindow*)_dayView) : (wxWindow*)_monthView)->SetFocus();
}

// arrows move between nights, Enter opens one and Esc goes back, Page Up and Down change month, Home is today
bool CalendarPanel::Keyboard(wxKeyEvent& event)
{
    switch (event.GetKeyCode()) {
    case WXK_TAB:
        // the views take every key, so Tab has to move on by hand
        static_cast<wxWindow*>(event.GetEventObject())->Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
        return true;
    case WXK_HOME:
        Select(wxDateTime::Today());
        return true;
    case WXK_PAGEUP:
        if (_night) Select(_selected - wxDateSpan::Month()); else Step(-1);
        return true;
    case WXK_PAGEDOWN:
        if (_night) Select(_selected + wxDateSpan::Month()); else Step(1);
        return true;
    case WXK_LEFT:
        Select(_selected - wxDateSpan::Day());
        return true;
    case WXK_RIGHT:
        Select(_selected + wxDateSpan::Day());
        return true;
    case WXK_UP:
        if (_night) return false;
        Select(_selected - wxDateSpan::Week());
        return true;
    case WXK_DOWN:
        if (_night) return false;
        Select(_selected + wxDateSpan::Week());
        return true;
    case WXK_RETURN:
    case WXK_NUMPAD_ENTER:
    case WXK_SPACE:
        if (!_night) ShowNight(true);
        return true;
    case WXK_ESCAPE:
    case WXK_BACK:
        if (!_night) return false;
        ShowNight(false);
        return true;
    }
    return false;
}

void CalendarPanel::NightMenu(const wxDateTime& night)
{
    if (_manager == nullptr) return;
    wxMenu menu;
    menu.Append(1, "Open " + night.Format("%A, %B ") + wxString::Format("%d", night.GetDay()));
    menu.AppendSeparator();
    wxMenu* add = new wxMenu();
    std::vector<PlayList*> playlists;
    for (const auto& pl : _manager->GetPlayLists()) {
        playlists.push_back(pl);
        add->Append(100 + (int)playlists.size() - 1, pl->GetNameNoTime() + "...");
    }
    auto* item = menu.AppendSubMenu(add, "Add a schedule starting this night for");
    if (playlists.empty()) item->Enable(false);
    int chosen = GetPopupMenuSelectionFromUser(menu);
    if (chosen == 1) {
        Select(night);
        ShowNight(true);
    } else if (chosen >= 100 && chosen - 100 < (int)playlists.size()) {
        _add(playlists[chosen - 100], night);
    }
}

void CalendarPanel::Step(int direction)
{
    if (_night) {
        Select(_selected + wxDateSpan::Days(direction));
    } else {
        _month += wxDateSpan::Months(direction);
        RefreshFrom(_manager);
    }
}

void CalendarPanel::Forget()
{
    _manager = nullptr;
    _signature.clear();
    _monthView->Set({}, _month, _selected);
    _dayView->Clear();
    _detail = ScheduleCalendar::Detail();
    _preview = ScheduleCalendar::Preview();
    _rows.clear();
    _list->DeleteAllItems();
}

std::string CalendarPanel::Signature() const
{
    std::string sig = _month.FormatISODate().ToStdString() + _selected.FormatISODate().ToStdString() + wxDateTime::Today().FormatISODate().ToStdString() +
                      (_inactive ? "i" : "") + "|" + Holidays::GetCountry() + "|";
    double latitude;
    double longitude;
    Schedule::GetLocation(latitude, longitude);
    sig += std::to_string(latitude) + "," + std::to_string(longitude) + "|";
    for (const auto& pl : _manager->GetPlayLists()) {
        // step edits change the play-by-play
        sig += std::to_string(pl->GetId()) + "/" + std::to_string(pl->GetChangeCount()) + "/" + std::to_string(pl->GetStepCount()) + "/" + std::to_string(pl->GetLengthMS()) +
               (pl->GetFirstOnce() ? "f" : "") + (pl->GetLastOnce() ? "l" : "") + (pl->GetShuffle() ? "s" : "") + ":";
        for (const auto& s : pl->GetSchedules()) {
            sig += std::to_string(s->GetId()) + "." + std::to_string(s->GetChangeCount()) + (s->GetEnabled() ? "e" : "d") + ",";
        }
        sig += ";";
    }
    return sig;
}

void CalendarPanel::RefreshFrom(ScheduleManager* manager, bool force)
{
    _manager = manager;
    if (_manager == nullptr) return;
    std::string sig = Signature();
    if (!force && sig == _signature) return;
    _signature = sig;
    Recompute();
}

void CalendarPanel::Recompute()
{
    _title->SetLabel(_month.Format("%B %Y"));
    const int fw = FirstWeekDay();
    wxDateTime first = _month;
    wxDateTime start = first - wxDateSpan::Days(((int)first.GetWeekDay() - fw + 7) % 7);
    wxDateTime last = _month + wxDateSpan::Month() - wxDateSpan::Day();
    wxDateTime end = last + wxDateSpan::Days(6 - ((int)last.GetWeekDay() - fw + 7) % 7);
    _monthView->Set(ScheduleCalendar::Nights(*_manager, start, end, _inactive), _month, _selected);
    _detail = ScheduleCalendar::DetailOf(*_manager, _selected, _inactive);
    _preview = ScheduleCalendar::PreviewOf(*_manager, _detail);
    // stretch the timeline to show a show running on past its end, within reason
    long long showEnd = 0;
    for (const auto& p : _preview.plan.plays) {
        if (p.window < 0) continue;
        if (DayNumber(_detail.windows[_preview.detailWindow[p.window]].night) <= DayNumber(_detail.night.date)) showEnd = std::max(showEnd, p.end);
    }
    int lastMinute = (int)std::ceil(showEnd / 3600000.0) * 60;
    _detail.axisEnd = std::min(std::max(_detail.axisEnd, lastMinute), _detail.axisEnd + 120);
    if (!_unfoldedNight.IsValid() || DayNumber(_unfoldedNight) != DayNumber(_selected)) {
        _unfolded.clear();
        _unfoldedNight = _selected;
    }
    _dayView->Set(_detail, _preview);
    if (_playByPlay) {
        FillList();
        FitHeader();
    }
    Layout();
}

void CalendarPanel::SetPlayByPlay(bool list)
{
    _playByPlay = list;
    const auto& t = ModernUI::GetTheme();
    _timelineButton->SetBitmap(ModernUI::MakeTextButton(this, "Timeline", !list, 30, &t.panel));
    _listButton->SetBitmap(ModernUI::MakeTextButton(this, "Play-by-play", list, 30, &t.panel));
    _dayView->SetHeaderOnly(list);
    _list->Show(list);
    _footer->Show(list);
    _nightPage->GetSizer()->GetItem(_dayView)->SetProportion(list ? 0 : 1);
    _dayView->SetMinSize(list ? wxSize(-1, _dayView->HeaderHeight(_nightPage->GetClientSize().x)) : FromDIP(wxSize(500, 150)));
    if (list) FillList();
    Layout();
    _nightPage->Layout();
    if (_night && IsShownOnScreen()) (list ? (wxWindow*)_list : (wxWindow*)_dayView)->SetFocus();
}

// the header's findings wrap, so its height follows the width
void CalendarPanel::FitHeader()
{
    if (!_playByPlay) return;
    int h = _dayView->HeaderHeight(_nightPage->GetClientSize().x);
    if (_dayView->GetMinSize().y != h) {
        _dayView->SetMinSize(wxSize(-1, h));
        _nightPage->Layout();
    }
}

void CalendarPanel::FillList()
{
    using SchedulePreview::Play;
    const auto& t = ModernUI::GetTheme();
    const auto& plays = _preview.plan.plays;
    const auto& findings = _preview.plan.findings;

    // four or more steps in a row with nothing to say about them show as one row
    _rows.clear();
    _folds.clear();
    auto plain = [](const Play& p) {
        return p.note.empty() && p.flag == SchedulePreview::Flag::None && p.ending == SchedulePreview::Ending::Finished && !p.called && p.offset == 0;
    };
    for (int i = 0; i < (int)plays.size();) {
        int j = i;
        if (plain(plays[i])) {
            while (j + 1 < (int)plays.size() && plain(plays[j + 1]) && plays[j + 1].window == plays[i].window) ++j;
        }
        if (j - i + 1 >= 4) _folds.push_back({ i, j });
        i = j + 1;
    }
    std::vector<int> darks;
    for (int i = 0; i < (int)findings.size(); ++i) {
        if (findings[i].kind == SchedulePreview::Kind::GoesDark) darks.push_back(i);
    }
    size_t d = 0;
    auto darkUntil = [&](long long when) {
        while (d < darks.size() && findings[darks[d]].at <= when) _rows.push_back({ -1, -1, darks[d++] });
    };
    size_t fold = 0;
    for (int i = 0; i < (int)plays.size();) {
        darkUntil(plays[i].start);
        if (fold < _folds.size() && _folds[fold].first == i) {
            _rows.push_back({ -1, (int)fold, -1 });
            if (_unfolded.count(i) > 0) {
                for (int k = i; k <= _folds[fold].second; ++k) _rows.push_back({ k, -1, -1 });
            }
            i = _folds[fold].second + 1;
            ++fold;
            continue;
        }
        _rows.push_back({ i, -1, -1 });
        ++i;
    }
    darkUntil(LLONG_MAX);

    auto schedule = [this](int w) { return w >= 0 ? wxString::FromUTF8(_preview.windows[w].name) : wxString(); };
    auto time = [](long long ms, bool estimated) { return (estimated ? wxString("~") : wxString()) + ScheduleCalendar::FormatTime(ms); };
    _list->Freeze();
    _list->DeleteAllItems();
    for (const auto& r : _rows) {
        long i = _list->InsertItem(_list->GetItemCount(), "");
        wxColour color;
        if (r.play >= 0) {
            const auto& p = plays[r.play];
            wxString step = wxString::FromUTF8(p.step);
            if (p.called) step += "  (" + wxString::FromUTF8(_preview.playlists[p.playlist]) + ")";
            if (p.pass > 1) step += wxString::FromUTF8("  \xC2\xB7  pass ") + wxString::Format("%d", p.pass);
            _list->SetItem(i, 0, time(p.start, p.estimated));
            _list->SetItem(i, 1, time(p.end, p.estimated));
            _list->SetItem(i, 2, schedule(p.window));
            _list->SetItem(i, 3, step);
            _list->SetItem(i, 4, SchedulePreview::Duration(p.end - p.start));
            _list->SetItem(i, 5, wxString::FromUTF8(p.note));
            if (p.flag == SchedulePreview::Flag::Cut) color = t.badText;
            else if (p.flag != SchedulePreview::Flag::None) color = t.waitText;
        } else if (r.fold >= 0) {
            int a = _folds[r.fold].first;
            int b = _folds[r.fold].second;
            bool open = _unfolded.count(a) > 0;
            bool same = std::all_of(plays.begin() + a, plays.begin() + b + 1, [&](const Play& p) { return p.step == plays[a].step; });
            wxString what = same ? wxString::FromUTF8(plays[a].step) + wxString::Format(" x%d", b - a + 1)
                            : plays[a].pass != plays[b].pass ? wxString::Format("Passes %d-%d, %d steps", plays[a].pass, plays[b].pass, b - a + 1)
                                                             : wxString::Format("%d steps", b - a + 1);
            _list->SetItem(i, 0, time(plays[a].start, plays[a].estimated));
            _list->SetItem(i, 1, time(plays[b].end, plays[b].estimated));
            _list->SetItem(i, 2, schedule(plays[a].window));
            _list->SetItem(i, 3, wxString::FromUTF8(open ? "\xE2\x96\xBE " : "\xE2\x96\xB8 ") + what);
            _list->SetItem(i, 4, SchedulePreview::Duration(plays[b].end - plays[a].start));
            _list->SetItem(i, 5, open ? "Double-click to fold these steps" : "Double-click to list these steps");
            color = t.muted;
        } else {
            const auto& f = findings[r.finding];
            _list->SetItem(i, 0, time(f.at, false));
            _list->SetItem(i, 1, time(f.at + f.amount, false));
            _list->SetItem(i, 2, schedule(f.window));
            _list->SetItem(i, 3, "Nothing plays");
            _list->SetItem(i, 4, SchedulePreview::Duration(f.amount));
            _list->SetItem(i, 5, "The lights are off: nothing scheduled has anything left to play");
            color = t.badText;
        }
        if (color.IsOk()) _list->SetItemTextColour(i, color);
    }
    for (int c = 0; c < 6; ++c) {
        _list->SetColumnWidth(c, wxLIST_AUTOSIZE_USEHEADER);
        if (!_rows.empty()) {
            int header = _list->GetColumnWidth(c);
            _list->SetColumnWidth(c, wxLIST_AUTOSIZE);
            if (_list->GetColumnWidth(c) < header) _list->SetColumnWidth(c, header);
        }
    }
    _list->Thaw();

    wxString footer = plays.empty() ? wxString("Nothing plays this night. ") : wxString();
    footer += "Requests, events and button presses can change the night. Shuffled playlists show one possible order, with its times marked ~.";
    if (!_preview.background.empty()) footer += " The background playlist " + wxString::FromUTF8(_preview.background) + " plays alongside.";
    if (_preview.plan.truncated) footer += " Only the first 20,000 steps are shown.";
    _footer->SetLabel(footer);
}

void CalendarPanel::ActivateRow(long row)
{
    if (row < 0 || row >= (long)_rows.size()) return;
    const Row r = _rows[row];
    if (r.fold >= 0) {
        int first = _folds[r.fold].first;
        if (_unfolded.erase(first) == 0) _unfolded.insert(first);
        FillList();
        _list->Select(row);
        _list->Focus(row);
        return;
    }
    int w = r.play >= 0 ? _preview.plan.plays[r.play].window : _preview.plan.findings[r.finding].window;
    if (w >= 0) _edit(_detail.windows[_preview.detailWindow[w]].schedule);
}

void CalendarPanel::ShowFinding(int finding)
{
    if (finding < 0 || finding >= (int)_preview.plan.findings.size()) return;
    if (!_playByPlay) SetPlayByPlay(true);
    const auto f = _preview.plan.findings[finding];
    bool unfolded = false;
    for (const auto& fold : _folds) {
        if (f.play >= fold.first && f.play <= fold.second) unfolded = _unfolded.insert(fold.first).second || unfolded;
    }
    if (unfolded) FillList();
    for (long i = 0; i < (long)_rows.size(); ++i) {
        if ((f.play >= 0 && _rows[i].play == f.play) || _rows[i].finding == finding) {
            _list->Select(i);
            _list->Focus(i);
            _list->EnsureVisible(i);
            break;
        }
    }
    _list->SetFocus();
}

void CalendarPanel::OnClock(wxTimerEvent&)
{
    // the "now" line moves with the clock, and at midnight today moves on
    if (IsShownOnScreen()) {
        RefreshFrom(_manager);
        _dayView->Refresh();
    }
}
