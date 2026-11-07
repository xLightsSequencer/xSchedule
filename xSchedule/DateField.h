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

#include <wx/combo.h>
#include <wx/datetime.h>

class DateFieldPopup;

// A date entry box with a calendar drop down that follows dark mode. It shows dates in the computer's short date
// format, and the up and down arrow keys change the day, month or year the cursor is in. It sends wxEVT_DATE_CHANGED
// when the user changes the date.
class DateField : public wxComboCtrl
{
    DateFieldPopup* _popup = nullptr;
    wxDateTime _value;
    wxString _format;

    void OnText(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKillFocus(wxFocusEvent& event);
    // the field ('d', 'm' or 'Y') at a text position and where it starts and ends
    char FieldAt(long pos, long& from, long& to) const;
    void ShowValue();

public:
    DateField(wxWindow* parent, wxWindowID id, const wxDateTime& date = wxDefaultDateTime, const wxPoint& pos = wxDefaultPosition,
              const wxSize& size = wxDefaultSize, long style = 0, const wxValidator& validator = wxDefaultValidator, const wxString& name = "DateField");

    wxDateTime GetDate() const { return _value; }
    void SetDate(const wxDateTime& date);

    // the computer's short date format, always with a four digit year and two digit day and month
    static wxString SystemFormat();

    // used by the drop down
    void SetDateFromUser(const wxDateTime& date);
};
