/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "PlayListItemBranchPanel.h"
#include "PlayListItemBranch.h"
#include "PlayListDialog.h"
#include "PlayList.h"
#include "PlayListStep.h"
#include "../ScheduleManager.h"
#include "../xScheduleMain.h"

//(*InternalHeaders(PlayListItemBranchPanel)
#include <wx/intl.h>
#include <wx/string.h>
//*)

namespace {
const wxString NEXT_STEP = "(the next step)";

void SelectOrAdd(wxChoice* choice, const wxString& value)
{
    if (!choice->SetStringSelection(value)) {
        choice->SetSelection(choice->Append(value));
    }
}
}

//(*IdInit(PlayListItemBranchPanel)
const long PlayListItemBranchPanel::ID_STATICTEXT_HELP = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT1 = wxNewId();
const long PlayListItemBranchPanel::ID_CHOICE_CONDITION = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT2 = wxNewId();
const long PlayListItemBranchPanel::ID_TEXTCTRL_STARTTIME = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT3 = wxNewId();
const long PlayListItemBranchPanel::ID_TEXTCTRL_ENDTIME = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT4 = wxNewId();
const long PlayListItemBranchPanel::ID_SPINCTRL_LOOPEVERY = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT5 = wxNewId();
const long PlayListItemBranchPanel::ID_SPINCTRL_LOOPFROM = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT6 = wxNewId();
const long PlayListItemBranchPanel::ID_CHOICE_TRUEACTION = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT7 = wxNewId();
const long PlayListItemBranchPanel::ID_CHOICE_TRUETARGET = wxNewId();
const long PlayListItemBranchPanel::ID_STATICTEXT8 = wxNewId();
const long PlayListItemBranchPanel::ID_CHOICE_FALSESTEP = wxNewId();
//*)

BEGIN_EVENT_TABLE(PlayListItemBranchPanel,wxPanel)
	//(*EventTable(PlayListItemBranchPanel)
	//*)
END_EVENT_TABLE()

PlayListItemBranchPanel::PlayListItemBranchPanel(wxWindow* parent, PlayListItemBranch* branch, wxWindowID id,const wxPoint& pos,const wxSize& size)
{
    _branch = branch;

	//(*Initialize(PlayListItemBranchPanel)
	wxBoxSizer* BoxSizer1;
	wxBoxSizer* BoxSizer2;
	wxFlexGridSizer* FlexGridSizer1;
	wxFlexGridSizer* FlexGridSizer2;

	Create(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _T("id"));
	FlexGridSizer1 = new wxFlexGridSizer(0, 1, 0, 0);
	FlexGridSizer1->AddGrowableCol(0);
	StaticText_Help = new wxStaticText(this, ID_STATICTEXT_HELP, _("This step doesn\'t play. It picks the step that plays next."), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT_HELP"));
	FlexGridSizer1->Add(StaticText_Help, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	FlexGridSizer2 = new wxFlexGridSizer(0, 2, 0, 0);
	FlexGridSizer2->AddGrowableCol(1);
	StaticText1 = new wxStaticText(this, ID_STATICTEXT1, _("If:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT1"));
	FlexGridSizer2->Add(StaticText1, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	Choice_Condition = new wxChoice(this, ID_CHOICE_CONDITION, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE_CONDITION"));
	Choice_Condition->SetSelection( Choice_Condition->Append(_("The time is between")) );
	Choice_Condition->Append(_("It is every nth loop"));
	Choice_Condition->Append(_("Always"));
	FlexGridSizer2->Add(Choice_Condition, 1, wxALL|wxEXPAND, 5);
	StaticText2 = new wxStaticText(this, ID_STATICTEXT2, _("Between:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT2"));
	FlexGridSizer2->Add(StaticText2, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	BoxSizer1 = new wxBoxSizer(wxHORIZONTAL);
	TextCtrl_StartTime = new wxTextCtrl(this, ID_TEXTCTRL_STARTTIME, _("21:00"), wxDefaultPosition, wxDefaultSize, wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL_STARTTIME"));
	TextCtrl_StartTime->SetToolTip(_("24 hour time, HH:MM. A window can run past midnight, and the same time twice means all day."));
	BoxSizer1->Add(TextCtrl_StartTime, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	StaticText3 = new wxStaticText(this, ID_STATICTEXT3, _("and"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT3"));
	BoxSizer1->Add(StaticText3, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	TextCtrl_EndTime = new wxTextCtrl(this, ID_TEXTCTRL_ENDTIME, _("00:00"), wxDefaultPosition, wxDefaultSize, wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL_ENDTIME"));
	TextCtrl_EndTime->SetToolTip(_("24 hour time, HH:MM. A window can run past midnight, and the same time twice means all day."));
	BoxSizer1->Add(TextCtrl_EndTime, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	FlexGridSizer2->Add(BoxSizer1, 1, wxEXPAND, 0);
	StaticText4 = new wxStaticText(this, ID_STATICTEXT4, _("Every:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT4"));
	FlexGridSizer2->Add(StaticText4, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	BoxSizer2 = new wxBoxSizer(wxHORIZONTAL);
	SpinCtrl_LoopEvery = new wxSpinCtrl(this, ID_SPINCTRL_LOOPEVERY, _T("2"), wxDefaultPosition, wxDefaultSize, 0, 1, 99, 2, _T("ID_SPINCTRL_LOOPEVERY"));
	SpinCtrl_LoopEvery->SetValue(_T("2"));
	BoxSizer2->Add(SpinCtrl_LoopEvery, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	StaticText5 = new wxStaticText(this, ID_STATICTEXT5, _("loops, starting with loop"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT5"));
	BoxSizer2->Add(StaticText5, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	SpinCtrl_LoopFrom = new wxSpinCtrl(this, ID_SPINCTRL_LOOPFROM, _T("2"), wxDefaultPosition, wxDefaultSize, 0, 1, 99, 2, _T("ID_SPINCTRL_LOOPFROM"));
	SpinCtrl_LoopFrom->SetValue(_T("2"));
	BoxSizer2->Add(SpinCtrl_LoopFrom, 1, wxALL|wxALIGN_CENTER_VERTICAL, 5);
	FlexGridSizer2->Add(BoxSizer2, 1, wxEXPAND, 0);
	StaticText6 = new wxStaticText(this, ID_STATICTEXT6, _("Then:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT6"));
	FlexGridSizer2->Add(StaticText6, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	Choice_TrueAction = new wxChoice(this, ID_CHOICE_TRUEACTION, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE_TRUEACTION"));
	Choice_TrueAction->SetSelection( Choice_TrueAction->Append(_("Go to step")) );
	Choice_TrueAction->Append(_("Play playlist, then carry on"));
	FlexGridSizer2->Add(Choice_TrueAction, 1, wxALL|wxEXPAND, 5);
	StaticText7 = new wxStaticText(this, ID_STATICTEXT7, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT7"));
	FlexGridSizer2->Add(StaticText7, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	Choice_TrueTarget = new wxChoice(this, ID_CHOICE_TRUETARGET, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE_TRUETARGET"));
	FlexGridSizer2->Add(Choice_TrueTarget, 1, wxALL|wxEXPAND, 5);
	StaticText8 = new wxStaticText(this, ID_STATICTEXT8, _("Otherwise go to:"), wxDefaultPosition, wxDefaultSize, 0, _T("ID_STATICTEXT8"));
	FlexGridSizer2->Add(StaticText8, 1, wxALL|wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL, 5);
	Choice_FalseStep = new wxChoice(this, ID_CHOICE_FALSESTEP, wxDefaultPosition, wxDefaultSize, 0, 0, 0, wxDefaultValidator, _T("ID_CHOICE_FALSESTEP"));
	FlexGridSizer2->Add(Choice_FalseStep, 1, wxALL|wxEXPAND, 5);
	FlexGridSizer1->Add(FlexGridSizer2, 1, wxALL|wxEXPAND, 5);
	SetSizer(FlexGridSizer1);
	FlexGridSizer1->Fit(this);
	FlexGridSizer1->SetSizeHints(this);

	Connect(ID_CHOICE_CONDITION,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&PlayListItemBranchPanel::OnChoice_ConditionSelect);
	Connect(ID_CHOICE_TRUEACTION,wxEVT_COMMAND_CHOICE_SELECTED,(wxObjectEventFunction)&PlayListItemBranchPanel::OnChoice_TrueActionSelect);
	//*)

    Choice_Condition->SetSelection((int)branch->GetCondition());
    TextCtrl_StartTime->SetValue(branch->GetStartTime());
    TextCtrl_EndTime->SetValue(branch->GetEndTime());
    SpinCtrl_LoopEvery->SetValue(branch->GetLoopEvery());
    SpinCtrl_LoopFrom->SetValue(branch->GetLoopFrom());
    Choice_TrueAction->SetSelection(branch->GetTruePlayList().empty() ? 0 : 1);
    FillTrueTargets();

    Choice_FalseStep->Append(NEXT_STEP);
    auto dlg = dynamic_cast<PlayListDialog*>(wxGetTopLevelParent(parent));
    if (dlg != nullptr && dlg->GetEditedPlayList() != nullptr) {
        for (auto step : dlg->GetEditedPlayList()->GetSteps()) {
            Choice_FalseStep->Append(step->GetNameNoTime());
        }
    }
    if (branch->GetFalseStep().empty()) {
        Choice_FalseStep->SetSelection(0);
    } else {
        SelectOrAdd(Choice_FalseStep, branch->GetFalseStep());
    }

    ValidateWindow();
}

void PlayListItemBranchPanel::FillTrueTargets()
{
    Choice_TrueTarget->Clear();
    if (Choice_TrueAction->GetSelection() == 1) {
        StaticText7->SetLabel(_("Playlist:"));
        for (auto pl : xScheduleFrame::GetScheduleManager()->GetPlayLists()) {
            Choice_TrueTarget->Append(pl->GetNameNoTime());
        }
        if (!_branch->GetTruePlayList().empty()) SelectOrAdd(Choice_TrueTarget, _branch->GetTruePlayList());
    } else {
        StaticText7->SetLabel(_("Step:"));
        auto dlg = dynamic_cast<PlayListDialog*>(wxGetTopLevelParent(GetParent()));
        if (dlg != nullptr && dlg->GetEditedPlayList() != nullptr) {
            for (auto step : dlg->GetEditedPlayList()->GetSteps()) {
                Choice_TrueTarget->Append(step->GetNameNoTime());
            }
        }
        if (!_branch->GetTrueStep().empty()) SelectOrAdd(Choice_TrueTarget, _branch->GetTrueStep());
    }
    if (Choice_TrueTarget->GetSelection() == wxNOT_FOUND && Choice_TrueTarget->GetCount() > 0) {
        Choice_TrueTarget->SetSelection(0);
    }
}

PlayListItemBranchPanel::~PlayListItemBranchPanel()
{
	//(*Destroy(PlayListItemBranchPanel)
	//*)
    _branch->SetCondition((PlayListItemBranch::Condition)Choice_Condition->GetSelection());
    _branch->SetStartTime(TextCtrl_StartTime->GetValue().Trim().Trim(false).ToStdString());
    _branch->SetEndTime(TextCtrl_EndTime->GetValue().Trim().Trim(false).ToStdString());
    _branch->SetLoopEvery(SpinCtrl_LoopEvery->GetValue());
    _branch->SetLoopFrom(SpinCtrl_LoopFrom->GetValue());
    std::string target = Choice_TrueTarget->GetStringSelection().ToStdString();
    if (Choice_TrueAction->GetSelection() == 1) {
        _branch->SetTruePlayList(target);
        _branch->SetTrueStep("");
    } else {
        _branch->SetTrueStep(target);
        _branch->SetTruePlayList("");
    }
    _branch->SetFalseStep(Choice_FalseStep->GetSelection() <= 0 ? "" : Choice_FalseStep->GetStringSelection().ToStdString());
}

void PlayListItemBranchPanel::ValidateWindow()
{
    bool time = Choice_Condition->GetSelection() == 0;
    bool loop = Choice_Condition->GetSelection() == 1;
    TextCtrl_StartTime->Enable(time);
    TextCtrl_EndTime->Enable(time);
    SpinCtrl_LoopEvery->Enable(loop);
    SpinCtrl_LoopFrom->Enable(loop);
    Choice_FalseStep->Enable(Choice_Condition->GetSelection() != 2);
}

void PlayListItemBranchPanel::OnChoice_ConditionSelect(wxCommandEvent& event)
{
    ValidateWindow();
}

void PlayListItemBranchPanel::OnChoice_TrueActionSelect(wxCommandEvent& event)
{
    FillTrueTargets();
}
