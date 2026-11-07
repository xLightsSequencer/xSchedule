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
class wxTopLevelWindow;
class wxBookCtrlBase;
class wxSplitterWindow;

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
    Delay,
    ChevronLeft,
    ChevronRight
};

// background: what shows round the rounded corners, the toolbar's color when not given
wxBitmap MakeIconButton(wxWindow* win, Glyph glyph, bool primary, int sizeDIP = 32, const wxColour* background = nullptr);
// a labelled button in the same style as the icon buttons
wxBitmap MakeTextButton(wxWindow* win, const wxString& label, bool primary, int heightDIP = 32, const wxColour* background = nullptr);
wxBitmap MakeTreeIcon(wxWindow* win, Glyph glyph);
wxBitmap MakeSwatch(wxWindow* win, const wxColour& color);

void DrawLevel(wxDC& dc, wxWindow* win, const wxString& label, int value);

// Grows a window to at least what its sizer needs at the current DPI (a size saved at another scale can be
// too small), within the screen it is on. Never shrinks it.
void FitToContents(wxTopLevelWindow* win);
// Widens report list columns whose header text does not fit.
void FitListHeaders(wxWindow* root);
// Lets the text box of file pickers take the spare width instead of staying a fixed share.
void GrowFilePickers(wxWindow* root);
// Logs every button, check box and label in the window that is narrower than its text (a layout check for
// development; enabled with the XSCHEDULE_LAYOUT_CHECK environment variable).
void LogClippedControls(wxWindow* root);
// Places a side by side splitter's sash at the given fraction, but never so either pane gets less than it needs.
void BalanceSplitter(wxSplitterWindow* splitter, double fraction);
// Tab titles treat "&" as a shortcut marker ("FSEQ & Video" shows as "FSEQ _Video"); escape them.
void EscapePageTitles(wxBookCtrlBase* book);

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
