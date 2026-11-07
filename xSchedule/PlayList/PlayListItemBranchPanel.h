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

//(*Headers(PlayListItemBranchPanel)
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
//*)

class PlayListItemBranch;

class PlayListItemBranchPanel: public wxPanel
{
    PlayListItemBranch* _branch;

    void ValidateWindow();
    void FillTrueTargets();

	public:

		PlayListItemBranchPanel(wxWindow* parent, PlayListItemBranch* branch, wxWindowID id=wxID_ANY,const wxPoint& pos=wxDefaultPosition,const wxSize& size=wxDefaultSize);
		virtual ~PlayListItemBranchPanel();

		//(*Declarations(PlayListItemBranchPanel)
		wxChoice* Choice_Condition;
		wxChoice* Choice_FalseStep;
		wxChoice* Choice_TrueAction;
		wxChoice* Choice_TrueTarget;
		wxSpinCtrl* SpinCtrl_LoopEvery;
		wxSpinCtrl* SpinCtrl_LoopFrom;
		wxStaticText* StaticText1;
		wxStaticText* StaticText2;
		wxStaticText* StaticText3;
		wxStaticText* StaticText4;
		wxStaticText* StaticText5;
		wxStaticText* StaticText6;
		wxStaticText* StaticText7;
		wxStaticText* StaticText8;
		wxStaticText* StaticText_Help;
		wxTextCtrl* TextCtrl_EndTime;
		wxTextCtrl* TextCtrl_StartTime;
		//*)

	protected:

		//(*Identifiers(PlayListItemBranchPanel)
		static const long ID_STATICTEXT_HELP;
		static const long ID_STATICTEXT1;
		static const long ID_CHOICE_CONDITION;
		static const long ID_STATICTEXT2;
		static const long ID_TEXTCTRL_STARTTIME;
		static const long ID_STATICTEXT3;
		static const long ID_TEXTCTRL_ENDTIME;
		static const long ID_STATICTEXT4;
		static const long ID_SPINCTRL_LOOPEVERY;
		static const long ID_STATICTEXT5;
		static const long ID_SPINCTRL_LOOPFROM;
		static const long ID_STATICTEXT6;
		static const long ID_CHOICE_TRUEACTION;
		static const long ID_STATICTEXT7;
		static const long ID_CHOICE_TRUETARGET;
		static const long ID_STATICTEXT8;
		static const long ID_CHOICE_FALSESTEP;
		//*)

	private:

		//(*Handlers(PlayListItemBranchPanel)
		void OnChoice_ConditionSelect(wxCommandEvent& event);
		void OnChoice_TrueActionSelect(wxCommandEvent& event);
		//*)

		DECLARE_EVENT_TABLE()
};
