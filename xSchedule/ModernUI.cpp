/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#define _USE_MATH_DEFINES
#include "ModernUI.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "../xlights/xLights/ui/wxUtilities.h"

namespace ModernUI {

namespace {

Theme MakeTheme(bool dark) {
    Theme t;
    t.dark = dark;
    if (dark) {
        t.toolbar = wxColour(0x2B, 0x2B, 0x2B);
        t.panel = wxColour(0x26, 0x26, 0x26);
        t.line = wxColour(0x3A, 0x3A, 0x3A);
        t.text = wxColour(0xE6, 0xE9, 0xED);
        t.muted = wxColour(0xA2, 0xAA, 0xB4);
        t.accent = wxColour(0x6E, 0xA8, 0xFE);
        t.onAccent = wxColour(0x0B, 0x1A, 0x33);
        t.accentSoft = wxColour(0x22, 0x34, 0x4F);
        t.accentText = wxColour(0xA9, 0xCB, 0xFF);
        t.buttonBg = wxColour(0x33, 0x33, 0x33);
        t.buttonBorder = wxColour(0x4A, 0x4A, 0x4A);
        t.track = wxColour(0x40, 0x40, 0x40);
        t.okBg = wxColour(0x1D, 0x35, 0x27);
        t.okText = wxColour(0x86, 0xE3, 0xA6);
        t.waitBg = wxColour(0x3A, 0x2F, 0x17);
        t.waitText = wxColour(0xF5, 0xC6, 0x6B);
        t.badBg = wxColour(0x3D, 0x22, 0x20);
        t.badText = wxColour(0xFF, 0xA3, 0x9A);
        t.offBg = wxColour(0x35, 0x35, 0x35);
        t.offText = wxColour(0xB4, 0xBB, 0xC4);
    } else {
        t.toolbar = wxColour(0xF7, 0xF7, 0xF7);
        t.panel = wxColour(0xFF, 0xFF, 0xFF);
        t.line = wxColour(0xDD, 0xDD, 0xDD);
        t.text = wxColour(0x1B, 0x1F, 0x24);
        t.muted = wxColour(0x5B, 0x64, 0x70);
        t.accent = wxColour(0x25, 0x63, 0xC9);
        t.onAccent = wxColour(0xFF, 0xFF, 0xFF);
        t.accentSoft = wxColour(0xE6, 0xEE, 0xFB);
        t.accentText = wxColour(0x1F, 0x56, 0xB0);
        t.buttonBg = wxColour(0xFF, 0xFF, 0xFF);
        t.buttonBorder = wxColour(0xC8, 0xC8, 0xC8);
        t.track = wxColour(0xE3, 0xE3, 0xE3);
        t.okBg = wxColour(0xE3, 0xF4, 0xE8);
        t.okText = wxColour(0x1E, 0x7B, 0x3C);
        t.waitBg = wxColour(0xFF, 0xF3, 0xD6);
        t.waitText = wxColour(0x8A, 0x5A, 0x00);
        t.badBg = wxColour(0xFD, 0xEC, 0xEA);
        t.badText = wxColour(0xB4, 0x23, 0x18);
        t.offBg = wxColour(0xEE, 0xEE, 0xEE);
        t.offText = wxColour(0x4F, 0x58, 0x64);
    }
    return t;
}

// Pills and buttons are drawn on an opaque background so their text anti-aliases cleanly;
// tree icons have no text and stay transparent.
class Canvas {
public:
    Canvas(int w, int h, const wxColour* background) :
        _image(w, h) {
        if (background != nullptr) {
            _image.SetRGB(wxRect(0, 0, w, h), background->Red(), background->Green(), background->Blue());
        } else {
            _image.InitAlpha();
            std::memset(_image.GetAlpha(), 0, (size_t)w * h);
        }
        _gc.reset(wxGraphicsContext::Create(_image));
        if (_gc == nullptr) {
            // no image capable renderer: draw into a throwaway context so callers need no checks
            _fallback = wxBitmap(std::max(w, 1), std::max(h, 1));
            _dc.SelectObject(_fallback);
            _gc.reset(wxGraphicsContext::Create(_dc));
        }
        _gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
    }
    wxGraphicsContext* operator->() {
        return _gc.get();
    }
    wxGraphicsContext* get() {
        return _gc.get();
    }
    wxBitmap Finish() {
        _gc.reset(); // flushes the drawing into the image
        return wxBitmap(_image);
    }

private:
    wxImage _image;
    wxBitmap _fallback;
    wxMemoryDC _dc;
    std::unique_ptr<wxGraphicsContext> _gc;
};

wxFont PillFont(wxWindow* win) {
    wxFont f = win->GetFont();
    f.SetWeight(wxFONTWEIGHT_SEMIBOLD);
    return f;
}

double TextWidth(wxWindow* win, const wxFont& font, const wxString& text, double* height = nullptr) {
    Canvas c(1, 1, nullptr);
    c->SetFont(font, *wxBLACK);
    double w = 0;
    double h = 0;
    c->GetTextExtent(text, &w, &h);
    if (height != nullptr) *height = h;
    return w;
}

void PillColours(PillStyle style, wxColour& fill, wxColour& border, wxColour& text, wxColour& dot) {
    const Theme& t = GetTheme();
    switch (style) {
    case PillStyle::On:
        fill = t.accentSoft;
        border = t.accent;
        text = t.accentText;
        dot = t.accent;
        break;
    case PillStyle::Off:
        fill = t.toolbar;
        border = t.buttonBorder;
        text = t.muted;
        dot = t.buttonBorder;
        break;
    case PillStyle::Ok:
        fill = t.okBg;
        border = t.okText;
        text = t.okText;
        dot = t.okText;
        break;
    case PillStyle::Warn:
        fill = t.waitBg;
        border = t.waitText;
        text = t.waitText;
        dot = t.waitText;
        break;
    case PillStyle::Bad:
        fill = t.badBg;
        border = t.badText;
        text = t.badText;
        dot = t.badText;
        break;
    case PillStyle::Neutral:
        fill = t.offBg;
        border = t.offBg;
        text = t.offText;
        dot = t.offText;
        break;
    }
}

void DrawGlyph(wxGraphicsContext* gc, Glyph glyph, double x, double y, double s, const wxColour& color, double stroke) {
    auto P = [&](double fx, double fy) { return wxPoint2DDouble(x + fx * s, y + fy * s); };
    wxGraphicsPath path = gc->CreatePath();
    bool fill = true;

    auto triangle = [&](wxPoint2DDouble a, wxPoint2DDouble b, wxPoint2DDouble c) {
        path.MoveToPoint(a);
        path.AddLineToPoint(b);
        path.AddLineToPoint(c);
        path.CloseSubpath();
    };

    switch (glyph) {
    case Glyph::Play:
        triangle(P(0.30, 0.18), P(0.30, 0.82), P(0.84, 0.50));
        break;
    case Glyph::Pause:
        path.AddRoundedRectangle(x + 0.22 * s, y + 0.18 * s, 0.20 * s, 0.64 * s, 0.04 * s);
        path.AddRoundedRectangle(x + 0.58 * s, y + 0.18 * s, 0.20 * s, 0.64 * s, 0.04 * s);
        break;
    case Glyph::Stop:
        path.AddRoundedRectangle(x + 0.22 * s, y + 0.22 * s, 0.56 * s, 0.56 * s, 0.08 * s);
        break;
    case Glyph::Prior:
        path.AddRectangle(x + 0.16 * s, y + 0.20 * s, 0.12 * s, 0.60 * s);
        triangle(P(0.84, 0.20), P(0.84, 0.80), P(0.32, 0.50));
        break;
    case Glyph::Next:
        path.AddRectangle(x + 0.72 * s, y + 0.20 * s, 0.12 * s, 0.60 * s);
        triangle(P(0.16, 0.20), P(0.16, 0.80), P(0.68, 0.50));
        break;
    case Glyph::Minus:
        fill = false;
        path.MoveToPoint(P(0.25, 0.5));
        path.AddLineToPoint(P(0.75, 0.5));
        break;
    case Glyph::Plus:
        fill = false;
        path.MoveToPoint(P(0.25, 0.5));
        path.AddLineToPoint(P(0.75, 0.5));
        path.MoveToPoint(P(0.5, 0.25));
        path.AddLineToPoint(P(0.5, 0.75));
        break;
    case Glyph::Playlist:
        fill = false;
        path.MoveToPoint(P(0.10, 0.22));
        path.AddLineToPoint(P(0.90, 0.22));
        path.MoveToPoint(P(0.10, 0.50));
        path.AddLineToPoint(P(0.58, 0.50));
        path.MoveToPoint(P(0.10, 0.78));
        path.AddLineToPoint(P(0.42, 0.78));
        path.AddCircle(x + 0.74 * s, y + 0.76 * s, 0.12 * s);
        path.MoveToPoint(P(0.86, 0.76));
        path.AddLineToPoint(P(0.86, 0.40));
        break;
    case Glyph::Schedule:
        fill = false;
        path.AddRoundedRectangle(x + 0.12 * s, y + 0.20 * s, 0.76 * s, 0.68 * s, 0.10 * s);
        path.MoveToPoint(P(0.12, 0.42));
        path.AddLineToPoint(P(0.88, 0.42));
        path.MoveToPoint(P(0.34, 0.08));
        path.AddLineToPoint(P(0.34, 0.28));
        path.MoveToPoint(P(0.66, 0.08));
        path.AddLineToPoint(P(0.66, 0.28));
        break;
    case Glyph::Step:
        fill = false;
        path.AddRoundedRectangle(x + 0.10 * s, y + 0.22 * s, 0.80 * s, 0.56 * s, 0.10 * s);
        path.MoveToPoint(P(0.42, 0.38));
        path.AddLineToPoint(P(0.42, 0.62));
        path.AddLineToPoint(P(0.62, 0.50));
        path.CloseSubpath();
        break;
    case Glyph::Sequence:
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                path.AddRoundedRectangle(x + (0.12 + c * 0.28) * s, y + (0.12 + r * 0.28) * s, 0.20 * s, 0.20 * s, 0.04 * s);
            }
        }
        break;
    case Glyph::Audio:
        fill = false;
        path.AddCircle(x + 0.32 * s, y + 0.74 * s, 0.13 * s);
        path.MoveToPoint(P(0.45, 0.74));
        path.AddLineToPoint(P(0.45, 0.14));
        path.AddLineToPoint(P(0.80, 0.26));
        break;
    case Glyph::Video:
        fill = false;
        path.AddRoundedRectangle(x + 0.08 * s, y + 0.24 * s, 0.58 * s, 0.52 * s, 0.08 * s);
        path.MoveToPoint(P(0.66, 0.42));
        path.AddLineToPoint(P(0.92, 0.28));
        path.AddLineToPoint(P(0.92, 0.72));
        path.AddLineToPoint(P(0.66, 0.58));
        break;
    case Glyph::Command:
        fill = false;
        path.MoveToPoint(P(0.16, 0.28));
        path.AddLineToPoint(P(0.42, 0.50));
        path.AddLineToPoint(P(0.16, 0.72));
        path.MoveToPoint(P(0.50, 0.74));
        path.AddLineToPoint(P(0.84, 0.74));
        break;
    case Glyph::Network:
        fill = false;
        path.AddCircle(x + 0.5 * s, y + 0.78 * s, 0.05 * s);
        path.MoveToPoint(P(0.5 - 0.22 * 0.7071, 0.78 - 0.22 * 0.7071));
        path.AddArc(x + 0.5 * s, y + 0.78 * s, 0.22 * s, 5 * M_PI / 4, 7 * M_PI / 4, true);
        path.MoveToPoint(P(0.5 - 0.44 * 0.7071, 0.78 - 0.44 * 0.7071));
        path.AddArc(x + 0.5 * s, y + 0.78 * s, 0.44 * s, 5 * M_PI / 4, 7 * M_PI / 4, true);
        break;
    case Glyph::Effect:
        fill = false;
        path.AddCircle(x + 0.5 * s, y + 0.5 * s, 0.17 * s);
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4;
            path.MoveToPoint(P(0.5 + 0.30 * std::cos(a), 0.5 + 0.30 * std::sin(a)));
            path.AddLineToPoint(P(0.5 + 0.44 * std::cos(a), 0.5 + 0.44 * std::sin(a)));
        }
        break;
    case Glyph::Delay:
        fill = false;
        path.AddCircle(x + 0.5 * s, y + 0.5 * s, 0.40 * s);
        path.MoveToPoint(P(0.5, 0.26));
        path.AddLineToPoint(P(0.5, 0.5));
        path.AddLineToPoint(P(0.68, 0.60));
        break;
    }

    if (fill) {
        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(color));
        gc->FillPath(path);
    } else {
        wxGraphicsPenInfo pen(color, stroke);
        pen.Cap(wxCAP_ROUND).Join(wxJOIN_ROUND);
        gc->SetPen(gc->CreatePen(pen));
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        gc->StrokePath(path);
    }
}

} // namespace

const Theme& GetTheme() {
    static Theme theme = MakeTheme(IsDarkMode());
    return theme;
}

int PillWidth(wxWindow* win, const wxString& label) {
    double tw = TextWidth(win, PillFont(win), label);
    return win->FromDIP(10) + win->FromDIP(8) + win->FromDIP(7) + (int)std::ceil(tw) + win->FromDIP(12);
}

wxBitmap MakePill(wxWindow* win, const wxString& label, PillStyle style, int minWidth) {
    const Theme& t = GetTheme();
    wxColour fill, border, text, dot;
    PillColours(style, fill, border, text, dot);

    const int h = win->FromDIP(28);
    const int w = std::max(PillWidth(win, label), minWidth);
    const double padX = win->FromDIP(10);
    const double dotD = win->FromDIP(8);
    const double gap = win->FromDIP(7);

    Canvas c(w, h, &t.toolbar);
    c->SetPen(c->CreatePen(wxGraphicsPenInfo(border, std::max(1.0, (double)win->FromDIP(1)))));
    c->SetBrush(wxBrush(fill));
    c->DrawRoundedRectangle(0.5, 0.5, w - 1.0, h - 1.0, win->FromDIP(6));

    c->SetPen(*wxTRANSPARENT_PEN);
    c->SetBrush(wxBrush(dot));
    c->DrawEllipse(padX, (h - dotD) / 2.0, dotD, dotD);

    c->SetFont(PillFont(win), text);
    double tw = 0;
    double th = 0;
    c->GetTextExtent(label, &tw, &th);
    c->DrawText(label, padX + dotD + gap, (h - th) / 2.0);
    return c.Finish();
}

wxBitmap MakeIconButton(wxWindow* win, Glyph glyph, bool primary, int sizeDIP) {
    const Theme& t = GetTheme();
    const int s = win->FromDIP(sizeDIP);
    Canvas c(s, s, &t.toolbar);
    c->SetPen(c->CreatePen(wxGraphicsPenInfo(primary ? t.accent : t.buttonBorder, std::max(1.0, (double)win->FromDIP(1)))));
    c->SetBrush(wxBrush(primary ? t.accent : t.buttonBg));
    c->DrawRoundedRectangle(0.5, 0.5, s - 1.0, s - 1.0, win->FromDIP(6));
    const double g = s * 0.5;
    DrawGlyph(c.get(), glyph, (s - g) / 2.0, (s - g) / 2.0, g, primary ? t.onAccent : t.text, std::max(1.5, win->FromDIP(2) * 1.0));
    return c.Finish();
}

wxBitmap MakeTreeIcon(wxWindow* win, Glyph glyph) {
    const Theme& t = GetTheme();
    const int s = win->FromDIP(16);
    Canvas c(s, s, nullptr);
    const wxColour& color = (glyph == Glyph::Playlist || glyph == Glyph::Sequence) ? t.accentText : t.muted;
    DrawGlyph(c.get(), glyph, 1, 1, s - 2, color, std::max(1.2, win->FromDIP(15) / 10.0));
    return c.Finish();
}

wxBitmap MakeSwatch(wxWindow* win, const wxColour& color) {
    const int w = win->FromDIP(5);
    const int h = win->FromDIP(16);
    Canvas c(w, h, nullptr);
    c->SetPen(*wxTRANSPARENT_PEN);
    c->SetBrush(wxBrush(color));
    c->DrawRoundedRectangle(0, 0, w, h, w / 2.0);
    return c.Finish();
}

void DrawLevel(wxDC& dc, wxWindow* win, const wxString& label, int value) {
    const Theme& t = GetTheme();
    const wxSize size = win->GetClientSize();

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
    if (gc == nullptr) return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(t.toolbar));
    gc->DrawRectangle(0, 0, size.x, size.y);

    const double r = win->FromDIP(6);
    gc->SetPen(gc->CreatePen(wxGraphicsPenInfo(t.buttonBorder, std::max(1.0, (double)win->FromDIP(1)))));
    gc->SetBrush(wxBrush(t.buttonBg));
    gc->DrawRoundedRectangle(0.5, 0.5, size.x - 1.0, size.y - 1.0, r);

    const double inset = win->FromDIP(6);
    const double barH = std::max(2.0, (double)win->FromDIP(3));
    const double barY = size.y - inset / 1.5 - barH;
    const double barW = size.x - inset * 2;
    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(t.track));
    gc->DrawRoundedRectangle(inset, barY, barW, barH, barH / 2);
    gc->SetBrush(wxBrush(t.accent));
    gc->DrawRoundedRectangle(inset, barY, barW * std::clamp(value, 0, 100) / 100.0, barH, barH / 2);

    wxFont f = win->GetFont();
    gc->SetFont(f, t.muted);
    double lw, lh;
    gc->GetTextExtent(label, &lw, &lh);
    const double textY = (barY - lh) / 2.0 + win->FromDIP(1);
    gc->DrawText(label, inset + win->FromDIP(2), textY);

    wxFont bold = f;
    bold.SetWeight(wxFONTWEIGHT_SEMIBOLD);
    wxString v = wxString::Format("%d%%", value);
    gc->SetFont(bold, t.text);
    double vw, vh;
    gc->GetTextExtent(v, &vw, &vh);
    gc->DrawText(v, size.x - inset - win->FromDIP(2) - vw, textY);
}

std::string FormatDuration(size_t ms) {
    size_t secs = ms / 1000;
    size_t h = secs / 3600;
    size_t m = (secs % 3600) / 60;
    size_t s = secs % 60;
    if (h > 0) {
        return wxString::Format("%d:%02d:%02d", (int)h, (int)m, (int)s).ToStdString();
    }
    return wxString::Format("%d:%02d", (int)m, (int)s).ToStdString();
}

std::string FormatWhen(const wxDateTime& when) {
    if (!when.IsValid()) return "Never";

    const wxDateTime today = wxDateTime::Today();
    // whole calendar days, unaffected by the 23 and 25 hour days around daylight saving changes
    const long days = std::lround(when.GetDateOnly().GetJDN() - today.GetJDN());
    if (days == 0) return ("Today " + when.Format("%H:%M")).ToStdString();
    if (days == 1) return ("Tomorrow " + when.Format("%H:%M")).ToStdString();
    if (days > 1 && days < 7) return when.Format("%a %H:%M").ToStdString();
    if (when.GetYear() == today.GetYear()) return when.Format("%a %d %b %H:%M").ToStdString();
    return when.Format("%d %b %Y %H:%M").ToStdString();
}

std::string FormatEndTime(const std::string& end) {
    wxDateTime dt;
    wxString::const_iterator parsedTo;
    const wxString text(end);
    if (!dt.ParseFormat(text, "%Y-%m-%d %H:%M", &parsedTo)) return end;
    if (dt.GetDateOnly() == wxDateTime::Today()) return dt.Format("%H:%M").ToStdString();
    return FormatWhen(dt);
}

NowPlayingBar::NowPlayingBar(wxWindow* parent, wxWindowID id) :
    wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, &NowPlayingBar::OnPaint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Refresh(); e.Skip(); });
}

wxSize NowPlayingBar::DoGetBestSize() const {
    return wxSize(FromDIP(300), FromDIP(44));
}

void NowPlayingBar::SetStatus(State state, const std::string& playlist, const std::string& step, size_t positionMS, size_t lengthMS, const std::string& detail) {
    // the bar is redrawn at most once per displayed second
    if (state == _state && playlist == _playlist && step == _step && detail == _detail && lengthMS == _length && positionMS / 1000 == _position / 1000) {
        return;
    }
    _state = state;
    _playlist = playlist;
    _step = step;
    _position = positionMS;
    _length = lengthMS;
    _detail = detail;
    Refresh();
}

void NowPlayingBar::OnPaint(wxPaintEvent&) {
    const Theme& t = GetTheme();
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();

    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
    if (gc == nullptr) return;
    gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

    gc->SetPen(*wxTRANSPARENT_PEN);
    gc->SetBrush(wxBrush(t.panel));
    gc->DrawRectangle(0, 0, size.x, size.y);
    gc->SetBrush(wxBrush(t.line));
    gc->DrawRectangle(0, size.y - FromDIP(1), size.x, FromDIP(1));

    const double pad = FromDIP(14);
    double x = pad;
    const double midY = size.y / 2.0;

    wxString chip = "IDLE";
    wxColour chipBg = t.offBg;
    wxColour chipText = t.offText;
    if (_state == State::Playing) {
        chip = "PLAYING";
        chipBg = t.okBg;
        chipText = t.okText;
    } else if (_state == State::Paused) {
        chip = "PAUSED";
        chipBg = t.waitBg;
        chipText = t.waitText;
    }
    wxFont chipFont = GetFont();
    chipFont.SetPointSize(std::max(7, chipFont.GetPointSize() - 1));
    chipFont.SetWeight(wxFONTWEIGHT_BOLD);
    gc->SetFont(chipFont, chipText);
    double cw, ch;
    gc->GetTextExtent(chip, &cw, &ch);
    const double chipPad = FromDIP(8);
    const double chipH = FromDIP(22);
    gc->SetBrush(wxBrush(chipBg));
    gc->DrawRoundedRectangle(x, midY - chipH / 2, cw + chipPad * 2, chipH, FromDIP(4));
    gc->DrawText(chip, x + chipPad, midY - ch / 2);
    x += cw + chipPad * 2 + FromDIP(14);

    wxFont normal = GetFont();
    normal.SetPointSize(normal.GetPointSize() + 1);
    wxFont bold = normal;
    bold.SetWeight(wxFONTWEIGHT_BOLD);

    double right = size.x - pad;
    if (!_detail.empty()) {
        gc->SetFont(GetFont(), t.muted);
        double dw, dh;
        gc->GetTextExtent(_detail, &dw, &dh);
        gc->DrawText(_detail, right - dw, midY - dh / 2);
        right -= dw + FromDIP(20);
    }

    const bool active = _state != State::Idle && _length > 0;
    if (active) {
        wxString time = FormatDuration(_position) + " / " + FormatDuration(_length);
        gc->SetFont(bold, t.text);
        double tw, th;
        gc->GetTextExtent(time, &tw, &th);
        gc->DrawText(time, right - tw, midY - th / 2);
        right -= tw + FromDIP(16);
    }

    if (_playlist.empty()) {
        gc->SetFont(normal, t.muted);
        double w, h;
        gc->GetTextExtent("Nothing playing", &w, &h);
        gc->DrawText("Nothing playing", x, midY - h / 2);
        x += w;
    } else {
        wxString prefix = wxString(_playlist) + (_step.empty() ? wxString() : wxString(L"  \u203A  "));
        gc->SetFont(normal, t.muted);
        double w, h;
        gc->GetTextExtent(prefix, &w, &h);
        gc->DrawText(prefix, x, midY - h / 2);
        x += w;
        if (!_step.empty()) {
            gc->SetFont(bold, t.text);
            gc->GetTextExtent(_step, &w, &h);
            gc->DrawText(_step, x, midY - h / 2);
            x += w;
        }
    }

    if (active) {
        const double left = x + FromDIP(20);
        const double width = right - left;
        if (width > FromDIP(60)) {
            const double barH = FromDIP(6);
            gc->SetBrush(wxBrush(t.track));
            gc->DrawRoundedRectangle(left, midY - barH / 2, width, barH, barH / 2);
            const double frac = std::min(1.0, (double)_position / (double)_length);
            gc->SetBrush(wxBrush(_state == State::Paused ? t.waitText : t.accent));
            gc->DrawRoundedRectangle(left, midY - barH / 2, std::max(barH, width * frac), barH, barH / 2);
        }
    }
}

} // namespace ModernUI
