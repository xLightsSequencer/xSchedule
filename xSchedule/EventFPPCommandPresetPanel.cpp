/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "EventFPPCommandPresetPanel.h"
#include "events/EventFPPCommandPreset.h"

//(*InternalHeaders(EventFPPCommandPresetPanel)
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/string.h>
//*)

//(*IdInit(EventFPPCommandPresetPanel)
const long EventFPPCommandPresetPanel::ID_STATICTEXT1 = wxNewId();
const long EventFPPCommandPresetPanel::ID_TEXTCTRL_PRESET_NAME = wxNewId();
//*)

BEGIN_EVENT_TABLE(EventFPPCommandPresetPanel,wxPanel)
	//(*EventTable(EventFPPCommandPresetPanel)
	//*)
END_EVENT_TABLE()

EventFPPCommandPresetPanel::EventFPPCommandPresetPanel(wxWindow* parent,wxWindowID id,const wxPoint& pos,const wxSize& size)
{
	//(*Initialize(EventFPPCommandPresetPanel)
	wxFlexGridSizer* FlexGridSizer1;

	Create(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("id"));
	FlexGridSizer1 = new wxFlexGridSizer(0, 2, 0, 0);
	FlexGridSizer1->AddGrowableCol(1);
	StaticText1 = new wxStaticText(this, ID_STATICTEXT1, _("Preset Name:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
	FlexGridSizer1->Add(StaticText1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	TextCtrl_PresetName = new wxTextCtrl(this, ID_TEXTCTRL_PRESET_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, _T("ID_TEXTCTRL_PRESET_NAME"));
	FlexGridSizer1->Add(TextCtrl_PresetName, 1, wxALL|wxEXPAND, 5);
	SetSizer(FlexGridSizer1);
	FlexGridSizer1->Fit(this);
	FlexGridSizer1->SetSizeHints(this);
	//*)

    // a note above the wxSmith layout, added here so the generated code stays as generated
    auto* note = new wxStaticText(this, wxID_ANY, _("Fires when an FPP sends Trigger Command Preset (enter the preset name) or Trigger Command Preset Slot (enter the slot number)."));
    note->Wrap(FromDIP(340));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(note, 0, wxALL, 5);
    SetSizer(nullptr, false);
    outer->Add(FlexGridSizer1, 1, wxEXPAND);
    SetSizer(outer);
    outer->SetSizeHints(this);
}

EventFPPCommandPresetPanel::~EventFPPCommandPresetPanel()
{
	//(*Destroy(EventFPPCommandPresetPanel)
	//*)
}

void EventFPPCommandPresetPanel::Save(EventBase* event)
{
    EventFPPCommandPreset* e = (EventFPPCommandPreset*)event;
    e->SetEventPresetName(TextCtrl_PresetName->GetValue().ToStdString());
}

void EventFPPCommandPresetPanel::Load(EventBase* event)
{
    EventFPPCommandPreset* e = (EventFPPCommandPreset*)event;
    TextCtrl_PresetName->SetValue(e->GetEventPresetName());
}
