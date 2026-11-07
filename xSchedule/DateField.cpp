/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "DateField.h"

#include <wx/calctrl.h>
#include <wx/generic/calctrlg.h>
#include <wx/dateevt.h>
#include <wx/textctrl.h>
#include <wx/uilocale.h>

// the generic calendar draws itself, so it follows dark mode where the native one stays white
class DateFieldPopup : public wxGenericCalendarCtrl, public wxComboPopup
{
    wxSize _size;

public:
    bool Create(wxWindow* parent) override
    {
        if (!wxGenericCalendarCtrl::Create(parent, wxID_ANY, wxDefaultDateTime, wxPoint(0, 0), wxDefaultSize,
                                    wxCAL_SEQUENTIAL_MONTH_SELECTION | wxBORDER_SUNKEN)) {
            return false;
        }
        _size = wxGenericCalendarCtrl::GetBestSize();
        Bind(wxEVT_CALENDAR_SEL_CHANGED, &DateFieldPopup::OnSelect, this);
        Bind(wxEVT_CALENDAR_DOUBLECLICKED, &DateFieldPopup::OnSelect, this);
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
            if (event.GetKeyCode() == WXK_ESCAPE && !event.HasModifiers()) Dismiss(); else event.Skip();
        });
        return true;
    }

    wxSize GetAdjustedSize(int, int, int) override { return _size; }
    wxWindow* GetControl() override { return this; }
    wxString GetStringValue() const override { return wxString(); }
    void OnPopup() override
    {
        DateField* field = static_cast<DateField*>(GetComboCtrl());
        if (field->GetDate().IsValid()) SetDate(field->GetDate());
    }

    void OnSelect(wxCalendarEvent& event)
    {
        static_cast<DateField*>(GetComboCtrl())->SetDateFromUser(wxGenericCalendarCtrl::GetDate());
        if (event.GetEventType() == wxEVT_CALENDAR_DOUBLECLICKED) Dismiss();
    }
};

wxString DateField::SystemFormat()
{
    static const wxString format = [] {
        // asked of the system locale directly: making it the app's locale would also change how numbers are read
        wxString f = wxUILocale(wxUILocale::GetSystemLocaleId()).GetInfo(wxLOCALE_SHORT_DATE_FMT, wxLOCALE_CAT_DATE);
        f.Replace("%y", "%Y");
        f.Replace("%e", "%d");
        f.Replace("%-d", "%d");
        f.Replace("%-m", "%m");
        // anything other than day, month and year numbers (month names, weekdays) can't be stepped with the arrow keys
        wxString rest = f;
        for (const char* token : { "%d", "%m", "%Y" }) {
            if (!rest.Replace(token, "")) return wxString("%Y-%m-%d");
        }
        if (rest.Contains("%")) return wxString("%Y-%m-%d");
        return f;
    }();
    return format;
}

DateField::DateField(wxWindow* parent, wxWindowID id, const wxDateTime& date, const wxPoint& pos, const wxSize& size, long style,
                     const wxValidator& validator, const wxString& name) :
    wxComboCtrl(parent, id, wxEmptyString, pos, size, style, validator, name),
    _format(SystemFormat())
{
    _popup = new DateFieldPopup();
    SetPopupControl(_popup);
    _value = date.IsValid() ? date.GetDateOnly() : wxDateTime::Today();
    ShowValue();

    GetTextCtrl()->Bind(wxEVT_TEXT, &DateField::OnText, this);
    GetTextCtrl()->Bind(wxEVT_KEY_DOWN, &DateField::OnKeyDown, this);
    GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, &DateField::OnKillFocus, this);
    GetTextCtrl()->SetToolTip(_format == "%Y-%m-%d" ? wxString("Year-month-day. The up and down arrow keys change the part the cursor is in.")
                                                    : wxDateTime(31, wxDateTime::Dec, 2026).Format(_format) + ". The up and down arrow keys change the part the cursor is in.");
}

void DateField::ShowValue()
{
    GetTextCtrl()->ChangeValue(_value.Format(_format));
}

void DateField::SetDate(const wxDateTime& date)
{
    if (!date.IsValid()) return;
    _value = date.GetDateOnly();
    ShowValue();
}

void DateField::SetDateFromUser(const wxDateTime& date)
{
    if (!date.IsValid()) return;
    wxDateTime d = date.GetDateOnly();
    GetTextCtrl()->ChangeValue(d.Format(_format));
    if (d == _value) return;
    _value = d;
    wxDateEvent event(this, _value, wxEVT_DATE_CHANGED);
    GetEventHandler()->ProcessEvent(event);
}

void DateField::OnText(wxCommandEvent& event)
{
    wxDateTime d;
    wxString::const_iterator end;
    wxString text = GetTextCtrl()->GetValue();
    // only a complete date counts; anything else waits for more typing, or is put back when the field loses focus
    if (d.ParseFormat(text, _format, &end) && end == text.end() && d.GetDateOnly() != _value) {
        _value = d.GetDateOnly();
        wxDateEvent changed(this, _value, wxEVT_DATE_CHANGED);
        GetEventHandler()->ProcessEvent(changed);
    }
}

void DateField::OnKillFocus(wxFocusEvent& event)
{
    event.Skip();
    GetTextCtrl()->ChangeValue(_value.Format(_format));
}

char DateField::FieldAt(long pos, long& from, long& to) const
{
    // the text always matches the format, with two digit day and month and a four digit year
    long at = 0;
    char last = 0;
    long lastFrom = 0;
    for (size_t i = 0; i < _format.length(); ++i) {
        if (_format[i] == '%' && i + 1 < _format.length()) {
            char token = (char)_format[i + 1];
            long width = token == 'Y' ? 4 : 2;
            if (pos <= at + width) {
                from = at;
                to = at + width;
                return token;
            }
            last = token;
            lastFrom = at;
            at += width;
            ++i;
        } else {
            ++at;
        }
    }
    from = lastFrom;
    to = lastFrom + (last == 'Y' ? 4 : 2);
    return last;
}

void DateField::OnKeyDown(wxKeyEvent& event)
{
    int key = event.GetKeyCode();
    if ((key != WXK_UP && key != WXK_DOWN) || event.HasAnyModifiers()) {
        event.Skip();
        return;
    }

    // step from what is typed when it is a date, so a typed change isn't lost
    wxDateTime d;
    wxString::const_iterator end;
    wxString text = GetTextCtrl()->GetValue();
    if (!d.ParseFormat(text, _format, &end) || end != text.end()) d = _value;

    long from, to;
    long selFrom, selTo;
    GetTextCtrl()->GetSelection(&selFrom, &selTo);
    char field = FieldAt(selFrom, from, to);
    int step = key == WXK_UP ? 1 : -1;
    if (field == 'd') d += wxDateSpan::Days(step);
    else if (field == 'm') d += wxDateSpan::Months(step);
    else if (field == 'Y') d += wxDateSpan::Years(step);

    SetDateFromUser(d);
    GetTextCtrl()->SetSelection(from, to);
}
