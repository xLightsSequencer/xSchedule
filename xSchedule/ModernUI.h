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

// Theme colors that follow the system light/dark setting, and runtime-drawn (DPI aware) art.

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/control.h>
#include <wx/datetime.h>

#include <string>

class wxDC;

namespace ModernUI {

struct Theme {
    bool dark = false;
    wxColour toolbar;
    wxColour panel;
    wxColour line;
    wxColour text;
    wxColour muted;
    wxColour accent;
    wxColour onAccent;
    wxColour accentSoft;
    wxColour accentText;
    wxColour buttonBg;
    wxColour buttonBorder;
    wxColour track;
    wxColour okBg;
    wxColour okText;
    wxColour waitBg;
    wxColour waitText;
    wxColour badBg;
    wxColour badText;
    wxColour offBg;
    wxColour offText;
};

const Theme& GetTheme();

enum class PillStyle {
    On,      // toggle that is switched on
    Off,     // toggle that is switched off / not applicable
    Ok,      // healthy / playing
    Warn,    // paused / unsaved
    Bad,     // error
    Neutral  // informational, nothing active
};

// minWidth lets every state of one button share a width so the toolbar does not shift.
wxBitmap MakePill(wxWindow* win, const wxString& label, PillStyle style, int minWidth = 0);
int PillWidth(wxWindow* win, const wxString& label);

enum class Glyph {
    Prior,
    Play,
    Pause,
    Stop,
    Next,
    Minus,
    Plus,
    Playlist,
    Schedule,
    Step,
    Sequence,
    Audio,
    Video,
    Command,
    Network,
    Effect,
    Delay
};

wxBitmap MakeIconButton(wxWindow* win, Glyph glyph, bool primary, int sizeDIP = 32);
wxBitmap MakeTreeIcon(wxWindow* win, Glyph glyph);
wxBitmap MakeSwatch(wxWindow* win, const wxColour& color);

void DrawLevel(wxDC& dc, wxWindow* win, const wxString& label, int value);

std::string FormatDuration(size_t ms);
std::string FormatWhen(const wxDateTime& when);
// Schedule end times come back as "YYYY-MM-DD HH:MM"; today's show just the time.
std::string FormatEndTime(const std::string& end);

class NowPlayingBar : public wxControl {
public:
    enum class State {
        Idle,
        Playing,
        Paused
    };

    NowPlayingBar(wxWindow* parent, wxWindowID id = wxID_ANY);

    void SetStatus(State state, const std::string& playlist, const std::string& step, size_t positionMS, size_t lengthMS, const std::string& detail);

protected:
    wxSize DoGetBestSize() const override;

private:
    void OnPaint(wxPaintEvent& event);

    State _state = State::Idle;
    std::string _playlist;
    std::string _step;
    std::string _detail;
    size_t _position = 0;
    size_t _length = 0;
};

} // namespace ModernUI
