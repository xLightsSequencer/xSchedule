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

#include "ShowBackups.h"

#include <wx/dialog.h>

#include <filesystem>
#include <string>
#include <vector>

class wxButton;
class wxListView;

// File > Restore from Backup: the show file's backups, newest first, to pick one to put back
class RestoreBackupDialog : public wxDialog {
public:
    RestoreBackupDialog(wxWindow* parent, const std::string& showDir, const std::string& showFile);

    std::filesystem::path GetChosen() const { return _chosen; }
    // when the chosen backup was made, as the list shows it
    wxString GetChosenWhen() const { return _chosenWhen; }

private:
    void Choose();

    std::string _showDir;
    std::vector<ShowBackups::Backup> _backups;
    wxListView* _list = nullptr;
    wxButton* _restore = nullptr;
    std::filesystem::path _chosen;
    wxString _chosenWhen;
};
