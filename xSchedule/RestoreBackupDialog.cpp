/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "RestoreBackupDialog.h"

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>
#include <wx/xml/xml.h>

#include <algorithm>

namespace {

wxString When(const std::tm& t)
{
    wxDateTime d((wxDateTime::wxDateTime_t)t.tm_mday, (wxDateTime::Month)t.tm_mon, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec);
    return d.Format("%a, ") + d.FormatDate() + "  " + d.FormatTime();
}

wxString Size(uintmax_t bytes)
{
    if (bytes < 1024) return wxString::Format("%d bytes", (int)bytes);
    return wxString::Format("%.1f KB", bytes / 1024.0);
}

// the playlists and schedules a backup holds
void Count(const std::filesystem::path& file, int& playlists, int& schedules)
{
    playlists = schedules = 0;
    wxLogNull quiet;
    wxXmlDocument doc;
    if (!doc.Load(wxString(file.wstring())) || doc.GetRoot() == nullptr) {
        playlists = schedules = -1;
        return;
    }
    for (wxXmlNode* n = doc.GetRoot()->GetChildren(); n != nullptr; n = n->GetNext()) {
        if (n->GetName() != "PlayList") continue;
        ++playlists;
        for (wxXmlNode* s = n->GetChildren(); s != nullptr; s = s->GetNext()) {
            if (s->GetName() == "Schedule") ++schedules;
        }
    }
}

} // namespace

RestoreBackupDialog::RestoreBackupDialog(wxWindow* parent, const std::string& showDir, const std::string& showFile) :
    wxDialog(parent, wxID_ANY, "Restore from Backup", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), _showDir(showDir)
{
    _backups = ShowBackups::List(showDir, showFile);

    auto* top = new wxBoxSizer(wxVERTICAL);
    wxString intro = _backups.empty() ? wxString("There are no backups yet. One is kept each time the show is saved.")
                                      : wxString("The show as it was before each save, newest first. Restoring a backup keeps a copy of the current show.");
    auto* text = new wxStaticText(this, wxID_ANY, intro);
    text->Wrap(FromDIP(560));
    top->Add(text, 0, wxALL, FromDIP(12));

    _list = new wxListView(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(600, 320)), wxLC_REPORT | wxLC_SINGLE_SEL);
    _list->SetName("Backups");
    _list->AppendColumn("Saved");
    _list->AppendColumn("Playlists", wxLIST_FORMAT_RIGHT);
    _list->AppendColumn("Schedules", wxLIST_FORMAT_RIGHT);
    _list->AppendColumn("Size", wxLIST_FORMAT_RIGHT);
    for (const auto& b : _backups) {
        int playlists;
        int schedules;
        Count(b.file, playlists, schedules);
        long i = _list->InsertItem(_list->GetItemCount(), When(b.when));
        _list->SetItem(i, 1, playlists < 0 ? wxString("?") : wxString::Format("%d", playlists));
        _list->SetItem(i, 2, schedules < 0 ? wxString("?") : wxString::Format("%d", schedules));
        _list->SetItem(i, 3, Size(b.size));
    }
    // wide enough for the header and the contents; sizing the last column to its header would stretch it to the edge
    for (int c = 0; c < 4; ++c) {
        wxListItem header;
        header.SetMask(wxLIST_MASK_TEXT);
        _list->GetColumn(c, header);
        int width = _list->GetTextExtent(header.GetText()).x + FromDIP(24);
        if (!_backups.empty()) {
            _list->SetColumnWidth(c, wxLIST_AUTOSIZE);
            width = std::max(width, _list->GetColumnWidth(c) + FromDIP(8));
        }
        _list->SetColumnWidth(c, width);
    }
    top->Add(_list, 1, wxLEFT | wxRIGHT | wxEXPAND, FromDIP(12));

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* folder = new wxButton(this, wxID_ANY, "Open Backup Folder");
    folder->Enable(std::filesystem::exists(ShowBackups::Folder(showDir)));
    buttons->Add(folder, 0, wxRIGHT, FromDIP(8));
    buttons->AddStretchSpacer();
    _restore = new wxButton(this, wxID_OK, "Restore");
    _restore->Enable(false);
    buttons->Add(_restore, 0, wxRIGHT, FromDIP(8));
    buttons->Add(new wxButton(this, wxID_CANCEL, "Close"));
    top->Add(buttons, 0, wxALL | wxEXPAND, FromDIP(12));
    SetSizerAndFit(top);
    SetEscapeId(wxID_CANCEL);

    _list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { _restore->Enable(true); });
    _list->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) { _restore->Enable(_list->GetFirstSelected() >= 0); });
    _list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { Choose(); });
    _restore->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Choose(); });
    folder->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { wxLaunchDefaultApplication(wxString(ShowBackups::Folder(_showDir).wstring())); });
    if (!_backups.empty()) _list->SetFocus();
}

void RestoreBackupDialog::Choose()
{
    long i = _list->GetFirstSelected();
    if (i < 0 || i >= (long)_backups.size()) return;
    _chosen = _backups[i].file;
    _chosenWhen = _list->GetItemText(i);
    EndModal(wxID_OK);
}
