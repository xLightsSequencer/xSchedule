/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <wx/config.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/mimetype.h>
#include <wx/msgdlg.h>
#include <wx/sckaddr.h>
#include <wx/socket.h>
#include <wx/xml/xml.h>

#include "OutputProcess.h"
#include "OutputProcessExcludeDim.h"
#include "Pinger.h"
#include "RunningSchedule.h"
#include "ScheduleManager.h"
#include "ScheduleOptions.h"
#include "UserButton.h"
#include "Xyzzy.h"
#include "xScheduleApp.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"
#include "xScheduleMain.h"
#include "../xlights/src-core/media/AudioManager.h"
#include "../xlights/src-core/utils/Parallel.h"
#include "../xlights/src-core/utils/UtilFunctions.h"
#include "ScheduleConfig.h"
#include "../xlights/src-core/utils/FileUtils.h"
#include "../xlights/src-core/utils/ip_utils.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"
#include "../xlights/src-core/media/VideoReader.h"
#include "../xlights/src-core/outputs/Controller.h"
#include "../xlights/src-core/outputs/ControllerEthernet.h"
#include "../xlights/src-core/outputs/IPOutput.h"
#include "../xlights/src-core/outputs/Output.h"
#include "../xlights/src-core/outputs/OutputManager.h"
#include <atomic>
#include "xScheduleVersion.h"
#include "PlayList/PlayList.h"
#include "PlayList/PlayListItemAudio.h"
#include "PlayList/PlayListItemFSEQ.h"
#include "PlayList/PlayListItemFSEQVideo.h"
#include "PlayList/PlayListItemText.h"
#include "PlayList/PlayListItemVideo.h"
#include "PlayList/PlayListStep.h"
#include "events/ListenerManager.h"
#include <wx/stdpaths.h>

#include <memory>

#include <nlohmann/json.hpp>

#include <log.h>
#include "Diagnostics.h"
#include "ScheduleCalendar.h"
#include "ShowBackups.h"
#include "../xlights/src-ui-wx/settings/XLightsConfigAdapter.h"

ScheduleManager::ScheduleManager(xScheduleFrame* frame, const std::string& showDir) {
    spdlog::info("Loading schedule from {}.", showDir);

    // prime fix file with our show directory for any filename fixups
    SetFixFileShowDir(showDir);
    // and the media folders set up in xLights, for audio a moved show no longer finds at its saved path
    FileUtils::SetFixFileDirectories(ScheduleConfig::GetXLightsMediaDirs());

    _syncManager = std::make_unique<SyncManager>(this);
    _testMode = false;
    _mainThread = wxThread::GetCurrentId();
    _listenerManager = nullptr;
    _pinger = nullptr;
    _webRequestToggle = false;
    _backgroundPlayList = nullptr;
    _queuedSongs = new PlayList();
    _queuedSongs->SetName("Song Queue");
    _fppSyncMaster = nullptr;
    _midiMaster = nullptr;
    _artNetSyncMaster = nullptr;
    _fppSyncMasterUnicast = nullptr;
    _manualOTL = -1;
    _immediatePlay = nullptr;
    _scheduleOptions = nullptr;
    _showDir = showDir;
    _startTime = wxGetUTCTimeMillis().GetLo();
    _outputManager = nullptr;
    _buffer = nullptr;
    _brightness = 100;
    _lastBrightness = 100;
    _xyzzy = nullptr;
    _timerAdjustment = 0;
    _lastXyzzyCommand = wxDateTime::Now();
    _outputManager = new OutputManager();

    _mode = (int)SYNCMODE::STANDALONE;
    _remoteMode = REMOTEMODE::DISABLED;
    auto* config = GetXLightsConfig();
    if (config != nullptr) {
        _mode = config->ReadLong("SyncMode", (int)SYNCMODE::STANDALONE);
        _remoteMode = (REMOTEMODE)config->ReadLong("RemoteMode", (int)REMOTEMODE::DISABLED);
    }

    wxLogNull logNo; // kludge: avoid "error 0" message from wxWidgets after new file is written
    _lastSavedChangeCount = 0;
    _changeCount = 0;
    wxXmlDocument doc;
    doc.Load(showDir + "/" + GetScheduleFile());

    std::string backgroundPlayList = "";

    if (doc.IsOk()) {
        for (wxXmlNode* n = doc.GetRoot()->GetChildren(); n != nullptr; n = n->GetNext()) {
            if (n->GetName() == "PlayList") {
                _playLists.push_back(new PlayList(_outputManager, n));
            } else if (n->GetName() == "Options") {
                _scheduleOptions = new ScheduleOptions(_outputManager, n, GetCommandManager());
                _outputManager->SetParallelTransmission(_scheduleOptions->IsParallelTransmission());
                OutputManager::SetRetryOpen(_scheduleOptions->IsRetryOpen());
                _outputManager->SetSyncEnabled(_scheduleOptions->IsSync());
                _scheduleOptions->ApplyLocation();
            } else if (n->GetName() == "OutputProcesses") {
                for (wxXmlNode* n1 = n->GetChildren(); n1 != nullptr; n1 = n1->GetNext()) {
                    OutputProcess* op = OutputProcess::CreateFromXml(_outputManager, n1);
                    if (op != nullptr) {
                        _outputProcessing.push_back(op);
                    }
                }
            } else if (n->GetName() == "Background") {
                backgroundPlayList = n->GetAttribute("PlayList", "");
            }
        }
    } else {
        spdlog::error("Problem loading xml file {}.", (showDir + "/" + GetScheduleFile()));
    }

    if (backgroundPlayList != "" && GetPlayList(backgroundPlayList) != nullptr) {
        _backgroundPlayList = new PlayList(*GetPlayList(backgroundPlayList));
        spdlog::debug("Background playlist loaded. {}.", _backgroundPlayList->GetNameNoTime());
    }

    if (_scheduleOptions == nullptr) {
        _scheduleOptions = new ScheduleOptions();
        _scheduleOptions->ApplyLocation();
        _outputManager->SetParallelTransmission(_scheduleOptions->IsParallelTransmission());
        _outputManager->SetSyncEnabled(_scheduleOptions->IsSync());
        OutputManager::SetRetryOpen(_scheduleOptions->IsRetryOpen());
    }

    VideoReader::SetHardwareAcceleratedVideo(_scheduleOptions->IsHardwareAcceleratedVideo());

    if (VideoReader::IsHardwareAcceleratedVideo()) {
        spdlog::debug("Hardware accelerated video enabled.");
    } else {
        spdlog::debug("Hardware accelerated video disabled.");
    }

    _outputManager->Load(_showDir, _scheduleOptions->IsSync());
    spdlog::info("Loaded outputs from {}.", _showDir + "/" + _outputManager->GetNetworksFileName());

    SetForceLocalIP(GetForceLocalIP());

    if (_scheduleOptions->IsSendOffWhenNotRunning()) {
        if (!_outputManager->IsOutputting()) {
            if (ScheduleConfig::GetBool("OutputActive", false)) {
                spdlog::warn("Warning: Lights output is already open in another process. This will cause issues.");
            }
            DisableRemoteOutputs();
            if (StartOutputToLights()) ScheduleConfig::SetBool("OutputActive", true);
#ifdef __WXMSW__
            ::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif
        }
        ManageBackground();
        spdlog::info("Started outputting to lights ... even though nothing is running.");
        StartVirtualMatrices();
    }

    _listenerManager = new ListenerManager(this);

    _syncManager->Start(_mode, _remoteMode, GetForceLocalIP());

    // This is out frame data buffer ... it cannot be resized
    spdlog::info("Allocated frame buffer of {} bytes", _outputManager->GetTotalChannels());
    _buffer = (uint8_t*)malloc(_outputManager->GetTotalChannels());
    memset(_buffer, 0x00, _outputManager->GetTotalChannels());

#ifdef __WXMSW__
    unsigned long state = ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED;
    if (_scheduleOptions->IsKeepScreenOn()) {
        state |= ES_DISPLAY_REQUIRED;
    }
    ::SetThreadExecutionState(state);
#endif
}

void ScheduleManager::AddPlayList(PlayList* playlist) {
    _playLists.push_back(playlist);
    _changeCount++;
}

std::list<PlayList*> ScheduleManager::GetPlayLists() {
    return _playLists;
}

int ScheduleManager::GetPPS() const {
    if (_outputManager != nullptr) {
        return _outputManager->GetPacketsPerSecond();
    }

    return 0;
}

void ScheduleManager::StartListeners() {
    _listenerManager->StartListeners(GetForceLocalIP());
}

int ScheduleManager::Sync(const std::string& filename, long ms) {
    wxCommandEvent event(EVT_SYNC);
    event.SetString(filename);
    event.SetInt(ms);
    wxPostEvent(wxGetApp().GetTopWindow(), event);
    return 50; // this is a problem
}

int ScheduleManager::DoSync(const std::string& filename, long ms) {
    // spdlog::debug("DoSync Enter");

    PlayList* pl = GetRunningPlayList();
    PlayListStep* pls = nullptr;

    // adjust the time we received by the desired latency
    if (ms >= 0) {
        ms += GetOptions()->GetRemoteLatency();
    }

    if (filename != "" && pl != nullptr && pl->GetRunningStep() != nullptr && pl->GetRunningStep()->GetNameNoTime() == filename) {
        // right step is running
        pls = pl->GetRunningStep();
    } else if (filename == "" && pl != nullptr) {
        // dont touch the playlist but need to work out which step should be playing
        long stepMS = 0;
        PlayListStep* shouldberunning = pl->GetStepAtTime(ms, stepMS);
        ms = stepMS;
        if (shouldberunning != nullptr) {
            if (shouldberunning != pl->GetRunningStep()) {
                spdlog::debug("Remote sync with no filename ... wrong step was running '{}' switching to '{}' at {}ms.", pl->GetRunningStep()->GetNameNoTime(), shouldberunning->GetNameNoTime(), ms);
                pl->JumpToStep(shouldberunning->GetNameNoTime());
                wxCommandEvent event2(EVT_SCHEDULECHANGED);
                wxPostEvent(wxGetApp().GetTopWindow(), event2);
            }
            if (pl->GetRunningStep() != nullptr) {
                pl->GetRunningStep()->SetSyncPosition(ms, GetOptions()->GetRemoteAcceptableJitter(), true);
            }
        } else {
            if (ms == 0xFFFFFFFE || ms == -2) {
                pl->Suspend(true);
            } else if (ms == 0xFFFFFFFD || ms == -3) {
                pl->Suspend(false);
            } else {
                spdlog::debug("Remote sync with no filename ... playlist was not sufficiently long for received sync position {}.", ms);
                pl->Stop();
            }
        }
    } else if (filename == "" && pl == nullptr) {
        if (_playLists.size() > 0) {
            // we only access the first playlist for timecode
            pl = new PlayList(*_playLists.front());
            long stepMS = 0;
            PlayListStep* shouldberunning = pl->GetStepAtTime(ms, stepMS);
            ms = stepMS;
            if (shouldberunning == nullptr) {
                spdlog::debug("Remote sync with no filename ... playlist was not sufficiently long for received sync position {}ms.", ms);
                delete pl;
                pl = nullptr;
            } else {
                spdlog::debug("Remote sync with no filename ... starting playlist '{}' step '{}' at {}ms.", pl->GetNameNoTime(), shouldberunning->GetNameNoTime(), ms);
                pl->Start(false, false, false);
                pl->JumpToStep(shouldberunning->GetNameNoTime());
                if (pl->GetRunningStep() != nullptr) {
                    pl->GetRunningStep()->SetSyncPosition(ms, GetOptions()->GetRemoteAcceptableJitter(), true);
                }
                _immediatePlay = pl;
                wxCommandEvent event2(EVT_SCHEDULECHANGED);
                wxPostEvent(wxGetApp().GetTopWindow(), event2);
            }
        } else {
            spdlog::warn("Remote sync with no filename ... No playlist found to run.");
        }
    } else {
        if (pl != nullptr) {
            StopPlayList(pl, false);
            pl = nullptr;
        }

        // need to start the playlist step
        if (filename != "") {
            StartStep(filename);
        }
        pl = GetRunningPlayList();
        if (pl != nullptr)
            pls = pl->GetRunningStep();
        wxCommandEvent event2(EVT_SCHEDULECHANGED);
        wxPostEvent(wxGetApp().GetTopWindow(), event2);
    }

    if (pls != nullptr) {
        if (ms == 0xFFFFFFFF || ms == -1) {
            if (pls->GetNameNoTime() == filename) {
                wxCommandEvent event(EVT_STOP);
                event.SetInt(pl->GetId());
                if (!GetOptions()->IsRemoteAllOff()) {
                    event.SetString("sustain");
                }
                wxPostEvent(wxGetApp().GetTopWindow(), event);
            }
        } else if (ms == 0xFFFFFFFE || ms == -2) {
            // pause
            if (pls->GetNameNoTime() == filename) {
                pl->Suspend(true);
            }
        } else if (ms == 0xFFFFFFFD || ms == -3) {
            // unpause
            if (pls->GetNameNoTime() == filename) {
                pl->Suspend(false);
            }
        } else {
            pls->SetSyncPosition((size_t)ms, GetOptions()->GetRemoteAcceptableJitter(), true);
        }
    }

    if (pls != nullptr) {
        _listenerManager->SetFrameMS(pls->GetFrameMS());
        // spdlog::debug("DoSync Leave");
        return pls->GetFrameMS();
    }
    if (pl != nullptr) {
        _listenerManager->SetFrameMS(pl->GetFrameMS());
        // spdlog::debug("DoSync Leave");
        return pl->GetFrameMS();
    }
    _listenerManager->SetFrameMS(50);
    // spdlog::debug("DoSync Leave");
    return 50;
}

ScheduleManager::~ScheduleManager() {
    spdlog::debug("ScheduleManager destructor starting.");
    AllOff();
    spdlog::debug("ScheduleManager destructor: stopping output.");
    _outputManager->StopOutput();
    ScheduleConfig::SetBool("OutputActive", false);
#ifdef __WXMSW__
    ::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
    spdlog::debug("ScheduleManager destructor: stopping virtual matrices.");
    StopVirtualMatrices();
    ManageBackground();
    spdlog::info("Stopped outputting to lights.");

    if (IsDirty()) {
        spdlog::debug("ScheduleManager destructor: schedule is dirty, prompting to save.");
        if (wxMessageBox("Unsaved changes to the schedule. Save now?", "Unsaved changes", wxYES_NO) == wxYES) {
            if (!Save()) {
                wxMessageBox("The schedule could not be saved to " + _showDir + ". See the log for details.", "Save failed", wxOK | wxICON_ERROR);
            }
        }
    }

    spdlog::debug("ScheduleManager destructor: stopping sync manager.");
    _syncManager->Stop(GetForceLocalIP());

    if (_listenerManager != nullptr) {
        spdlog::debug("ScheduleManager destructor: stopping listener manager.");
        _listenerManager->Stop();
        delete _listenerManager;
        spdlog::debug("ScheduleManager destructor: listener manager stopped.");
    }

    while (_overlayData.size() > 0) {
        auto todelete = _overlayData.front();
        _overlayData.remove(todelete);
        delete todelete;
    }

    if (_xyzzy != nullptr) {
        wxString res;
        _xyzzy->Close(res, "");

        // clear the screen
        _xyzzy->DrawBlack(_buffer, _outputManager->GetTotalChannels());

        delete _xyzzy;
        _xyzzy = nullptr;
    }

    if (_backgroundPlayList != nullptr) {
        spdlog::debug("Background playlist stopped and deleted. {}.", _backgroundPlayList->GetNameNoTime());
        _backgroundPlayList->Stop();
        delete _backgroundPlayList;
        _backgroundPlayList = nullptr;
    }

    while (_eventPlayLists.size() > 0) {
        _eventPlayLists.front()->Stop();
        delete _eventPlayLists.front();
        _eventPlayLists.pop_front();
    }

    while (_outputProcessing.size() > 0) {
        auto toremove = _outputProcessing.front();
        _outputProcessing.remove(toremove);
        delete toremove;
    }

    while (_playLists.size() > 0) {
        auto toremove = _playLists.front();
        _playLists.remove(toremove);
        delete toremove;
    }

    if (_immediatePlay != nullptr) {
        delete _immediatePlay;
        _immediatePlay = nullptr;
    }
    ClearInterruptedPlays();

    if (_queuedSongs != nullptr) {
        delete _queuedSongs;
        _queuedSongs = nullptr;
    }

    while (_activeSchedules.size() > 0) {
        auto toremove = _activeSchedules.front();
        _activeSchedules.remove(toremove);
        delete toremove;
    }

    delete _scheduleOptions;
    delete _outputManager;
    _syncManager = nullptr;

    if (_buffer != nullptr) {
        free(_buffer);
    }

#ifdef __WXMSW__
    ::SetThreadExecutionState(ES_CONTINUOUS);
#endif

    spdlog::info("Closed schedule.");
    spdlog::debug("ScheduleManager destructor complete.");
}

std::list<PlayListItem*> ScheduleManager::GetPlayListIps() const {
    std::list<PlayListItem*> res;

    for (const auto& it : _playLists) {
        for (const auto& it2 : it->GetSteps()) {
            for (const auto& it3 : it2->GetItems()) {
                if (it3->HasIP()) {
                    res.push_back(it3);
                }
            }
        }
    }

    return res;
}

bool ScheduleManager::GetWebRequestToggle() {
    bool rc = _webRequestToggle;
    _webRequestToggle = false;
    return rc;
}

bool ScheduleManager::IsDirty() {
    bool res = _lastSavedChangeCount != _changeCount;

    auto it = _playLists.begin();
    while (!res && it != _playLists.end()) {
        res = res || (*it)->IsDirty();
        ++it;
    }

    res = res || _scheduleOptions->IsDirty();

    for (const auto& it2 : _outputProcessing) {
        res = res || it2->IsDirty();
    }

    return res;
}

void ScheduleManager::SetDirty() {
    _changeCount++;
}

bool ScheduleManager::Save() {
    wxXmlDocument doc;
    wxXmlNode* root = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "xSchedule");
    doc.SetRoot(root);

    root->AddChild(_scheduleOptions->Save());

    for (const auto& it : _playLists) {
        root->AddChild(it->Save());
    }

    if (_outputProcessing.size() != 0) {
        wxXmlNode* op = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "OutputProcesses");

        for (const auto& it : _outputProcessing) {
            op->AddChild(it->Save());
        }

        root->AddChild(op);
    }

    if (_backgroundPlayList != nullptr) {
        wxXmlNode* background = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "Background");
        background->AddAttribute("PlayList", _backgroundPlayList->GetNameNoTime());
        root->AddChild(background);
    }

    // write a temporary file and rename it over the show file, so a failed save cannot damage it
    std::string backupError;
    if (!ShowBackups::Take(_showDir, GetScheduleFile(), ShowBackups::Now(), backupError)) {
        spdlog::warn("Could not back up the show file before saving: {}", backupError);
    }

    const std::string file = _showDir + "/" + GetScheduleFile();
    const std::string temp = file + ".saving";
    if (!doc.Save(temp)) {
        spdlog::error("Unable to write schedule to {}.", temp);
        if (wxFileExists(temp)) wxRemoveFile(temp);
        return false;
    }
    if (!wxRenameFile(temp, file, true)) {
        spdlog::error("Unable to replace {} with the newly saved schedule {}.", file, temp);
        return false;
    }
    ClearDirty();
    spdlog::info("Saved Schedule to {}.", file);
    return true;
}

void ScheduleManager::ClearDirty() {
    _lastSavedChangeCount = _changeCount;

    for (const auto& it : _playLists) {
        it->ClearDirty();
    }

    _scheduleOptions->ClearDirty();

    for (const auto& it : _outputProcessing) {
        it->ClearDirty();
    }
}

void ScheduleManager::RemovePlayList(PlayList* playlist) {
    spdlog::info("Deleting playlist {}.", playlist->GetNameNoTime());
    _playLists.remove(playlist);
    _changeCount++;
}

PlayList* ScheduleManager::GetRunningPlayList(int id) const {
    if (_immediatePlay != nullptr && _immediatePlay->IsRunning() && _immediatePlay->GetId() == (wxUint32)id) {
        return _immediatePlay;
    }
    for (auto it : _activeSchedules) {
        if (it->GetPlayList()->GetId() == (wxUint32)id) {
            return it->GetPlayList();
        }
    }
    return nullptr;
}

PlayList* ScheduleManager::GetRunningPlayList() const {
    // find the highest priority running playlist
    PlayList* running = nullptr;

    if (_immediatePlay != nullptr && _immediatePlay->IsRunning()) {
        running = _immediatePlay;
    } else if ((GetRunningSchedule() == nullptr || GetRunningSchedule()->GetSchedule()->GetPriority() <= Schedule::GetMaxSchedulePriorityBelowQueued()) &&
               _queuedSongs->GetStepCount() > 0 && _queuedSongs->IsRunning()) {
        running = _queuedSongs;
    } else {
        if (GetRunningSchedule() != nullptr) {
            running = GetRunningSchedule()->GetPlayList();
        }
    }

    return running;
}

void ScheduleManager::StopAll(bool sustain) {
    spdlog::info("Stopping all playlists.");

    _syncManager->SendStop();

    _playNext.reset();
    ClearInterruptedPlays();
    if (_immediatePlay != nullptr) {
        _immediatePlay->Stop();
        delete _immediatePlay;
        _immediatePlay = nullptr;
    }

    if (_queuedSongs->IsRunning()) {
        _queuedSongs->Stop();
        _queuedSongs->RemoveAllSteps();
    }

    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        (*it)->Stop();
    }

    while (_eventPlayLists.size() > 0) {
        _eventPlayLists.front()->Stop();
        delete _eventPlayLists.front();
        _eventPlayLists.pop_front();
    }

    if (!sustain) {
        if (!IsSlave() || GetOptions()->IsRemoteAllOff()) {
            AllOff();
        }
    }
}

void ScheduleManager::AllOff() {
    spdlog::debug("Turning all the lights off.");

    memset(_buffer, 0x00, _outputManager->GetTotalChannels()); // clear out any prior frame data
    _outputManager->StartFrame(0);

    if ((_backgroundPlayList != nullptr || _eventPlayLists.size() > 0) && _scheduleOptions->IsSendBackgroundWhenNotRunning()) {
        if (_backgroundPlayList != nullptr) {
            spdlog::debug("   ... except the background lights.");

            if (!_backgroundPlayList->IsRunning()) {
                _backgroundPlayList->Start(true);
                spdlog::debug("Background playlist started. {}.", _backgroundPlayList->GetNameNoTime());
            }
            _backgroundPlayList->Frame(_buffer, _outputManager->GetTotalChannels(), true);
        }

        if (_eventPlayLists.size() > 0) {
            spdlog::debug("   ... except the event lights.");

            auto it = _eventPlayLists.begin();
            while (it != _eventPlayLists.end()) {
                if ((*it)->Frame(_buffer, _outputManager->GetTotalChannels(), true)) {
                    auto temp = it;
                    ++it;
                    (*temp)->Stop();
                    delete *temp;
                    _eventPlayLists.remove(*temp);
                } else {
                    ++it;
                }
            }
        }

        // apply any overlay data
        for (auto it = _overlayData.begin(); it != _overlayData.end(); ++it) {
            (*it)->Set(_buffer, _outputManager->GetTotalChannels());
        }
    }

    // apply any output processing
    for (const auto& it : _outputProcessing) {
        it->Frame(_buffer, _outputManager->GetTotalChannels(), _outputProcessing);
    }

    // if (_brightness < 100)
    //{
    //     ApplyBrightness();
    // }

    for (const auto& it : *GetOptions()->GetVirtualMatrices()) {
        it->Frame(_buffer, _outputManager->GetTotalChannels());
    }

    _outputManager->SetManyChannels(0, _buffer, _outputManager->GetTotalChannels());
    _outputManager->EndFrame();
}

void ScheduleManager::ApplyBrightness() {
    if (_brightness == 100)
        return;

    if (_brightness != _lastBrightness) {
        _lastBrightness = _brightness;
        CreateBrightnessArray();
    }

    auto totalChannels = _outputManager->GetTotalChannels();

    auto ed = OutputProcess::GetExcludeDim(_outputProcessing, 1, totalChannels);

    if (ed.size() == 0) { // handle the simple case fast
        // uint8_t* pb = _buffer;
        // for (size_t i = 0; i < totalChannels; ++i) {
        //     *pb = _brightnessArray[*pb];
        //     pb++;
        // }
        parallel_for(0, totalChannels, [this](int i) {
            _buffer[i] = _brightnessArray[_buffer[i]];
        });
    } else {
        auto exclude = ed.begin();
        uint8_t* pb = _buffer;
        for (size_t i = 0; i < (size_t)totalChannels; ++i) {
            while (exclude != ed.end() && i > (*exclude)->GetLastExcludeChannel() - 1)
                ++exclude;
            bool ex = (exclude != ed.end() && i >= (*exclude)->GetFirstExcludeChannel() - 1);

            if (!ex) {
                *pb = _brightnessArray[*pb];
            }
            pb++;
        }
    }
}

int ScheduleManager::Frame(bool outputframe, xScheduleFrame* frame) {
    static bool reentry = false;
    static int oldrate = 50;

    if (reentry)
        return oldrate;

    reentry = true;

    wxStopWatch sw;

    int rate = 0;
    long totalChannels = _outputManager->GetTotalChannels();

    // timeout xyzzy if no api calls for 15 seconds
    if (_xyzzy != nullptr && (wxDateTime::Now() - _lastXyzzyCommand).GetSeconds() > 15) {
        spdlog::info("Stopping xyzzy due to timeout.");

        wxString msg;
        DoXyzzy("close", "", msg, "");
    }

    if (IsTest()) {
        long msec = OutputTimeMS();

        if (outputframe) {
            memset(_buffer, 0x00, totalChannels); // clear out any prior frame data
            _outputManager->StartFrame(msec);
            TestFrame(_buffer, totalChannels, msec);
        }

        // apply any output processing
        for (const auto& it : _outputProcessing) {
            it->Frame(_buffer, totalChannels, _outputProcessing);
        }

        if (outputframe && _brightness < 100) {
            ApplyBrightness();
        }

        for (const auto& it : *GetOptions()->GetVirtualMatrices()) {
            it->Frame(_buffer, totalChannels);
        }

        if (outputframe) {
            _outputManager->SetManyChannels(0, _buffer, totalChannels);
            _outputManager->EndFrame();
        }
    } else {
        if (!IsSlave() && ReleaseFinishedImmediatePlay()) {
            // let any suspended schedule carry on now rather than at the next schedule check
            wxCommandEvent event(EVT_DOCHECKSCHEDULE);
            wxPostEvent(wxGetApp().GetTopWindow(), event);
            wxCommandEvent event2(EVT_SCHEDULECHANGED);
            wxPostEvent(wxGetApp().GetTopWindow(), event2);
        }
        PlayList* running = GetRunningPlayList();
        static PlayListStep* lastStep = nullptr;
        PlayListStep* step = running != nullptr ? running->GetRunningStep() : nullptr;
        if (step != lastStep) {
            lastStep = step;
            Diagnostics::SetPlaying(step != nullptr ? running->GetNameNoTime() + " / " + step->GetNameNoTime() : std::string());
        }
        if (running != nullptr || _xyzzy != nullptr) {
            rate = 50;
            std::string fseq = "";
            std::string media = "";

            if (running != nullptr) {
                fseq = running->GetActiveSyncItemFSEQ();
                media = running->GetActiveSyncItemMedia();
                rate = running->GetFrameMS();
            }

            long msec = OutputTimeMS();

            if (outputframe) {
                memset(_buffer, 0x00, totalChannels); // clear out any prior frame data
                _outputManager->StartFrame(msec);
            }

            bool done = false;
            if (running != nullptr) {
                spdlog::debug("Frame: About to run step frame {}ms", sw.Time());
                Diagnostics::Activity activity("playing a step");
                done = running->Frame(_buffer, totalChannels, outputframe);
                spdlog::debug("Frame: step frame done {}ms", sw.Time());

                std::string call = running->TakePendingCall();
                if (!call.empty() && !done) {
                    PlayList* p = GetPlayList(call);
                    if (p == nullptr || p->GetId() == running->GetId()) {
                        spdlog::warn("Branch in playlist {} can't play playlist '{}'.", running->GetNameNoTime(), call);
                    } else {
                        // the step after the branch has just started; it starts again once the other playlist ends
                        running->SetRestartStepOnResume(true);
                        running->Suspend(true);
                        _playNext = p->GetId();
                        wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                }

                if (running->GetRunningStep() != nullptr) {
                    size_t fms;
                    std::string tsn = "";
                    auto ts = running->GetRunningStep()->GetTimeSource(fms);
                    if (ts != nullptr) {
                        tsn = running->GetRunningStep()->GetTimeSource(fms)->GetNameNoTime();
                    }

                    Diagnostics::Activity activity("sending sync");
                    _syncManager->SendSync(rate,
                                           running->GetRunningStep()->GetLengthMS(),
                                           running->GetRunningStep()->GetPosition(),
                                           running->GetPosition(),
                                           fseq,
                                           media,
                                           running->GetRunningStep()->GetNameNoTime(),
                                           tsn,
                                           running->GetRunningStepIndex(),
                                           running->GetRunningStep()->GetBaseTimeCodeTime());
                }

                // for queued songs we must remove the queued song when it finishes
                if (running == _queuedSongs) {
                    if (_queuedSongs->GetRunningStep() != _queuedSongs->GetSteps().front()) {
                        PlayListStep* finished = _queuedSongs->GetSteps().front();
                        _queuedSongs->RemoveStep(finished);
                        delete finished;
                        wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                        wxCommandEvent event2(EVT_SCHEDULECHANGED);
                        wxPostEvent(wxGetApp().GetTopWindow(), event2);
                    }
                }
            }

            if (_backgroundPlayList != nullptr) {
                if (!_backgroundPlayList->IsRunning()) {
                    _backgroundPlayList->Start(true);
                    spdlog::debug("Background playlist restarted. {}.", _backgroundPlayList->GetNameNoTime());
                }
                _backgroundPlayList->Frame(_buffer, totalChannels, outputframe);
            }

            if (_eventPlayLists.size() > 0) {
                Diagnostics::Activity activity("playing event playlists");
                auto it = _eventPlayLists.begin();
                while (it != _eventPlayLists.end()) {
                    if ((*it)->Frame(_buffer, _outputManager->GetTotalChannels(), outputframe)) {
                        auto temp = it;
                        ++it;
                        (*temp)->Stop();
                        delete *temp;
                        _eventPlayLists.remove(*temp);
                    } else {
                        ++it;
                    }
                }
            }

            if (_xyzzy != nullptr) {
                _xyzzy->Frame(_buffer, totalChannels, outputframe);
            }

            if (outputframe) {
                // apply any overlay data
                for (auto it = _overlayData.begin(); it != _overlayData.end(); ++it) {
                    (*it)->Set(_buffer, totalChannels);
                }

                frame->ManipulateBuffer(_buffer, totalChannels);

                spdlog::debug("Frame: Overlay data done {}ms", sw.Time());

                // apply any output processing
                for (auto it = _outputProcessing.begin(); it != _outputProcessing.end(); ++it) {
                    (*it)->Frame(_buffer, totalChannels, _outputProcessing);
                }

                spdlog::debug("Frame: Output processing done {}ms", sw.Time());

                if (outputframe && _brightness < 100) {
                    ApplyBrightness();
                }

                spdlog::debug("Frame: Brightness done {}ms", sw.Time());

                for (const auto& it : *GetOptions()->GetVirtualMatrices()) {
                    it->Frame(_buffer, totalChannels);
                }

                spdlog::debug("Frame: Virtual matrices done {}ms", sw.Time());

                _listenerManager->ProcessFrame(_buffer, totalChannels);

                spdlog::debug("Frame: Listening done {}ms", sw.Time());

                _outputManager->SetManyChannels(0, _buffer, totalChannels);

                spdlog::debug("Frame: Data set {}ms", sw.Time());

                Diagnostics::Activity activity("sending output to the controllers");
                _outputManager->EndFrame();

                spdlog::debug("Frame: Data sent {}ms", sw.Time());
            }

            if (done) {
                if (running != nullptr) {
                    _syncManager->SendStop();

                    // playlist is done
                    if (!running->IsSuspended()) {
                        StopPlayList(running, false);
                    }
                }

                wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                wxPostEvent(wxGetApp().GetTopWindow(), event);
                wxCommandEvent event2(EVT_SCHEDULECHANGED);
                wxPostEvent(wxGetApp().GetTopWindow(), event2);
            }
        } else {
            if (_scheduleOptions->IsSendOffWhenNotRunning()) {
                if (outputframe) {
                    memset(_buffer, 0x00, totalChannels); // clear out any prior frame data
                    _outputManager->StartFrame(0);
                    _outputManager->AllOff(false);
                }

                if ((_backgroundPlayList != nullptr || _eventPlayLists.size() > 0) && _scheduleOptions->IsSendBackgroundWhenNotRunning()) {
                    if (_backgroundPlayList != nullptr) {
                        if (!_backgroundPlayList->IsRunning()) {
                            _backgroundPlayList->Start(true);
                            spdlog::debug("Background playlist restarted. {}.", _backgroundPlayList->GetNameNoTime());
                        }
                        _backgroundPlayList->Frame(_buffer, totalChannels, outputframe);
                    }

                    if (_eventPlayLists.size() > 0) {
                        auto it = _eventPlayLists.begin();
                        while (it != _eventPlayLists.end()) {
                            if ((*it)->Frame(_buffer, _outputManager->GetTotalChannels(), true)) {
                                auto temp = it;
                                ++it;
                                (*temp)->Stop();
                                delete *temp;
                                _eventPlayLists.remove(*temp);
                            } else {
                                ++it;
                            }
                        }
                    }

                    // apply any overlay data
                    for (auto it = _overlayData.begin(); it != _overlayData.end(); ++it) {
                        (*it)->Set(_buffer, totalChannels);
                    }

                    frame->ManipulateBuffer(_buffer, totalChannels);
                }

                // apply any output processing
                for (const auto& it : _outputProcessing) {
                    it->Frame(_buffer, totalChannels, _outputProcessing);
                }

                if (outputframe && _brightness < 100) {
                    ApplyBrightness();
                }

                for (const auto& it : *GetOptions()->GetVirtualMatrices()) {
                    it->Frame(_buffer, totalChannels);
                }

                _listenerManager->ProcessFrame(_buffer, totalChannels);

                if (outputframe) {
                    _outputManager->SetManyChannels(0, _buffer, totalChannels);
                    _outputManager->EndFrame();
                }
            } else {
                if (_eventPlayLists.size() > 0) {
                    if (outputframe) {
                        memset(_buffer, 0x00, totalChannels); // clear out any prior frame data
                        _outputManager->StartFrame(0);
                        _outputManager->AllOff(false);
                    }

                    auto it = _eventPlayLists.begin();
                    while (it != _eventPlayLists.end()) {
                        if ((*it)->Frame(_buffer, _outputManager->GetTotalChannels(), true)) {
                            auto temp = it;
                            ++it;
                            (*temp)->Stop();
                            delete *temp;
                            _eventPlayLists.remove(*temp);
                        } else {
                            ++it;
                        }
                    }

                    frame->ManipulateBuffer(_buffer, totalChannels);

                    // apply any output processing
                    for (auto it2 = _outputProcessing.begin(); it2 != _outputProcessing.end(); ++it2) {
                        (*it2)->Frame(_buffer, totalChannels, _outputProcessing);
                    }

                    if (outputframe && _brightness < 100) {
                        ApplyBrightness();
                    }

                    for (auto it2 : *GetOptions()->GetVirtualMatrices()) {
                        it2->Frame(_buffer, totalChannels);
                    }

                    if (outputframe) {
                        _outputManager->SetManyChannels(0, _buffer, totalChannels);
                        _outputManager->EndFrame();
                    }

                    if (_eventPlayLists.size() == 0) {
                        // last event playlist ended ... turn everything off
                        _outputManager->AllOff(true);
                        for (auto& it2 : *GetOptions()->GetVirtualMatrices()) {
                            it2->AllOff();
                        }
                    }
                }
            }
        }
    }

    UpdatePlaybackSnapshot();

    reentry = false;
    if (rate == 0)
        rate = 50;
    oldrate = rate;

    if (_overrideMS != 0) {
        rate = _overrideMS;
    }

    return rate;
}

bool ScheduleManager::IsSlave() const {
    if (_syncManager != nullptr) {
        return _syncManager->IsSlave();
    }
    return false;
}

bool ScheduleManager::IsFPPRemoteOrMaster() const {
    if (_syncManager != nullptr) {
        return _syncManager->IsFPPRemoteOrMaster();
    }
    return false;
}

void ScheduleManager::CreateBrightnessArray() {
    for (size_t i = 0; i < 256; ++i) {
        int b = (i * _brightness) / 100;
        _brightnessArray[i] = (uint8_t)(b & 0xFF);
    }
}

bool ScheduleManager::PlayPlayList(PlayList* playlist, size_t& rate, bool loop, const std::string& step, bool forcelast, int plloops, bool random, int steploops) {
    // the original play commands replace any manual playlist, including ones waiting to resume
    ClearInterruptedPlays();
    return StartManualPlayList(playlist, rate, loop, step, forcelast, plloops, random, steploops);
}

void ScheduleManager::ClearInterruptedPlays() {
    for (auto it : _interruptedPlays) {
        it->Stop();
        delete it;
    }
    _interruptedPlays.clear();
}

bool ScheduleManager::PlayPlayListThenResume(PlayList* playlist, size_t& rate, bool restartInterruptedStep) {
    if (playlist == nullptr) return false;

    if (_immediatePlay != nullptr && _immediatePlay->IsRunning()) {
        spdlog::info("Manual playlist {} interrupted by {}.", _immediatePlay->GetNameNoTime(), playlist->GetNameNoTime());
        if (restartInterruptedStep) _immediatePlay->SetRestartStepOnResume(true);
        if (!_immediatePlay->IsSuspended()) _immediatePlay->Suspend(true);
        _interruptedPlays.push_back(_immediatePlay);
        _immediatePlay = nullptr;
        if (_interruptedPlays.size() > 10) {
            spdlog::info("Too many interrupted playlists, dropping {}.", _interruptedPlays.front()->GetNameNoTime());
            _interruptedPlays.front()->Stop();
            delete _interruptedPlays.front();
            _interruptedPlays.pop_front();
        }
    } else if (restartInterruptedStep) {
        PlayList* running = GetRunningPlayList();
        if (running != nullptr) running->SetRestartStepOnResume(true);
    }
    return StartManualPlayList(playlist, rate);
}

bool ScheduleManager::PlayPlayListNext(PlayList* playlist, size_t& rate) {
    if (playlist == nullptr) return false;

    PlayList* running = GetRunningPlayList();
    if (running == nullptr || running->IsSuspended()) {
        _playNext.reset();
        return PlayPlayListThenResume(playlist, rate, false);
    }
    spdlog::info("Playlist {} will play when {} finishes its current step.", playlist->GetNameNoTime(), running->GetNameNoTime());
    _playNext = playlist->GetId();
    running->SetSuspendAtEndOfCurrentStep();
    return true;
}

// Runs before the schedule check resumes anything, as that would otherwise resume the playlist that just paused for it.
bool ScheduleManager::StartPlayNextIfReady(size_t& rate) {
    if (!_playNext.has_value()) return false;
    PlayList* running = GetRunningPlayList();
    if (running != nullptr && !running->IsSuspended()) return false;

    PlayList* p = GetPlayList(*_playNext);
    _playNext.reset();
    if (p == nullptr) return false;
    spdlog::info("Playing playlist {} next.", p->GetNameNoTime());
    return PlayPlayListThenResume(p, rate, false);
}

bool ScheduleManager::StartManualPlayList(PlayList* playlist, size_t& rate, bool loop, const std::string& step, bool forcelast, int plloops, bool random, int steploops) {
    bool result = true;

    if (playlist == nullptr) {
        spdlog::info("Error PlayPlayList called with null playlist.");
        return false;
    }
    spdlog::info("Playing playlist {}.", playlist->GetNameNoTime());

    if (_immediatePlay != nullptr) {
        _immediatePlay->Stop();
        delete _immediatePlay;
        _immediatePlay = nullptr;
    }

    if (_queuedSongs != nullptr && _queuedSongs->IsRunning()) {
        spdlog::info("Suspending queued playlist so immediate can play.");
        _queuedSongs->Suspend(true);
    }

    // this needs to create a copy of everything ... including steps etc
    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        if (!(*it)->GetPlayList()->IsSuspended()) {
            spdlog::info("Suspending playlist {} due to schedule {} so immediate can play.", (*it)->GetPlayList()->GetNameNoTime(), (*it)->GetSchedule()->GetName());
            (*it)->GetPlayList()->Suspend(true);
        }
    }

    _immediatePlay = new PlayList(*playlist);
    wxASSERT(_immediatePlay != nullptr);
    if (step != "") {
        _immediatePlay->Start(loop, random, plloops, step);
        _immediatePlay->GetRunningStep()->SetLoops(steploops);
    } else {
        _immediatePlay->Start(loop, random, plloops);
    }

    if (forcelast) {
        _immediatePlay->StopAtEndOfCurrentStep();
    }

    // play the zero frame immediately ... this ensures the first frame happens in less than the first frame time
    Frame(true, ((xScheduleApp*)wxTheApp)->GetFrame());

    rate = 25; // always start fast
    return result;
}

// A manual playlist can end without going through StopPlayList (next step on the last step, a jump to a step that
// doesn't exist, a timecode stop, an empty playlist). Left in place, it keeps every schedule suspended.
bool ScheduleManager::ReleaseFinishedImmediatePlay() {
    bool released = false;
    if (_immediatePlay != nullptr && !_immediatePlay->IsRunning()) {
        spdlog::info("Manual playlist {} has ended.", _immediatePlay->GetNameNoTime());
        delete _immediatePlay;
        _immediatePlay = nullptr;
        released = true;
    }
    if (_immediatePlay == nullptr && !_interruptedPlays.empty()) {
        _immediatePlay = _interruptedPlays.back();
        _interruptedPlays.pop_back();
        spdlog::info("Resuming manual playlist {}.", _immediatePlay->GetNameNoTime());
        _immediatePlay->Suspend(false);
        released = true;
    }
    return released;
}

struct ScheduleManager::PlaybackSnapshot {
    PlaybackState state;
    bool paused = false;
    wxLongLong taken = 0;
};

void ScheduleManager::UpdatePlaybackSnapshot() {
    PlaybackSnapshot snapshot;
    PlayList* pl = GetRunningPlayList();
    if (pl != nullptr && !IsTest()) {
        PlayListStep* step = pl->GetRunningStep();
        snapshot.state.playing = true;
        snapshot.state.playlistMS = (uint32_t)pl->GetPosition();
        snapshot.state.stepNo = pl->GetRunningStepIndex();
        snapshot.state.stepMS = step == nullptr ? 0 : (uint32_t)step->GetPosition();
        snapshot.state.baseTimeCodeSecs = step == nullptr ? -1 : step->GetBaseTimeCodeTime();
        snapshot.paused = pl->IsPaused() || pl->IsSuspended();
    }
    snapshot.taken = wxGetUTCTimeMillis();

    std::lock_guard<std::mutex> lock(_snapshotLock);
    if (_snapshot == nullptr) _snapshot = std::make_unique<PlaybackSnapshot>();
    *_snapshot = snapshot;
}

PlaybackState ScheduleManager::GetPlaybackSnapshot() const {
    std::lock_guard<std::mutex> lock(_snapshotLock);
    if (_snapshot == nullptr) return PlaybackState();
    PlaybackState state = _snapshot->state;
    if (state.playing && !_snapshot->paused) {
        uint32_t since = (uint32_t)(wxGetUTCTimeMillis() - _snapshot->taken).GetValue();
        state.playlistMS += since;
        state.stepMS += since;
    }
    return state;
}

// Milliseconds since start, for the outputs' frame timing. long is 32 bits on Windows, so the plain difference went
// negative after about 24.8 days and LOR outputs then sent a heartbeat every frame. It now wraps back to 0 instead,
// which the LOR heartbeat already treats as time moving backwards.
long ScheduleManager::OutputTimeMS() const {
    return (long)((wxUint32)(wxGetUTCTimeMillis().GetLo() - _startTime) & 0x7FFFFFFF);
}

bool compare_runningschedules(const RunningSchedule* first, const RunningSchedule* second) {
    return first->GetSchedule()->GetPriority() > second->GetSchedule()->GetPriority();
}

int ScheduleManager::CheckSchedule() {
    if (_syncManager->IsSlave())
        return 50;

    ReleaseFinishedImmediatePlay();
    size_t playNextRate = 50;
    StartPlayNextIfReady(playNextRate);

    spdlog::debug("Checking the schedule ...");

    bool higherThanQueuedPlayListScheduleFound = false;

    // check all the schedules and add into the list any that should be in the active schedules list
    for (const auto& it : _playLists) {
        auto schedules = it->GetSchedules();
        for (const auto& it2 : schedules) {
            spdlog::debug("   Checking playlist {} schedule {}.", it->GetNameNoTime(), it2->GetName());
            if (it2->CheckActive()) {
                spdlog::debug("   It should be active.");
                bool found = false;

                for (const auto& it3 : _activeSchedules) {
                    if (it3->GetSchedule()->GetId() == it2->GetId()) {
                        if (it3->GetSchedule()->GetPriority() > Schedule::GetMaxSchedulePriorityBelowQueued()) {
                            higherThanQueuedPlayListScheduleFound = true;
                        }
                        found = true;
                        break;
                    }
                }

                // some playlist are only allowed to start in specific minutes
                if (!found && it2->ShouldFire()) {
                    // is hasnt been active before now
                    RunningSchedule* rs = new RunningSchedule(it, it2);
                    if (rs->GetSchedule() != nullptr && rs->GetSchedule()->GetPriority() > Schedule::GetMaxSchedulePriorityBelowQueued()) {
                        higherThanQueuedPlayListScheduleFound = true;
                    }
                    _activeSchedules.push_back(rs);
                    rs->GetPlayList()->StartSuspended(rs->GetSchedule()->GetLoop(), rs->GetSchedule()->GetRandom(), rs->GetSchedule()->GetLoops());

                    if (_scheduleOptions->IsLateStartingScheduleUsesTime()) {
                        long late = rs->GetSchedule()->GetTimeSinceStartTime().GetSeconds().ToLong();
                        if (late > 5) {
                            spdlog::debug("Schedule {} started {} seconds late so jumping ahead.", rs->GetSchedule()->GetName(), late);
                            rs->GetPlayList()->SetPosition(late);
                        }
                    }

                    spdlog::info("   Scheduler starting suspended playlist {} due to schedule {}.", it->GetNameNoTime(), it2->GetName());
                } else {
                    spdlog::debug("   It was already in the list.");
                }
            }
        }
    }

    std::list<RunningSchedule*> todelete;
    for (const auto& it : _activeSchedules) {
        if (!it->GetSchedule()->CheckActive()) {
            if (!it->GetPlayList()->IsRunning()) {
                spdlog::info("   Scheduler removing playlist {} due to schedule {}.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                // this shouldnt be in the list any longer
                todelete.push_back(it);
            } else {
                if (!it->GetPlayList()->IsFinishingUp()) {
                    if (it->GetSchedule()->IsHardStop()) {
                        spdlog::info("   Scheduler telling playlist {} due to schedule {} it is time to hard stop.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                        // This looks weird but i need to do it this way
                        it->Stop();
                        StopPlayList(it->GetPlayList(), false);
                        todelete.push_back(it);
                    } else if (it->GetSchedule()->IsStopAtEndOfLoop()) {
                        // finish the current pass through the playlist (and its end steps) then stop
                        if (it->GetPlayList()->IsStoppingAtEndOfThisLoop()) continue;
                        spdlog::info("   Scheduler telling playlist {} due to schedule {} to stop at the end of this loop.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                        it->GetPlayList()->SetStepLooping(false); // a looping step would never reach the end of the loop
                        it->GetPlayList()->StopAtEndOfThisLoop();
                    } else {
                        spdlog::info("   Scheduler telling playlist {} due to schedule {} it is time to finish up.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                        it->GetPlayList()->JumpToEndStepsAtEndOfCurrentStep();
                    }
                }
            }
        }
    }

    for (const auto& it : todelete) {
        _activeSchedules.remove(it);
        delete it;
    }

    if (todelete.size() > 0 && _activeSchedules.size() == 0) {
        // Last active schedule removed ... lets blank the show
        AllOff();
    }

    int framems = 50;

    _activeSchedules.sort(compare_runningschedules);

    if (_immediatePlay == nullptr) {
        if (higherThanQueuedPlayListScheduleFound && !_queuedSongs->IsSuspended()) {
            _queuedSongs->Suspend(true);
        }
        if (_queuedSongs->GetSteps().size() == 0 || higherThanQueuedPlayListScheduleFound) {
            RunningSchedule* toUnsuspend = nullptr;
            bool first = true;
            for (const auto& it : _activeSchedules) {
                if (first) {
                    if (!it->IsStopped() && (it->GetPlayList()->IsRunning() || (it->GetSchedule()->GetFireFrequency() != "Fire once" && it->GetSchedule()->ShouldFire()))) {
                        first = false;

                        PlayList* actuallyRunningPlaylist = GetRunningPlayList();

                        if (actuallyRunningPlaylist != nullptr && actuallyRunningPlaylist != it->GetPlayList() && it->GetSchedule()->GetGracefullyInterrupt()) {
                            spdlog::info("   Playlist {} being gracefully interupted by schedule {} on Playlist {}.", actuallyRunningPlaylist->GetNameNoTime(), it->GetSchedule()->GetName(), it->GetPlayList()->GetNameNoTime());
                            actuallyRunningPlaylist->SetSuspendAtEndOfCurrentStep();
                            if (!it->GetPlayList()->IsRunning() &&
                                it->GetSchedule()->GetFireFrequency() != "Fire once" &&
                                it->GetSchedule()->ShouldFire()) {
                                it->GetPlayList()->StartSuspended();
                            }
                        } else {
                            if (!it->GetPlayList()->IsRunning() &&
                                it->GetSchedule()->GetFireFrequency() != "Fire once" &&
                                it->GetSchedule()->ShouldFire()) {
                                it->GetPlayList()->StartSuspended();
                                toUnsuspend = it;
                            } else if (it->GetPlayList()->IsSuspended()) {
                                toUnsuspend = it;
                            }
                        }
                    }
                } else {
                    if (!it->GetPlayList()->IsSuspended() && toUnsuspend != nullptr) {
                        spdlog::info("   Suspending playlist {} due to schedule {}.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                        it->GetPlayList()->Suspend(true);
                    }
                }
            }
            if (toUnsuspend != nullptr) {
                spdlog::info("   Unsuspending playlist {} due to schedule {}.", toUnsuspend->GetPlayList()->GetNameNoTime(), toUnsuspend->GetSchedule()->GetName());
                framems = toUnsuspend->GetPlayList()->Suspend(false);
                toUnsuspend->GetSchedule()->DidFire();
            }
        } else {
            // we should be playing our queued song
            // make sure they are all suspended
            for (const auto& it : _activeSchedules) {
                if (!it->GetPlayList()->IsSuspended()) {
                    spdlog::info("   Suspending playlist {} due to schedule {} so immediate can play.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                    it->GetPlayList()->Suspend(true);
                }
            }

            if (!_queuedSongs->IsRunning()) {
                // we need to start it
                _queuedSongs->Start();
            } else if (_queuedSongs->IsSuspended()) {
                // we need to unsuspend it
                _queuedSongs->Suspend(false);
            } else {
                // it is already running
            }

            if (_queuedSongs->GetRunningStep() != nullptr) {
                framems = _queuedSongs->GetRunningStep()->GetFrameMS();
            }
        }
    } else {
        // make sure they are all suspended
        for (const auto& it : _activeSchedules) {
            if (!it->GetPlayList()->IsSuspended()) {
                spdlog::info("   Suspending playlist {} due to schedule {} so immediate can play.", it->GetPlayList()->GetNameNoTime(), it->GetSchedule()->GetName());
                it->GetPlayList()->Suspend(true);
            }
        }

        if (_queuedSongs->GetSteps().size() > 0 && !_queuedSongs->IsSuspended()) {
            spdlog::info("   Suspending queued playlist so immediate can play.");
            _queuedSongs->Suspend(true);
        }

        if (_immediatePlay->GetRunningStep() != nullptr) {
            framems = _immediatePlay->GetRunningStep()->GetFrameMS();
        }
    }

    spdlog::debug("   Active scheduled playlists: {}", _activeSchedules.size());
    for (const auto& it : _activeSchedules) {
        if (it->GetPlayList() != nullptr && it->GetSchedule() != nullptr) {
            PlayListStep* step = it->GetPlayList()->GetRunningStep();
            std::string runstate = it->IsStopped() ? _("Stopped") : it->GetPlayList()->IsRunning() ? _("Running")
                                                                : it->GetPlayList()->IsSuspended() ? _("Suspended")
                                                                                                   : _("Done");
            std::string suspend = it->GetPlayList()->IsSuspended() ? _("Suspended") : _("");
            std::string stepname = step == nullptr ? _("").ToStdString() : step->GetNameNoTime();
            std::string pos = std::string(step == nullptr ? _("") : FORMATTIME(step->GetPosition()));
            std::string len = std::string(step == nullptr ? _("") : FORMATTIME(step->GetLengthMS()));

            spdlog::debug("        Playlist {}, Schedule {} Priority {} {} {} Step '{}' Time {}/{}",
                              it->GetPlayList()->GetName(),
                              it->GetSchedule()->GetName(),
                              it->GetSchedule()->GetPriority(),
                              runstate,
                              suspend,
                              stepname,
                              pos,
                              len);
        } else {
            spdlog::error("        Weird playlist or schedule was null.");
        }
    }

    if (_overrideMS != 0) {
        framems = _overrideMS;
    }

    return framems;
}

std::string ScheduleManager::FormatTime(size_t timems) {
    return wxString::Format(wxT("%i:%02i.%03i"), (long)(timems / 60000), (long)((timems % 60000) / 1000), (long)(timems % 1000)).ToStdString();
}

std::string ScheduleManager::GetStatus() const {
    PlayList* curr = GetRunningPlayList();

    if (!IsSomethingPlaying()) {
        return "Idle";
    }

    return "Playing " + curr->GetRunningStep()->GetNameNoTime() + " " + curr->GetRunningStep()->GetStatus();
}

bool ScheduleManager::IsQuery(const wxString& command) {
    wxString c = command.Lower();
    if (c == "getplaylists" ||
        c == "getplayliststeps" ||
        c == "getmatrices" ||
        c == "getplayingeffects" ||
        c == "getqueuedsteps" ||
        c == "listwebfolders" ||
        c == "getnextscheduledplaylist" ||
        c == "getplaylistschedules" ||
        c == "getplaylistschedule" ||
        c == "getplayingstatus" ||
        c == "getrangesset" ||
        c == "getbuttons" ||
        c == "getcalendar" ||
        c == "getcalendarday" ||
        c == "getnightplan" ||
        c == "getmatrix") {
        return true;
    }
    return false;
}

PlayList* ScheduleManager::GetPlayList(const std::string& playlist) const {
    for (const auto& it : _playLists) {
        if (wxString(it->GetNameNoTime()).Lower() == wxString(playlist).Lower()) {
            return it;
        }
    }

    return nullptr;
}

bool ScheduleManager::IsQueuedPlaylistRunning() const {
    return GetRunningPlayList() != nullptr && _queuedSongs->GetId() == GetRunningPlayList()->GetId();
}

// localhost/xScheduleCommand?Command=<command>&Parameters=<comma separated parameters>
bool ScheduleManager::Action(const wxString& command, const wxString& parameters, const wxString& data, PlayList* selplaylist, PlayListStep* selplayliststep, Schedule* selschedule, size_t& rate, wxString& msg) {

    bool result = true;
    bool scheduleChanged = false;

    Diagnostics::Activity activity("running a command", command.ToStdString());
    Command* cmd = _commandManager.GetCommand(command);

    if (cmd == nullptr) {
        result = false;
        msg = "Unknown command.";
    } else {
        if (!cmd->IsValid(parameters, selplaylist, selplayliststep, selschedule, this, msg, IsQueuedPlaylistRunning())) {
            result = false;
        } else {
            if (_mainThread != wxThread::GetCurrentId()) {
                spdlog::debug("Action '{}':'{}' arrived not on main thread ... switching threads.",
                              command.ToStdString(), parameters.ToStdString());

                // Because of what this function can do calling it on the main thread is dangerous ... so we need to switch threads
                ActionMessageData* amd = new ActionMessageData(command, parameters, data);
                amd->_source = Diagnostics::GetCommandSource();
                wxCommandEvent event(EVT_DOACTION);
                event.SetClientData(amd);
                wxPostEvent(wxGetApp().GetTopWindow(), event);
            } else {
                std::string source = Diagnostics::GetCommandSource();
                spdlog::info("Command '{}' '{}' from {}.", command.ToStdString(), parameters.ToStdString(), source.empty() ? std::string("xSchedule") : source);
                if (command == "Stop all now") {
                    // we cant stop here as this might be in the middle of playing the playlist
                    wxCommandEvent event(EVT_STOP);
                    event.SetInt(-1);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Close xSchedule") {
                    wxCommandEvent event(EVT_QUIT);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                } else if (command == "Stop") {
                    PlayList* p = GetRunningPlayList();
                    if (p != nullptr) {
                        wxCommandEvent event(EVT_STOP);
                        event.SetInt(p->GetId());
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                    scheduleChanged = true;
                } else if (command == "Play selected playlist") {
                    if (selplaylist != nullptr) {
                        if (!PlayPlayList(selplaylist, rate)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play selected playlist step") {
                    if (selplaylist != nullptr) {
                        if (selplayliststep != nullptr) {
                            std::string step = selplayliststep->GetNameNoTime();
                            if (!PlayPlayList(selplaylist, rate, false, step, true)) {
                                result = false;
                                msg = "Unable to start playlist step.";
                            }
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play selected playlist step looped") {
                    if (selplaylist != nullptr) {
                        if (selplayliststep != nullptr) {
                            std::string step = selplayliststep->GetNameNoTime();
                            if (!PlayPlayList(selplaylist, rate, false, step, true)) {
                                result = false;
                                msg = "Unable to start playlist step.";
                            } else {
                                GetRunningPlayList()->SetStepLooping(true);
                                GetRunningPlayList()->ClearStopAtEndOfCurrentStep();
                            }
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Adjust frame interval by ms") {
                    if (_overrideMS != 0) {
                        rate = _overrideMS;
                    } else if (GetRunningPlayList() != nullptr) {
                        rate = GetRunningPlayList()->GetFrameMS();
                    } else {
                        rate = 2 * rate;
                    }
                    rate += wxAtoi(parameters);
                    if (rate != 0) {
                        if (rate < 15)
                            rate = 15;
                        if (rate > 10000)
                            rate = 10000;
                    }
                    if (rate == 0) {
                        _overrideMS = 0;
                        if (GetRunningPlayList() != nullptr) {
                            rate = GetRunningPlayList()->GetFrameMS();
                        }
                        spdlog::debug("Frame rate override cleared. Frame rate now {}ms", rate);
                    } else {
                        _overrideMS = rate;
                        spdlog::debug("Frame rate adjusted by {}ms to {}ms", wxAtoi(parameters), rate);
                    }
                } else if (command == "Set frame interval to ms") {
                    rate = wxAtoi(parameters);
                    if (rate != 0) {
                        if (rate < 15)
                            rate = 15;
                        if (rate > 10000)
                            rate = 10000;
                    }
                    if (rate == 0) {
                        _overrideMS = 0;
                        if (GetRunningPlayList() != nullptr) {
                            rate = GetRunningPlayList()->GetFrameMS();
                        }
                        spdlog::debug("Frame rate override cleared. Frame rate now {}ms", rate);
                    } else {
                        _overrideMS = rate;
                        spdlog::debug("Frame rate set to {}ms", rate);
                    }
                } else if (command == "Play selected playlist looped") {
                    if (selplaylist != nullptr) {
                        if (!PlayPlayList(selplaylist, rate, true)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play specified playlist") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play specified playlist next") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));
                    if (p == nullptr || !PlayPlayListNext(p, rate)) {
                        result = false;
                        msg = "Unable to start playlist.";
                    }
                    scheduleChanged = true;
                } else if (command == "Play specified playlist now then resume" || command == "Play specified playlist now then restart interrupted step") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));
                    if (p == nullptr || !PlayPlayListThenResume(p, rate, command == "Play specified playlist now then restart interrupted step")) {
                        result = false;
                        msg = "Unable to start playlist.";
                    }
                    scheduleChanged = true;
                } else if (command == "Play specified playlist if not running") {
                    PlayList* running = GetRunningPlayList();
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        if (running == nullptr || running->GetId() != p->GetId()) {
                            if (!PlayPlayList(p, rate)) {
                                result = false;
                                msg = "Unable to start playlist.";
                            }
                        } else {
                            result = false;
                            msg = "Playlist not started as it is already runnning.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Change show folder") {
                    if (parameters == "" || wxDir::Exists(parameters)) {
                        wxCommandEvent event(EVT_CHANGESHOWFOLDER);
                        event.SetString(parameters);
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    } else {
                        msg = "Change show folder: Folder does not exist '" + parameters + "'";
                        result = false;
                    }
                } else if (command == "Fire plugin event") {
                    wxArrayString ep = wxSplit(parameters, '|');
                    if (ep.size() > 0) {
                        std::string plugin = ep[0];

                        std::string eps = "";
                        if (ep.size() > 1) {
                            eps = ep[1];
                        }

                        if (plugin == "*") {
                            ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().FireEvent("Command", eps);
                        } else {
                            ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().FirePluginEvent(plugin, "Command", eps);
                        }
                    } else {
                        msg = "No plugin specified";
                        result = false;
                    }
                } else if (command == "Start plugin") {
                    auto plugin = ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().GetPluginFromLabel(parameters);
                    if (plugin == "") {
                        msg = "Plugin not found";
                        result = false;
                    } else {
                        if (!((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().StartPlugin(plugin, _showDir, _scheduleOptions->GetOurURL())) {
                            msg = "Plugin could not be started";
                            result = false;
                        }
                    }
                    ((xScheduleApp*)wxTheApp)->GetFrame()->PluginStateChanged();
                } else if (command == "Stop plugin") {
                    auto plugin = ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().GetPluginFromLabel(parameters);
                    if (plugin == "") {
                        msg = "Plugin not found";
                        result = false;
                    } else {
                        if (!((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().StopPlugin(plugin)) {
                            msg = "Plugin could not be stopped";
                            result = false;
                        }
                    }
                    ((xScheduleApp*)wxTheApp)->GetFrame()->PluginStateChanged();
                } else if (command == "Send command to plugin") {
                    wxArrayString split = wxSplit(parameters, ',');
                    std::string plugin;
                    std::string command;
                    std::string params;
                    if (split.size() > 0)
                        plugin = split[0];
                    if (split.size() > 1)
                        command = split[1];
                    if (split.size() > 2)
                        params = split[2];

                    if (split.size() < 2) {
                        msg = "Plugin command requires at least the plugin name and a command";
                        result = false;
                    } else {
                        auto p = ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().GetPluginFromLabel(plugin);
                        if (p == "") {
                            msg = "Plugin not found";
                            result = false;
                        } else {
                            std::string m;
                            ((xScheduleApp*)wxTheApp)->GetFrame()->GetPluginManager().SendCommand(p, command, params, &result, &m);
                            msg = m;
                        }
                    }
                } else if (command == "Set mode") {
                    int mode = (int)SYNCMODE::STANDALONE;
                    REMOTEMODE remote = REMOTEMODE::DISABLED;
                    bool test = false;
                    wxArrayString modes = wxSplit(parameters, '|');
                    for (const auto& it : modes) {
                        auto m = it.Lower().Trim().Trim(false);
                        if (m == "test") {
                            test = true;
                        } else if (m == "master_fppunicast") {
                            mode |= (int)SYNCMODE::FPPUNICASTMASTER;
                        } else if (m == "master_fppunicastcsv") {
                            mode |= (int)SYNCMODE::FPPUNICASTCSVMASTER;
                        } else if (m == "master_artnet") {
                            mode |= (int)SYNCMODE::ARTNETMASTER;
                        } else if (m == "master_fppbroadcast") {
                            mode |= (int)SYNCMODE::FPPBROADCASTMASTER;
                        } else if (m == "master_midi") {
                            mode |= (int)SYNCMODE::MIDIMASTER;
                        } else if (m == "master_osc") {
                            mode |= (int)SYNCMODE::OSCMASTER;
                        } else if (m == "remote_fppunicast") {
                            remote = REMOTEMODE::FPPSLAVE;
                        } else if (m == "remote_artnet") {
                            remote = REMOTEMODE::ARTNETSLAVE;
                        } else if (m == "remote_fppbroadcast") {
                            remote = REMOTEMODE::FPPSLAVE;
                        } else if (m == "remote_fpp") {
                            remote = REMOTEMODE::FPPSLAVE;
                        } else if (m == "remote_fppcsv") {
                            remote = REMOTEMODE::FPPCSVSLAVE;
                        } else if (m == "remote_midi") {
                            remote = REMOTEMODE::MIDISLAVE;
                        } else if (m == "remote_smpte") {
                            remote = REMOTEMODE::SMPTESLAVE;
                        } else if (m == "remote_osc") {
                            remote = REMOTEMODE::OSCSLAVE;
                        } else if (m == "standalone") {
                            // no need to do anything
                        } else {
                            result = false;
                            msg = "Invalid mode set '" + m + "'";
                        }
                    }
                    SetTestMode(test);
                    SetMode(mode, remote);
                } else if (command == "Play specified playlist if nothing running") {
                    PlayList* running = GetRunningPlayList();
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        if (running == nullptr) {
                            if (!PlayPlayList(p, rate)) {
                                result = false;
                                msg = "Unable to start playlist.";
                            }
                        } else {
                            result = false;
                            msg = "Playlist not started as something is already runnning.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play specified playlist looped") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, true)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Stop specified playlist") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        wxCommandEvent event(EVT_STOP);
                        event.SetInt(p->GetId());
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                    scheduleChanged = true;
                } else if (command == "Stop specified playlist at end of current step") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        wxCommandEvent event(EVT_STOP);
                        event.SetInt(p->GetId());
                        event.SetString("end");
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                    scheduleChanged = true;
                } else if (command == "Stop playlist at end of current step") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        wxCommandEvent event(EVT_STOP);
                        event.SetInt(p->GetId());
                        event.SetString("end");
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                    scheduleChanged = true;
                } else if (command == "Stop specified playlist at end of current loop") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));
                    if (p != nullptr) {
                        p = GetRunningPlayList(p->GetId());
                    }

                    if (p != nullptr) {
                        p->StopAtEndOfThisLoop();
                    }
                    scheduleChanged = true;
                } else if (command == "Jump to play once at end at end of current step and then stop") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->JumpToEndStepsAtEndOfCurrentStep();
                    }
                    scheduleChanged = true;
                } else if (command == "Stop playlist at end of current loop") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->StopAtEndOfThisLoop();
                    }
                    scheduleChanged = true;
                } else if (command == "Pause") {
                    result = ToggleCurrentPlayListPause(msg);
                    scheduleChanged = true;
                } else if (command == "Next step in current playlist") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        if (_queuedSongs != nullptr && p->GetId() == _queuedSongs->GetId()) {
                            _queuedSongs->Stop();
                            PlayListStep* skipped = _queuedSongs->GetSteps().front();
                            _queuedSongs->RemoveStep(skipped);
                            delete skipped;
                            _queuedSongs->Start();
                        } else {
                            p->JumpToNextStep();
                            rate = 25; // always start fast
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Add n Seconds To Current Step Position") {
                    PlayList* p = GetRunningPlayList();
                    int seconds = wxAtoi(parameters);

                    if (p != nullptr) {
                        PlayListStep* pls = p->GetRunningStep();
                        if (pls != nullptr) {
                            pls->Advance(seconds);
                        } else {
                            result = false;
                            msg = "Current playlist has no running step.";
                        }
                    } else {
                        result = false;
                        msg = "No playlist playing.";
                    }
                } else if (command == "Restart step in current playlist") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->RestartCurrentStep();
                    }
                    scheduleChanged = true;
                } else if (command == "Prior step in current playlist") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->JumpToPriorStep();
                        rate = 25; // always start fast                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Toggle loop current step") {
                    result = ToggleCurrentPlayListStepLoop(msg);
                    scheduleChanged = true;
                } else if (command == "Play specified step in specified playlist looped") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, false, step)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        } else {
                            GetRunningPlayList()->SetStepLooping(true);
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Run process") {
                    bool run = false;
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());
                    std::string item = DecodeItem(split[2].ToStdString());

                    if (pl == "" && step == "") {
                        PlayListItem* pli = FindRunProcessNamed(item);
                        if (pli != nullptr) {
                            if (pli->GetTitle() == "Run Process") {
                                pli->Start(pli->GetDurationMS());
                                pli->Frame(nullptr, 0, 50, 50, true);
                                pli->Stop();
                                run = true;
                            }
                        }
                    } else {
                        PlayList* p = GetPlayList(pl);
                        if (p != nullptr) {
                            PlayListStep* pls = p->GetStep(step);

                            if (pls != nullptr) {
                                PlayListItem* pli = pls->GetItem(item);

                                if (pli != nullptr) {
                                    if (pli->GetTitle() == "Run Process") {
                                        pli->Start(pli->GetDurationMS());
                                        pli->Frame(nullptr, 0, 50, 50, true);
                                        pli->Stop();
                                        run = true;
                                    }
                                }
                            }
                        }
                    }
                    if (!run) {
                        result = false;
                        msg = "Unable to find run process.";
                    }
                } else if (command == "Run event playlist step") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                            _eventPlayLists.push_back(new PlayList(*p, step));
                            _eventPlayLists.back()->Start(false, false, 0, step);
                            _eventPlayLists.back()->StopAtEndOfCurrentStep();
                        }
                    }
                } else if (command == "Run event playlist step looped") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            // a loop of this step already running is replaced; another copy would only play on top of it
                            for (auto it2 = _eventPlayLists.begin(); it2 != _eventPlayLists.end();) {
                                if ((*it2)->GetId() == p->GetId() && (*it2)->GetRunningStep() != nullptr && (*it2)->GetRunningStep()->GetNameNoTime() == pls->GetNameNoTime()) {
                                    (*it2)->Stop();
                                    delete *it2;
                                    it2 = _eventPlayLists.erase(it2);
                                } else {
                                    ++it2;
                                }
                            }

                            spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                            _eventPlayLists.push_back(new PlayList(*p, step));
                            _eventPlayLists.back()->Start(false, false, 0, step);
                            _eventPlayLists.back()->LoopStep(step);
                        }
                    }
                } else if (command == "Run event playlist step unique") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            // stop and remove any existing items from the specified playlist
                            auto it2 = _eventPlayLists.begin();
                            while (it2 != _eventPlayLists.end()) {
                                if ((*it2)->GetId() == p->GetId()) {
                                    auto temp = it2;
                                    ++it2;
                                    (*temp)->Stop();
                                    delete *temp;
                                    _eventPlayLists.remove(*temp);
                                } else {
                                    ++it2;
                                }
                            }

                            spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                            _eventPlayLists.push_back(new PlayList(*p, step));
                            _eventPlayLists.back()->Start(false, false, 0, step);
                            _eventPlayLists.back()->StopAtEndOfCurrentStep();
                        }
                    }
                } else if (command == "Stop event playlist if playing step") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            // stop and remove any existing items from the specified playlist
                            auto it2 = _eventPlayLists.begin();
                            while (it2 != _eventPlayLists.end()) {
                                if ((*it2)->GetId() == p->GetId() &&
                                    ((*it2)->GetRunningStep()->GetId() == pls->GetId())) {
                                    auto temp = it2;
                                    ++it2;
                                    (*temp)->Stop();
                                    delete *temp;
                                    _eventPlayLists.remove(*temp);
                                    spdlog::info("Stopped event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());
                                } else {
                                    ++it2;
                                }
                            }
                        }
                    }
                } else if (command == "Set playlist as background") {
                    std::string pl = DecodePlayList(parameters);

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        SetBackgroundPlayList(p);
                        spdlog::info("Set playlist as background {}.", p->GetNameNoTime());
                    }
                } else if (command == "Clear background playlist") {
                    SetBackgroundPlayList(nullptr);
                    spdlog::info("Clear background playlist.");
                } else if (command == "Stop all event playlists") {
                    auto it2 = _eventPlayLists.begin();
                    while (it2 != _eventPlayLists.end()) {
                        spdlog::info("Stopped event playlist {}.", (*it2)->GetNameNoTime());
                        auto temp = it2;
                        ++it2;
                        (*temp)->Stop();
                        delete *temp;
                        _eventPlayLists.remove(*temp);
                    }
                } else if (command == "Stop event playlist") {
                    std::string pl = DecodePlayList(parameters);

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        auto it2 = _eventPlayLists.begin();
                        while (it2 != _eventPlayLists.end()) {
                            if ((*it2)->GetId() == p->GetId()) {
                                auto temp = it2;
                                ++it2;
                                (*temp)->Stop();
                                delete *temp;
                                _eventPlayLists.remove(*temp);
                                spdlog::info("Stopped event playlist {}.", p->GetNameNoTime());
                            } else {
                                ++it2;
                            }
                        }
                    }
                } else if (command == "Run event playlist step unique looped") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            // stop and remove any existing items from the specified playlist
                            auto it2 = _eventPlayLists.begin();
                            while (it2 != _eventPlayLists.end()) {
                                if ((*it2)->GetId() == p->GetId()) {
                                    auto temp = it2;
                                    ++it2;
                                    (*temp)->Stop();
                                    delete *temp;
                                    _eventPlayLists.remove(*temp);
                                } else {
                                    ++it2;
                                }
                            }

                            spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                            _eventPlayLists.push_back(new PlayList(*p, step));
                            _eventPlayLists.back()->Start(false, false, 0, step);
                            _eventPlayLists.back()->LoopStep(step);
                        }
                    }
                } else if (command == "Run event playlist step if idle") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        bool running = false;
                        // stop and remove any existing items from the specified playlist
                        auto it2 = _eventPlayLists.begin();
                        while (it2 != _eventPlayLists.end()) {
                            if ((*it2)->GetId() == p->GetId()) {
                                running = true;
                                break;
                            } else {
                                ++it2;
                            }
                        }

                        if (!running) {
                            PlayListStep* pls = p->GetStep(step);

                            if (pls != nullptr) {
                                spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                                _eventPlayLists.push_back(new PlayList(*p, step));
                                _eventPlayLists.back()->Start(false, false, 0, step);
                                _eventPlayLists.back()->StopAtEndOfCurrentStep();
                            }
                        } else {
                            spdlog::info("Event playlist {} step {} not started because playlist is already playing.", p->GetNameNoTime(), step);
                        }
                    }
                } else if (command == "Run event playlist step if idle looped") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);
                    if (p != nullptr) {
                        bool running = false;
                        // stop and remove any existing items from the specified playlist
                        auto it2 = _eventPlayLists.begin();
                        while (it2 != _eventPlayLists.end()) {
                            if ((*it2)->GetId() == p->GetId()) {
                                running = true;
                                break;
                            } else {
                                ++it2;
                            }
                        }

                        if (!running) {
                            PlayListStep* pls = p->GetStep(step);

                            if (pls != nullptr) {
                                spdlog::info("Playing event playlist {} step {}.", p->GetNameNoTime(), pls->GetNameNoTime());

                                _eventPlayLists.push_back(new PlayList(*p, step));
                                _eventPlayLists.back()->Start(false, false, 0, step);
                                _eventPlayLists.back()->LoopStep(step);
                            }
                        } else {
                            spdlog::info("Event playlist {} step {} not started because playlist is already playing.", p->GetNameNoTime(), step);
                        }
                    }
                } else if (command == "Play specified playlist step once only") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, false, step, true)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Enqueue playlist step") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        PlayListStep* pls = p->GetStep(step);

                        if (pls != nullptr) {
                            if (_queuedSongs->GetSteps().size() == 0 || (_queuedSongs->GetSteps().size() > 0 && _queuedSongs->GetSteps().back()->GetId() != pls->GetId())) {
                                _queuedSongs->AddStep(new PlayListStep(*pls), -1);
                                if (!_queuedSongs->IsRunning()) {
                                    _queuedSongs->StartSuspended();
                                }
                                wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                                wxPostEvent(wxGetApp().GetTopWindow(), event);
                                wxCommandEvent event2(EVT_SCHEDULECHANGED);
                                wxPostEvent(wxGetApp().GetTopWindow(), event2);
                            } else {
                                result = false;
                                msg = "step is already at the end of the list ... I wont add a duplicate.";
                            }
                        } else {
                            result = false;
                            msg = "Unknown step.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Clear playlist queue") {
                    if (_queuedSongs->IsRunning()) {
                        _queuedSongs->Stop();
                    }

                    _queuedSongs->RemoveAllSteps();
                    _syncManager->SendStop();

                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Play playlist starting at step") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, false, step)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play playlist starting at step looped") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, true, step)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Play playlist step") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, false, step, true)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Set step position") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        PlayListStep* s = p->GetRunningStep();
                        if (s != nullptr) {
                            long ms = wxAtoi(parameters) * s->GetLengthMS() / 255;
                            s->SetSyncPosition((size_t)ms, GetOptions()->GetRemoteAcceptableJitter(), true);
                        } else {
                            result = false;
                            msg = "No step running.";
                        }
                    } else {
                        result = false;
                        msg = "No playlist running.";
                    }
                } else if (command == "Set step position ms") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        PlayListStep* s = p->GetRunningStep();
                        if (s != nullptr) {
                            long ms = wxAtoi(parameters);
                            if (ms < 0)
                                ms = 0;
                            if ((size_t)ms > s->GetLengthMS())
                                ms = s->GetLengthMS();
                            s->SetSyncPosition((size_t)ms, GetOptions()->GetRemoteAcceptableJitter(), true);
                        } else {
                            result = false;
                            msg = "No step running.";
                        }
                    } else {
                        result = false;
                        msg = "No playlist running.";
                    }
                } else if (command == "Jump to specified step in current playlist") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->JumpToStep(parameters);
                        rate = 25; // always start fast                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Jump to specified step in current playlist at the end of current step") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        p->JumpToStepAtEndOfCurrentStep(parameters);
                    }
                    scheduleChanged = true;
                } else if (command == "Jump to random step in current playlist") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        auto r = p->GetRandomStep();
                        if (r != nullptr) {
                            p->JumpToStep(r->GetNameNoTime());
                            rate = 25; // always start fast                        }
                            scheduleChanged = true;
                        }
                    }
                } else if (command == "Jump to random step in specified playlist") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        auto r = p->GetRandomStep();
                        if (r != nullptr) {
                            PlayPlayList(p, rate, false, r->GetNameNoTime(), false);
                            scheduleChanged = true;
                        }
                    }
                } else if (command == "Play one random step in specified playlist") {
                    PlayList* p = GetPlayList(DecodePlayList(parameters));

                    if (p != nullptr) {
                        auto r = p->GetRandomStep();
                        if (r != nullptr) {
                            PlayPlayList(p, rate, false, r->GetNameNoTime(), true);
                            scheduleChanged = true;
                        }
                    }
                } else if (command == "Start test mode") {
                    wxArrayString pp = wxSplit(parameters, '|');
                    if (pp.size() > 0) {
                        GetOptions()->GetTestOptions()->SetMode(pp[0]);
                    }
                    if (pp.size() > 1) {
                        long start = -1;
                        long end = -1;
                        wxString data1;
                        wxString msg1;
                        RetrieveData("GetModels", data1, msg1);
                        try {
                            nlohmann::json root = nlohmann::json::parse(data1.ToStdString());
                        
                            if (root.contains("models") && root["models"].is_array()) {
                                auto models = root["models"];
                                for (const auto& m : models) {
                                    if (m.contains("name") && m["name"].get<std::string>() == pp[1]) {
                                        start = wxAtoi(m["startchannel"].get<std::string>()) - 1;
                                        end = start + wxAtoi(m["channels"].get<std::string>()) - 1;
                                        break;
                                    }
                                }
                            }
                            if (start == -1) {
                                spdlog::error("Unable to find model '{}' in models JSON", pp[1].ToStdString());
                            }
                        } catch (nlohmann::json::exception& ex) {
                            spdlog::error("Unable to load models JSON");
                        
                        } catch(std::exception &ex) {
                            spdlog::error("Unable to load models JSON");
                        }
                        GetOptions()->GetTestOptions()->SetBounds(start, end);
                    }
                    if (pp.size() > 2) {
                        int interval = wxAtoi(pp[2]);
                        if (interval > 0) {
                            GetOptions()->GetTestOptions()->SetInterval(interval);
                        }
                    }
                    if (pp.size() > 3) {
                        GetOptions()->GetTestOptions()->SetLevel1(wxAtoi(pp[3]));
                    }
                    if (pp.size() > 4) {
                        GetOptions()->GetTestOptions()->SetLevel2(wxAtoi(pp[4]));
                    }
                    SetTestMode(true);
                } else if (command == "Stop test mode") {
                    GetOptions()->GetTestOptions()->ClearBounds();
                    SetTestMode(false);
                } else if (command == "Add to the current schedule n minutes") {
                    RunningSchedule* rs = GetRunningSchedule();
                    if (rs != nullptr && rs->GetSchedule() != nullptr) {
                        rs->GetSchedule()->AddMinsToEndTime(wxAtoi(parameters));
                        wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                    }
                } else if (command == "Set volume to") {
                    int volume = wxAtoi(parameters);
                    SetVolume(volume);
                } else if (command == "Adjust volume by") {
                    int volume = wxAtoi(parameters);
                    AdjustVolumeBy(volume);
                } else if (command == "Toggle output to lights") {
                    result = ToggleOutputToLights(nullptr, msg, false);
                } else if (command == "Toggle mute") {
                    ToggleMute();
                } else if (command == "Increase brightness by n%") {
                    int by = wxAtoi(parameters);
                    AdjustBrightness(by);
                } else if (command == "Set brightness to n%") {
                    int b = wxAtoi(parameters);
                    SetBrightness(b);
                } else if (command == "Toggle current playlist random") {
                    result = ToggleCurrentPlayListRandom(msg);
                    scheduleChanged = true;
                } else if (command == "Toggle current playlist loop") {
                    result = ToggleCurrentPlayListLoop(msg);
                    scheduleChanged = true;
                } else if (command == "Save schedule") {
                    if (!Save()) {
                        result = false;
                        msg = "Unable to save the schedule.";
                    }
                    scheduleChanged = true;
                } else if (command == "Run command at end of current step") {
                    PlayList* p = GetRunningPlayList();

                    if (p != nullptr) {
                        wxArrayString parms = wxSplit(parameters, ',');

                        if (parms.Count() > 0) {
                            std::string newparms = "";
                            for (size_t i = 1; i < parms.Count(); ++i) {
                                if (newparms != "")
                                    newparms += ",";
                                newparms += parms[i].ToStdString();
                            }

                            p->SetCommandAtEndOfCurrentStep(parms[0].ToStdString(), newparms);
                        }
                    }
                } else if (command == "Restart selected schedule") {
                    if (selschedule != nullptr) {
                        for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
                            if ((*it)->GetSchedule()->GetId() == selschedule->GetId()) {
                                auto todelete = *it;
                                _activeSchedules.erase(it);
                                todelete->GetPlayList()->Stop();
                                delete todelete;
                                wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                                wxPostEvent(wxGetApp().GetTopWindow(), event);
                                wxCommandEvent event2(EVT_SCHEDULECHANGED);
                                wxPostEvent(wxGetApp().GetTopWindow(), event2);
                                break;
                            }
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Restart all schedules") {
                    for (auto it : _activeSchedules) {
                        delete it;
                    }
                    _activeSchedules.clear();
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Restart named schedule") {
                    auto rs = GetRunningSchedule(DecodeSchedule(parameters));
                    if (rs != nullptr) {
                        rs->Reset();
                        wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                        wxPostEvent(wxGetApp().GetTopWindow(), event);
                        wxCommandEvent event2(EVT_SCHEDULECHANGED);
                        wxPostEvent(wxGetApp().GetTopWindow(), event2);
                    }
                    scheduleChanged = true;
                } else if (command == "Deactivate all schedules") {
                    for (const auto& it : _playLists) {
                        auto schedules = it->GetSchedules();
                        for (const auto& it2 : schedules) {
                            it2->SetEnabled(false);
                        }
                    }
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Activate all schedules") {
                    for (const auto& it : _playLists) {
                        auto schedules = it->GetSchedules();
                        for (const auto& it2 : schedules) {
                            it2->SetEnabled(true);
                        }
                    }
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Activate specified schedule") {
                    for (const auto& it : _playLists) {
                        auto schedules = it->GetSchedules();
                        for (const auto& it2 : schedules) {
                            if (it2->GetName() == parameters) {
                                it2->SetEnabled(true);
                            }
                        }
                    }
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Deactivate specified schedule") {
                    for (const auto& it : _playLists) {
                        auto schedules = it->GetSchedules();
                        for (const auto& it2 : schedules) {
                            if (it2->GetName() == parameters) {
                                it2->SetEnabled(false);
                            }
                        }
                    }
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "Restart playlist schedules") {
                    auto plname = DecodePlayList(parameters);
                    auto pl = GetPlayList(plname);
                    if (pl != nullptr) {
                        for (const auto& it : _activeSchedules) {
                            if (it->GetPlayList()->GetId() == pl->GetId()) {
                                if (!it->GetPlayList()->IsRunning()) {
                                    // need to restart this one
                                    it->Reset();
                                }
                            }
                        }
                    }
                    wxCommandEvent event(EVT_DOCHECKSCHEDULE);
                    wxPostEvent(wxGetApp().GetTopWindow(), event);
                    scheduleChanged = true;
                } else if (command == "PressButton") {
                    UserButton* b = _scheduleOptions->GetButton(DecodeButton(parameters));

                    if (b != nullptr) {
                        std::string c = b->GetCommand();
                        std::string p = b->GetParameters();

                        if (c != "") {
                            result = Action(c, p, "", selplaylist, selplayliststep, selschedule, rate, msg);
                        }
                    } else {
                        result = false;
                        msg = "Unknown button.";
                    }
                } else if (command == "Play specified playlist n times") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    int loops = wxAtoi(split[1]);
                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, loops > 1, "", false, loops)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else if (command == "Refresh current playlist") {
                    if (IsCurrentPlayListScheduled()) {
                        auto rs = GetRunningSchedule();
                        auto plid = rs->GetPlayList()->GetId();
                        auto sid = rs->GetSchedule()->GetId();

                        bool loop = rs->GetPlayList()->IsLooping();
                        std::string step = "";
                        if (rs->GetPlayList()->GetRunningStep() != nullptr) {
                            step = rs->GetPlayList()->GetRunningStep()->GetNameNoTime();
                        }
                        int loopsLeft = rs->GetPlayList()->GetLoopsLeft();
                        bool random = rs->GetPlayList()->IsRandom();

                        rs->GetPlayList()->Stop();
                        _syncManager->SendStop();
                        _activeSchedules.remove(rs);
                        delete rs;

                        PlayList* orig = nullptr;
                        PlayList* pl = nullptr;
                        Schedule* sc = nullptr;

                        for (const auto& it : _playLists) {
                            if (it->GetId() == plid) {
                                orig = it;
                                pl = new PlayList(*it);
                                break;
                            }
                        }

                        if (pl != nullptr) {
                            auto schs = orig->GetSchedules();
                            for (const auto& it : schs) {
                                if (it->GetId() == sid) {
                                    sc = new Schedule(*it);
                                    break;
                                }
                            }

                            if (sc != nullptr) {
                                rs = new RunningSchedule(pl, sc);
                                _activeSchedules.push_back(rs);
                                rs->GetPlayList()->StartSuspended(loop, random, loopsLeft, step);

                                _activeSchedules.sort(compare_runningschedules);
                            }
                        }

                        CheckSchedule();
                    } else if (_immediatePlay != nullptr) {
                        PlayList* p = GetRunningPlayList();
                        if (p != nullptr) {
                            bool loop = p->IsLooping();
                            bool forcelast = p->IsFinishingUp();
                            int loopsLeft = p->GetLoopsLeft();
                            bool random = p->IsRandom();

                            std::string step = p->GetRunningStep()->GetNameNoTime();
                            int steploopsleft = p->GetRunningStep()->GetLoopsLeft();

                            p->Stop();
                            _syncManager->SendStop();

                            auto plid = p->GetId();

                            for (const auto& it : _playLists) {
                                if (it->GetId() == plid) {
                                    PlayPlayList(it, rate, loop, step, forcelast, loopsLeft, random, steploopsleft);
                                    break;
                                }
                            }
                        }
                    } else {
                        result = false;
                        msg = "Only scheduled and immediately played playlists can be restarted.";
                    }
                    scheduleChanged = true;
                } else if (command == "Bring to foreground") {
                    ((wxFrame*)wxGetApp().GetTopWindow())->Iconize(false);
                    wxGetApp().GetTopWindow()->Raise();
                } else if (command == "Set current text") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string textname = DecodeItem(split[0].ToStdString());
                    PlayListItemText* pliText = nullptr;
                    PlayList* p = GetRunningPlayList();
                    if (p != nullptr) {
                        pliText = p->GetRunningText(textname);
                    }
                    if (pliText == nullptr && _backgroundPlayList != nullptr) {
                        pliText = _backgroundPlayList->GetRunningText(textname);
                    }

                    if (pliText != nullptr) {
                        wxString text = "";
                        if (split.size() > 1) {
                            text = split[1];
                        }

                        wxString properties = "";
                        if (split.size() > 2) {
                            properties = split[2];
                        }

                        DoText(pliText, text, properties);
                    } else {
                        result = false;
                        msg = "Text not found to set ... it may not be running.";
                    }
                } else if (command == "Set pixels") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    size_t sc = wxAtol(split[0]);

                    APPLYMETHOD blendMode = APPLYMETHOD::METHOD_OVERWRITE;
                    if (split.size() > 1) {
                        blendMode = EncodeBlendMode(split[1].ToStdString());
                    }

                    PixelData* p = nullptr;
                    for (const auto& it : _overlayData) {
                        if (it->GetStartChannel() == sc) {
                            p = it;
                            if (data.length() == 0) {
                                spdlog::debug("Pixel overlay data removed.");
                                _overlayData.remove(p);
                                delete p;
                            } else {
                                spdlog::debug("Pixel overlay data changed.");
                                p->SetData(data, blendMode);
                            }
                            break;
                        }
                    }

                    if (p == nullptr && data.length() != 0) {
                        spdlog::debug("Pixel overlay data removed.");
                        p = new PixelData(sc, data, blendMode);
                        _overlayData.push_back(p);
                    }
                } else if (command == "Set pixel range") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    if (split.size() < 4) {
                        result = false;
                        msg = "Set pixel range did not get all the parameters '" + parameter + "'";
                    } else {
                        size_t sc = wxAtol(split[0]);
                        size_t ch = wxAtol(split[1]);
                        wxColor c(split[2]);
                        APPLYMETHOD blendMode = APPLYMETHOD::METHOD_OVERWRITE;
                        blendMode = EncodeBlendMode(split[3].ToStdString());

                        bool changed = false;
                        for (auto it = _overlayData.begin(); it != _overlayData.end(); ++it) {
                            if ((*it)->GetStartChannel() == sc) {
                                if (ch != 0 && (*it)->GetSize() == ch) {
                                    spdlog::debug("Pixel overlay data changed.");
                                    (*it)->SetColor(c, blendMode);
                                    changed = true;
                                } else {
                                    // removed, or replaced below by one of the new size
                                    spdlog::debug("Pixel overlay data removed.");
                                    delete *it;
                                    _overlayData.erase(it);
                                }
                                break;
                            }
                        }

                        if (!changed && ch != 0) {
                            spdlog::debug("Pixel overlay data added.");
                            _overlayData.push_back(new PixelData(sc, ch, c, blendMode));
                        }
                    }
                } else if (command == "Clear all overlays") {
                    std::list<PixelData*>::iterator i = _overlayData.begin();
                    while (i != _overlayData.end()) {
                        spdlog::debug("Pixel overlay data removed.");
                        delete *i;
                        i = _overlayData.erase(i);
                    }
                } else if (command == "Play specified playlist step n times") {
                    wxString parameter = parameters;
                    wxArrayString split = wxSplit(parameter, ',');

                    std::string pl = DecodePlayList(split[0].ToStdString());
                    std::string step = DecodeStep(split[1].ToStdString());
                    int loops = wxAtoi(split[2]);

                    PlayList* p = GetPlayList(pl);

                    if (p != nullptr) {
                        if (!PlayPlayList(p, rate, false, step, false, -1, false, loops)) {
                            result = false;
                            msg = "Unable to start playlist.";
                        }
                    }
                    scheduleChanged = true;
                } else {
                    result = false;
                    msg = "Unrecognised command. Check command case.";
                }
            }
        }
    }

    if (!result) {
        spdlog::error("Action failed: {}", msg.ToStdString());

        wxCommandEvent event(EVT_STATUSMSG);
        event.SetString(msg);
        wxPostEvent(wxGetApp().GetTopWindow(), event);
    }

    // Clean up immediate play if one of the actions led to it stopping, and let a suspended schedule carry on now
    // rather than at the next schedule check
    if (_mainThread == wxThread::GetCurrentId() && ReleaseFinishedImmediatePlay()) {
        wxCommandEvent event(EVT_DOCHECKSCHEDULE);
        wxPostEvent(wxGetApp().GetTopWindow(), event);
        scheduleChanged = true;
    }

    if (scheduleChanged) {
        wxCommandEvent event(EVT_SCHEDULECHANGED);
        wxPostEvent(wxGetApp().GetTopWindow(), event);
    }

    if (_overrideMS != 0) {
        rate = _overrideMS;
    }

    return result;
}

bool ScheduleManager::Action(const wxString& label, PlayList* selplaylist, PlayListStep* selplayliststep, Schedule* selschedule, size_t& rate, wxString& msg) {
    UserButton* b = _scheduleOptions->GetButton(label);

    if (b != nullptr) {
        std::string command = b->GetCommand();
        std::string parameters = b->GetParameters();

        return Action(command, parameters, "", selplaylist, selplayliststep, selschedule, rate, msg);
    } else {
        msg = "Unknown button.";
        if (_overrideMS != 0) {
            rate = _overrideMS;
        }
        return false;
    }
}

void ScheduleManager::StopPlayList(PlayList* playlist, bool atendofcurrentstep, bool sustain, bool stopSchedules) {
    if (!atendofcurrentstep) {
        for (auto it = _interruptedPlays.begin(); it != _interruptedPlays.end();) {
            if ((*it)->GetId() == playlist->GetId()) {
                (*it)->Stop();
                delete *it;
                it = _interruptedPlays.erase(it);
            } else {
                ++it;
            }
        }
    }
    if (_immediatePlay != nullptr && _immediatePlay->GetId() == playlist->GetId()) {
        if (atendofcurrentstep) {
            _immediatePlay->StopAtEndOfCurrentStep();
        } else {
            _syncManager->SendStop();
            _immediatePlay->Stop();
            delete _immediatePlay;
            _immediatePlay = nullptr;
        }
    }

    for (const auto& it : _playLists) {
        if (it->GetId() == playlist->GetId() && it->IsRunning()) {
            if (atendofcurrentstep) {
                it->StopAtEndOfCurrentStep();
            } else {
                _syncManager->SendStop();
                it->Stop();
            }
        }
    }

    // a schedule plays its own copy of the playlist, which the loops above never match
    if (stopSchedules) {
        for (const auto& it : _activeSchedules) {
            if (it->GetPlayList()->GetId() == playlist->GetId() && it->GetPlayList()->IsRunning()) {
                if (atendofcurrentstep) {
                    it->GetPlayList()->StopAtEndOfCurrentStep();
                } else {
                    _syncManager->SendStop();
                    it->Stop();
                }
            }
        }
    }

    if (!sustain) {
        if (!IsSlave() || GetOptions()->IsRemoteAllOff()) {
            AllOff();
        }
    }
}

void ScheduleManager::GetNextScheduledPlayList(PlayList** p, Schedule** s) {
    size_t signature = _playLists.size();
    for (const auto& pit : _playLists) {
        signature = signature * 31 + pit->GetId();
        for (const auto& sit : pit->GetSchedules()) {
            signature = signature * 31 + sit->GetId() * 7 + sit->GetChangeCount() + (sit->GetEnabled() ? 1 : 0);
        }
    }
    const wxDateTime zero((time_t)0);
    bool passed = _nextAt.IsValid() && _nextAt != zero && _nextAt <= wxDateTime::Now();
    if (signature == _nextSignature && time(nullptr) - _nextWorkedOut < 30 && !passed) {
        *p = _nextPlayList;
        *s = _nextSchedule;
        return;
    }

    *p = nullptr;
    *s = nullptr;
    wxDateTime next = wxDateTime((time_t)0);
    for (const auto& pit : _playLists) {
        auto schedules = pit->GetSchedules();
        for (const auto& sit : schedules) {
            wxDateTime n = sit->GetNextTriggerDateTime();
            if (n != wxDateTime((time_t)0)) {
                if (next == wxDateTime((time_t)0)) {
                    *p = pit;
                    *s = sit;
                    next = n;
                } else if (n < next) {
                    *p = pit;
                    *s = sit;
                    next = n;
                }
            }
        }
    }

    _nextPlayList = *p;
    _nextSchedule = *s;
    _nextAt = next;
    _nextWorkedOut = time(nullptr);
    _nextSignature = signature;
}

// 127.0.0.1/xScheduleStash?Command=Store&Key=<key> ... this must be posted with the data in the body of the request ... key must be filename legal
// 127.0.0.1/xScheduleStash?Command=Retrieve&Key=<key> ... this returs a text response with the data if successful

// 127.0.0.1/xScheduleQuery?Query=GetPlayLists&Parameters=
// 127.0.0.1/xScheduleQuery?Query=GetPlayListSteps&Parameters=<playlistname>
// 127.0.0.1/xScheduleQuery?Query=GetPlayingStatus&Parameters=
// 127.0.0.1/xScheduleQuery?Query=GetButtons&Parameters=

bool ScheduleManager::Query(const wxString& command, const wxString& parameters, wxString& data, wxString& msg, const wxString& ip, const wxString& referenceIn) {
    const std::string reference = JSONSafe(referenceIn.ToStdString());
    wxASSERT(IsQuery(command));

    bool result = true;
    data = "";
    std::string c = command.Lower();
    if (c == "getplaylists") {
        bool first = true;
        data = "{\"playlists\":[";
        for (const auto& it : _playLists) {
            if (first) {
                first = false;
            } else {
                data += ",";
            }
            data += "{\"name\":\"" + JSONSafe(it->GetNameNoTime()) +
                    "\",\"id\":\"" + wxString::Format(wxT("%i"), it->GetId()).ToStdString() +
                    "\",\"nextscheduled\":\"" + it->GetNextScheduledTime() +
                    "\",\"length\":\"" + FormatTime(it->GetLengthMS()) +
                    "\",\"lengthms\":\"" + wxString::Format("%ld", (long)it->GetLengthMS()) + "\"}";
        }
        data += "],\"reference\":\"" + reference + "\"}";
    } else if (c == "getplayingeffects") {
        bool first = true;
        data = "{\"playingeffects\":[";
        for (const auto& it : _eventPlayLists) {
            if (first) {
                first = false;
            } else {
                data += ",";
            }
            auto running = it->GetRunningStep();
            if (running != nullptr) {
                data += "{\"name\":\"" + JSONSafe(running->GetNameNoTime()) + "\"}";
            }
        }
        data += "],\"reference\":\"" + reference + "\"}";
    } else if (c == "getplayliststeps") {
        PlayList* p = GetPlayList(DecodePlayList(parameters));

        if (p != nullptr) {
            data = "{\"steps\":[";
            auto steps = p->GetSteps();
            for (auto it = steps.begin(); it != steps.end(); ++it) {
                if (it != steps.begin()) {
                    data += ",";
                }

                std::string first;
                if (*it == steps.front() && p->GetFirstOnce()) {
                    first = "\",\"startonly\":\"true";
                } else {
                    first = "\",\"startonly\":\"false";
                }

                std::string last;
                if (*it == steps.back() && p->GetLastOnce()) {
                    last = "\",\"endonly\":\"true";
                } else {
                    last = "\",\"endonly\":\"false";
                }

                data += "{\"name\":\"" + JSONSafe((*it)->GetNameNoTime()) +
                        "\",\"id\":\"" + wxString::Format(wxT("%i"), (*it)->GetId()).ToStdString() +
                        first + last +
                        "\",\"everystep\":\"" + ((*it)->GetEveryStep() ? _("true") : _("false")) +
                        "\",\"offset\":\"" + (*it)->GetStartTime(p) +
                        "\",\"length\":\"" + FormatTime((*it)->GetLengthMS()) +
                        "\",\"lengthms\":\"" + wxString::Format("%ld", (long)(*it)->GetLengthMS()) + "\"}";
            }
            data += "],\"reference\":\"" + reference + "\"}";
        } else {
            data = "{\"steps\":[],\"reference\":\"" + reference + "\"}";
            result = false;
            msg = "Playlist '" + parameters + "' not found.";
        }
    } else if (c == "getmatrices") {
        data = "{\"matrices\":[";
        auto ms = _scheduleOptions->GetMatrices();
        for (auto it = ms->begin(); it != ms->end(); ++it) {
            if (it != ms->begin()) {
                data += ",";
            }
            data += "\"" + JSONSafe((*it)->GetName()) + "\"";
        }
        data += "],\"reference\":\"" + reference + "\"}";
    } else if (c == "getmatrix") {
        auto ms = _scheduleOptions->GetMatrices();
        for (auto it : *ms) {
            if (wxString(it->GetName()).Lower() == wxString(parameters).Lower()) {
                data = "{\"name\":\"" + JSONSafe(it->GetName()) +
                       "\",\"width\":\"" + wxString::Format(wxT("%i"), it->GetWidth()) +
                       "\",\"height\":\"" + wxString::Format(wxT("%i"), it->GetHeight()) +
                       "\",\"startchannel\":\"" + it->GetStartChannel() +
                       "\",\"channels\":\"" + wxString::Format(wxT("%zu"), it->GetChannels()) +
                       "\",\"startlocation\":\"" + it->GetStartLocation() +
                       "\",\"orientation\":\"" + it->GetOrientation() +
                       "\",\"strings\":\"" + wxString::Format(wxT("%i"), it->GetStrings()) +
                       "\",\"stringlength\":\"" + wxString::Format(wxT("%i"), it->GetStringLength()) +
                       "\",\"strandsperstring\":\"" + wxString::Format(wxT("%i"), it->GetStrandsPerString()) +
                       "\",\"reference\":\"" + reference + "\"}";
            }
        }
        if (data.IsEmpty()) // not found
        {
            data = "{\"name\":\"\",\"width\":\"\",\"height\":\"\",\"startchannel\":\"\",\"channels\":\"\",\"startlocation\":\"\",\"orientation\":\"\",\"strings\":\"\",\"stringlength\":\"\",\"strandsperstring\":\"\",\"reference\":\"" + reference + "\"}";
        }
    } else if (c == "getrangesset") {
        bool first = true;
        data = "{\"overlaychannels\":[";
        for (const auto& it : _overlayData) {
            if (!first) {
                data += ",";
            }
            data += "{\"startchannel\":\"" + wxString::Format("%ld", (long)it->GetStartChannel()) +
                    "\",\"channels\":\"" + wxString::Format(wxT("%ld"), (long)it->GetSize()) +
                    "\"}";
            first = false;
        }
        data += "],\"reference\":\"" + reference + "\"}";
    } else if (c == "getqueuedsteps") {
        PlayList* p = _queuedSongs;

        data = "{\"steps\":[";
        auto steps = p->GetSteps();
        for (auto it = steps.begin(); it != steps.end(); ++it) {
            if (it != steps.begin()) {
                data += ",";
            }
            data += "{\"name\":\"" + JSONSafe((*it)->GetNameNoTime()) +
                    "\",\"id\":\"" + wxString::Format(wxT("%i"), (*it)->GetId()).ToStdString() +
                    "\",\"length\":\"" + FormatTime((*it)->GetLengthMS()) +
                    "\",\"lengthms\":\"" + wxString::Format("%ld", (long)((*it)->GetLengthMS())) + "\"}";
        }
        data += "],\"reference\":\"" + reference + "\"}";
    } else if (c == "listwebfolders") {
        if (wxString(parameters).Contains("..")) {
            result = false;
            msg = "Illegal request.";
        } else {
            // parameters holds the subdirectory to scan ... blank is the web directory

#ifdef __WXMSW__
            wxString d = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
#elif __LINUX__
            wxString d = wxStandardPaths::Get().GetDataDir();
            if (!wxDir::Exists(d)) {
                d = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
            }
#else
            wxString d = wxStandardPaths::Get().GetResourcesDir();
#endif
            d += "/" + _scheduleOptions->GetWWWRoot();

            if (parameters != "") {
                d += "/" + parameters;
            }

            if (!wxDir::Exists(d)) {
                result = false;
                msg = "Unknown folder.";
            } else {
                wxDir dir(d);

                data = "{\"folders\":[";
                bool first = true;
                wxString dirname;
                bool found = dir.GetFirst(&dirname, "", wxDIR_DIRS);

                while (found) {
                    if (!first) {
                        data += ",";
                    }
                    first = false;
                    data += "\"" + JSONSafe(dirname.ToStdString()) + "\"";

                    found = dir.GetNext(&dirname);
                }
                data += "],\"reference\":\"" + reference + "\"}";
            }
        }
    } else if (c == "getnextscheduledplaylist") {
        PlayList* p = nullptr;
        Schedule* s = nullptr;
        GetNextScheduledPlayList(&p, &s);

        if (p == nullptr) {
            data = "{\"playlistname\":\"\",\"playlistid\":\"\",\"schedulename\":\"\",\"scheduleid\":\"\",\"start\":\"Never\",\"end\":\"\",\"reference\":\"" + reference + "\"}";
        } else {
            data = "{\"playlistname\":\"" + JSONSafe(p->GetNameNoTime()) + "\"," +
                   "\"playlistid\":\"" + wxString::Format("%i", p->GetId()).ToStdString() + "\"," +
                   "\"schedulename\":\"" + JSONSafe(s->GetName()) + "\"," +
                   "\"scheduleid\":\"" + wxString::Format("%i", s->GetId()).ToStdString() + "\"," +
                   "\"start\":\"" + s->GetNextTriggerTime() + "\"," +
                   "\"end\":\"" + s->GetEndTimeAsString() + "\"," +
                   "\"reference\":\"" + reference + "\"" +
                   "}";
        }
    } else if (c == "getplaylistschedules") {
        PlayList* p = GetPlayList(DecodePlayList(parameters));
        if (p != nullptr) {
            data = "{\"schedules\":[";
            auto scheds = p->GetSchedules();
            for (auto it = scheds.begin(); it != scheds.end(); ++it) {
                if (it != scheds.begin()) {
                    data += ",";
                }
                data += (*it)->GetJSON("");
            }
            data += "],\"reference\":\"" + reference + "\"}";
        } else {
            data = "{\"schedules\":[],\"reference\":\"" + reference + "\"}";
            result = false;
            msg = "Playlist '" + parameters + "' not found.";
        }
    } else if (c == "getplaylistschedule") {
        wxArrayString plsc = wxSplit(parameters, ',');

        if (plsc.Count() == 2) {
            PlayList* p = GetPlayList(DecodePlayList(plsc[0].ToStdString()));
            if (p != nullptr) {
                Schedule* schedule = p->GetSchedule(DecodeSchedule(plsc[1].ToStdString()));

                if (schedule != nullptr) {
                    data = schedule->GetJSON(referenceIn.ToStdString());
                } else {
                    result = false;
                    msg = "Playlist '" + plsc[0] + "' does not have a schedule '" + plsc[1] + "'.";
                }
            } else {
                result = false;
                msg = "Playlist '" + plsc[0] + "' not found.";
            }
        } else {
            result = false;
            msg = "Incorrect parameters. Playlist and schedule expected: " + parameters;
        }
    } else if (c == "getplayingstatus") {
        PlayList* p = GetRunningPlayList();
        if (p == nullptr || p->GetRunningStep() == nullptr) {
            data = "{\"status\":\"idle\",\"outputtolights\":\"" + std::string(_outputManager->IsOutputting() ? "true" : "false") +
                   "\",\"volume\":\"" + wxString::Format(wxT("%i"), GetVolume()) +
                   "\",\"brightness\":\"" + wxString::Format(wxT("%i"), GetBrightness()) +
                   "\",\"ip\":\"" + ip +
                   "\",\"version\":\"" + xschedule_version_string +
                   "\",\"reference\":\"" + reference +
                   "\",\"passwordset\":\"" + (_scheduleOptions->GetPassword() == "" ? "false" : "true") +
                   "\",\"time\":\"" + wxDateTime::Now().Format("%Y-%m-%d %H:%M:%S") +
                   "\"," + GetPingStatus() + "}";
        } else {
            std::string nextsong;
            std::string nextsongid;

            if (p->IsRandom()) {
                nextsong = "God knows";
                nextsongid = "";
            } else {
                auto next = p->PeekNextStep();
                if (next == nullptr) {
                    nextsong = "";
                    nextsongid = "";
                } else {
                    nextsong = next->GetNameNoTime();
                    nextsongid = wxString::Format(wxT("%i"), next->GetId());
                }
            }

            RunningSchedule* rs = GetRunningSchedule();

            data = "{\"status\":\"" + std::string(p->IsPaused() ? "paused" : "playing") +
                   "\",\"playlist\":\"" + JSONSafe(p->GetNameNoTime()) +
                   "\",\"playlistid\":\"" + wxString::Format(wxT("%i"), p->GetId()).ToStdString() +
                   "\",\"playlistlooping\":\"" + (p->IsLooping() || p->GetLoopsLeft() > 0 ? "true" : "false") +
                   "\",\"playlistloopsleft\":\"" + wxString::Format(wxT("%i"), p->GetLoopsLeft()).ToStdString() +
                   "\",\"random\":\"" + (p->IsRandom() ? "true" : "false") +
                   "\",\"step\":\"" + JSONSafe(p->GetRunningStep()->GetNameNoTime()) +
                   "\",\"stepid\":\"" + wxString::Format(wxT("%i"), p->GetRunningStep()->GetId()).ToStdString() +
                   "\",\"steplooping\":\"" + (p->IsStepLooping() || p->GetRunningStep()->GetLoopsLeft() > 0 ? "true" : "false") +
                   "\",\"steploopsleft\":\"" + wxString::Format(wxT("%i"), p->GetRunningStep()->GetLoopsLeft()).ToStdString() +
                   "\",\"length\":\"" + FormatTime(p->GetRunningStep()->GetLengthMS()) +
                   "\",\"lengthms\":\"" + wxString::Format("%ld", (long)(p->GetRunningStep()->GetLengthMS())) +
                   "\",\"position\":\"" + FormatTime(p->GetRunningStep()->GetPosition()) +
                   "\",\"positionms\":\"" + wxString::Format("%ld", (long)(p->GetRunningStep()->GetPosition())) +
                   "\",\"left\":\"" + FormatTime(p->GetRunningStep()->GetLengthMS() - p->GetRunningStep()->GetPosition()) +
                   "\",\"leftms\":\"" + wxString::Format("%ld", (long)(p->GetRunningStep()->GetLengthMS() - p->GetRunningStep()->GetPosition())) +
                   "\",\"playlistposition\":\"" + FormatTime(p->GetPosition()) +
                   "\",\"playlistpositionms\":\"" + wxString::Format("%ld", (long)(p->GetPosition())) +
                   "\",\"playlistleft\":\"" + FormatTime(p->GetLengthMS() - p->GetPosition()) +
                   "\",\"playlistleftms\":\"" + wxString::Format("%ld", (long)(p->GetLengthMS() - p->GetPosition())) +
                   "\",\"trigger\":\"" + std::string(IsCurrentPlayListScheduled() ? "scheduled" : (_immediatePlay != nullptr) ? "manual"
                                                                                                                              : "queued") +
                   "\",\"schedulename\":\"" + std::string((IsCurrentPlayListScheduled() && rs != nullptr) ? JSONSafe(rs->GetSchedule()->GetName()) : "N/A") +
                   "\",\"scheduleend\":\"" + std::string((IsCurrentPlayListScheduled() && rs != nullptr) ? rs->GetSchedule()->GetNextEndTime() : "N/A") +
                   "\",\"scheduleid\":\"" + std::string((IsCurrentPlayListScheduled() && rs != nullptr) ? wxString::Format(wxT("%i"), rs->GetSchedule()->GetId()).ToStdString() : "N/A") +
                   "\",\"nextstep\":\"" + JSONSafe(nextsong) +
                   "\",\"nextstepid\":\"" + nextsongid +
                   "\",\"version\":\"" + xschedule_version_string +
                   "\",\"queuelength\":\"" + wxString::Format(wxT("%ld"), (long)_queuedSongs->GetSteps().size()) +
                   "\",\"volume\":\"" + wxString::Format(wxT("%i"), GetVolume()) +
                   "\",\"brightness\":\"" + wxString::Format(wxT("%i"), GetBrightness()) +
                   "\",\"time\":\"" + wxDateTime::Now().Format("%Y-%m-%d %H:%M:%S") +
                   "\",\"ip\":\"" + ip +
                   "\",\"reference\":\"" + reference +
                   "\",\"autooutputtolights\":\"" + (_manualOTL == -1 ? "true" : "false") +
                   "\",\"passwordset\":\"" + (_scheduleOptions->GetPassword() == "" ? "false" : "true") +
                   "\",\"outputtolights\":\"" + std::string(_outputManager->IsOutputting() ? "true" : "false") +
                   "\"," + GetPingStatus() + "}";

            // spdlog::info("{}", data);
        }
    } else if (c == "getbuttons") {
        data = _scheduleOptions->GetButtonsJSON(_commandManager, referenceIn.ToStdString());
    } else if (c == "getcalendar" || c == "getcalendarday") {
        data = CalendarJSON(c == "getcalendarday", parameters.ToStdString(), reference, msg);
        result = !data.empty();
    } else if (c == "getnightplan") {
        data = NightPlanJSON(parameters.ToStdString(), reference, msg);
        result = !data.empty();
    } else {
        result = false;
        msg = "Unknown query.";
    }

    return result;
}

// GetCalendar: "from,to[,inactive]", dates as YYYY-MM-DD. GetCalendarDay: "date[,inactive]". Times are minutes from
// the night's midnight, so 1470 is 12:30 AM the next morning.
std::string ScheduleManager::CalendarJSON(bool day, const std::string& parameters, const std::string& reference, wxString& msg) {
    auto parts = wxSplit(parameters, ',');
    auto date = [&parts](size_t i, wxDateTime& d) {
        return i < parts.size() && d.ParseISODate(parts[i].Trim().Trim(false));
    };
    bool inactive = parts.size() > (day ? 1u : 2u) && parts.back().Trim().Trim(false).Lower() == "inactive";
    wxDateTime from;
    wxDateTime to;
    if (!date(0, from) || (!day && !date(1, to))) {
        msg = day ? "Expected a date, YYYY-MM-DD." : "Expected two dates, YYYY-MM-DD,YYYY-MM-DD.";
        return "";
    }
    if (!day && (to < from || (to - from).GetDays() > 400)) {
        msg = "The range must run forward, and cover at most 400 days.";
        return "";
    }

    auto id = [](const ScheduleCalendar::Window& w) { return wxString::Format("%u", (unsigned)w.schedule->GetId()).ToStdString(); };
    auto windowJSON = [&id](const ScheduleCalendar::Window& w) {
        return "{\"id\":\"" + id(w) + "\",\"night\":\"" + w.night.FormatISODate().ToStdString() +
               "\",\"start\":" + std::to_string(w.start) + ",\"end\":" + std::to_string(w.end) + ",\"playEnd\":" + std::to_string(w.playEnd) +
               ",\"skipped\":" + (w.skipped ? "true" : "false") + ",\"enabled\":" + (w.enabled ? "true" : "false") + "}";
    };
    auto clashesJSON = [&id](const std::vector<ScheduleCalendar::Clash>& clashes, const std::vector<ScheduleCalendar::Window>& windows) {
        std::string res = "[";
        for (const auto& c : clashes) {
            if (res.size() > 1) res += ",";
            res += "{\"start\":" + std::to_string(c.start) + ",\"end\":" + std::to_string(c.end) +
                   ",\"a\":\"" + id(windows[c.a]) + "\",\"b\":\"" + id(windows[c.b]) + "\"}";
        }
        return res + "]";
    };
    auto nightJSON = [](const ScheduleCalendar::Night& n) {
        return "\"date\":\"" + n.date.FormatISODate().ToStdString() + "\",\"holiday\":\"" + JSONSafe(n.holiday) +
               "\",\"sunset\":" + std::to_string(n.sunset) + ",\"dusk\":" + std::to_string(n.dusk);
    };

    std::string res = "{\"schedules\":[";
    int index = 0;
    for (const auto& pl : _playLists) {
        for (const auto& s : pl->GetSchedules()) {
            if (index > 0) res += ",";
            res += "{\"id\":\"" + wxString::Format("%u", (unsigned)s->GetId()).ToStdString() + "\",\"name\":\"" + JSONSafe(s->GetName()) +
                   "\",\"playlist\":\"" + JSONSafe(pl->GetNameNoTime()) + "\",\"priority\":" + std::to_string(s->GetPriority()) +
                   ",\"enabled\":" + (s->GetEnabled() ? "true" : "false") + ",\"index\":" + std::to_string(index) + "}";
            ++index;
        }
    }
    res += "],";

    if (day) {
        auto d = ScheduleCalendar::DetailOf(*this, from, inactive);
        res += nightJSON(d.night) + ",\"axisStart\":" + std::to_string(d.axisStart) + ",\"axisEnd\":" + std::to_string(d.axisEnd) + ",\"windows\":[";
        for (size_t i = 0; i < d.windows.size(); ++i) {
            if (i > 0) res += ",";
            res += windowJSON(d.windows[i]);
        }
        res += "],\"lanes\":[";
        for (size_t i = 0; i < d.lanes.size(); ++i) {
            if (i > 0) res += ",";
            res += "{\"window\":" + std::to_string(d.lanes[i].window) + ",\"segments\":[";
            for (size_t j = 0; j < d.lanes[i].segments.size(); ++j) {
                const auto& g = d.lanes[i].segments[j];
                if (j > 0) res += ",";
                const char* state = g.state == ScheduleCalendar::State::Plays ? "plays" : g.state == ScheduleCalendar::State::Hidden ? "hidden" : g.state == ScheduleCalendar::State::Clash ? "clash" : "done";
                res += "{\"start\":" + std::to_string(g.start) + ",\"end\":" + std::to_string(g.end) + ",\"state\":\"" + state + "\",\"by\":" + std::to_string(g.by) + "}";
            }
            res += "]}";
        }
        res += "],\"onTheLights\":[";
        for (size_t i = 0; i < d.onTheLights.size(); ++i) {
            const auto& g = d.onTheLights[i];
            if (i > 0) res += ",";
            res += "{\"start\":" + std::to_string(g.start) + ",\"end\":" + std::to_string(g.end) + ",\"window\":" + std::to_string(g.by) + "}";
        }
        res += "],\"clashes\":" + clashesJSON(d.clashes, d.windows);
    } else {
        res += "\"days\":[";
        bool first = true;
        for (const auto& n : ScheduleCalendar::Nights(*this, from, to, inactive)) {
            if (!first) res += ",";
            first = false;
            res += "{" + nightJSON(n) + ",\"windows\":[";
            for (size_t i = 0; i < n.windows.size(); ++i) {
                if (i > 0) res += ",";
                res += windowJSON(n.windows[i]);
            }
            res += "],\"clashes\":" + clashesJSON(ScheduleCalendar::ClashesOf(n.windows), n.windows) + "}";
        }
        res += "]";
    }
    return res + ",\"reference\":\"" + reference + "\"}";
}

// GetNightPlan: "date[,inactive]", the date as YYYY-MM-DD. What each schedule will play that night, step by step, with
// times in ms from the night's midnight.
std::string ScheduleManager::NightPlanJSON(const std::string& parameters, const std::string& reference, wxString& msg) {
    auto parts = wxSplit(parameters, ',');
    wxDateTime date;
    if (parts.empty() || !date.ParseISODate(parts[0].Trim().Trim(false))) {
        msg = "Expected a date, YYYY-MM-DD.";
        return "";
    }
    bool inactive = parts.size() > 1 && parts.back().Trim().Trim(false).Lower() == "inactive";
    auto d = ScheduleCalendar::DetailOf(*this, date, inactive);
    auto p = ScheduleCalendar::PreviewOf(*this, d);
    auto window = [&](int w) {
        if (w < 0) return std::string("\"schedule\":\"\",\"id\":\"\"");
        const auto& dw = d.windows[p.detailWindow[w]];
        return "\"schedule\":\"" + JSONSafe(dw.schedule->GetName()) + "\",\"id\":\"" + wxString::Format("%u", (unsigned)dw.schedule->GetId()).ToStdString() + "\"";
    };
    static const char* endings[] = { "finished", "suspended", "cut", "unfinished" };
    static const char* flags[] = { "none", "late", "wait", "cut" };
    static const char* kinds[] = { "runslate", "startslate", "cutoff", "goesdark" };
    std::string res = "{\"date\":\"" + date.FormatISODate().ToStdString() + "\",\"plays\":[";
    for (size_t i = 0; i < p.plan.plays.size(); ++i) {
        const auto& pl = p.plan.plays[i];
        if (i > 0) res += ",";
        res += "{\"start\":" + std::to_string(pl.start) + ",\"end\":" + std::to_string(pl.end) + "," + window(pl.window) +
               ",\"playlist\":\"" + JSONSafe(p.playlists[pl.playlist]) + "\",\"step\":\"" + JSONSafe(pl.step) + "\",\"pass\":" + std::to_string(pl.pass) +
               ",\"offset\":" + std::to_string(pl.offset) + ",\"called\":" + (pl.called ? "true" : "false") + ",\"estimated\":" + (pl.estimated ? "true" : "false") +
               ",\"ending\":\"" + endings[(int)pl.ending] + "\",\"flag\":\"" + flags[(int)pl.flag] + "\",\"note\":\"" + JSONSafe(pl.note) + "\"}";
    }
    res += "],\"findings\":[";
    for (size_t i = 0; i < p.plan.findings.size(); ++i) {
        const auto& f = p.plan.findings[i];
        if (i > 0) res += ",";
        res += "{\"kind\":\"" + std::string(kinds[(int)f.kind]) + "\"," + window(f.window) + ",\"at\":" + std::to_string(f.at) + ",\"amount\":" + std::to_string(f.amount) +
               ",\"upTo\":" + (f.upTo ? "true" : "false") + ",\"play\":" + std::to_string(f.play) + ",\"text\":\"" + JSONSafe(SchedulePreview::Describe(f, p.windows)) + "\"}";
    }
    res += "],\"truncated\":" + std::string(p.plan.truncated ? "true" : "false") + ",\"background\":\"" + JSONSafe(p.background) + "\"";
    return res + ",\"reference\":\"" + reference + "\"}";
}

static std::atomic<bool> __startingOutputToLights = false;

bool ScheduleManager::IsStartingOutputToLights() {
    return __startingOutputToLights;
}

// xSchedule runs unattended: an output that will not open is skipped and logged, and the others keep
// going. The xLights core only asks "continue?" when interactive; otherwise it closes every output.
bool ScheduleManager::StartOutputToLights() {
    OutputManager::SetConfirmCallback([](const std::string& message, const std::string& title) {
        spdlog::warn("{} Continuing with the other outputs.", message);
        return true;
    });

    struct Starting {
        Starting() { __startingOutputToLights = true; OutputManager::SetInteractive(true); }
        ~Starting() { OutputManager::SetInteractive(false); __startingOutputToLights = false; }
    } starting;
    return _outputManager->StartOutput();
}

void ScheduleManager::DisableRemoteOutputs() {
    // The only way to undo this disable is to restart xSchedule
    // That is not ideal but solving it would require adding amybe a session disable in output
    // and at this time of year i am not inclined to introduce that change - fix this in 2019

    if (_syncManager->IsMaster(SYNCMODE::FPPBROADCASTMASTER) || _syncManager->IsMaster(SYNCMODE::FPPUNICASTMASTER) || _syncManager->IsMaster(SYNCMODE::FPPUNICASTCSVMASTER) || _syncManager->IsMaster(SYNCMODE::FPPMULTICASTMASTER)) {
        std::list<std::string> remotes = GetOptions()->GetFPPRemotes();

        for (auto it = remotes.begin(); it != remotes.end(); ++it) {
            auto outputs = _outputManager->GetOutputs();
            for (auto ito = outputs.begin(); ito != outputs.end(); ++ito) {
                if ((*ito)->IsIpOutput() && (*it == (*ito)->GetIP() || *it == (*ito)->GetResolvedIP())) {
                    spdlog::debug("Output '{}' disabled as this is a listed remote to receive FPP Sync packets.", (*ito)->GetLongDescription());
                    spdlog::debug("    The only way to re-enable these outputs is to shut down xSchedule and restart it.");
                    (*ito)->Enable(false);
                }
            }
        }
    }
}

std::string ScheduleManager::GetPingStatus() {
    std::string res = "\"pingstatus\":[";

    if (_pinger != nullptr) {
        auto ps = _pinger->GetPingers();

        bool first = true;
        for (auto it : ps) {
            if (first) {
                first = false;
            } else {
                res += ",";
            }

            res += "{\"controller\":\"" + it->GetIP() + " " + JSONSafe(it->GetName()) +
                   "\",\"ip\":\"" + it->GetIP() +
                   "\",\"result\":\"" + APinger::GetPingResultName(it->GetPingResult()) +
                   "\",\"failcount\":\"" + wxString::Format("%i", it->GetFailCount()) + "\"}";
        }
    }

    res += "]";
    return res;
}

bool ScheduleManager::StoreData(const wxString& key, const wxString& data, wxString& msg) const {
    bool result = true;

    if (!wxDir::Exists(_showDir + "/xScheduleData")) {
        wxDir sd(_showDir);
        sd.Make(_showDir + "/xScheduleData");
    }

    std::string filename = _showDir + "/xScheduleData/" + key + ".dat";

    wxFile dataFile(filename, wxFile::write);

    if (dataFile.IsOpened()) {
        dataFile.Write(data.c_str(), data.size());
    } else {
        result = false;
        msg = "Unable to create file " + filename;
    }

    return result;
}

bool ScheduleManager::RetrieveData(const wxString& key, wxString& data, wxString& msg) const {
    bool result = true;

    std::string filename = _showDir + "/xScheduleData/" + key + ".dat";

    if (!wxFile::Exists(filename)) {
        data = "";
        result = false;
    } else {
        wxFile dataFile(filename);

        wxString d = "";
        dataFile.ReadAll(&d);
        data = d;
    }

    return result;
}

bool ScheduleManager::ToggleCurrentPlayListRandom(wxString& msg) {
    PlayList* p = GetRunningPlayList();

    if (p != nullptr) {
        return p->SetRandom(!p->IsRandom());
    } else {
        msg = "No playlist currently playing.";
        return false;
    }
}

bool ScheduleManager::ToggleCurrentPlayListPause(wxString& msg) {
    PlayList* p = GetRunningPlayList();
    if (p != nullptr) {
        p->TogglePause();
    } else {
        msg = "No playlist currently playing.";
        return false;
    }

    return true;
}

bool ScheduleManager::ToggleCurrentPlayListLoop(wxString& msg) {
    PlayList* p = GetRunningPlayList();

    if (p != nullptr) {
        return p->SetLooping(!p->IsLooping());
    } else {
        msg = "No playlist currently playing.";
        return false;
    }
}

bool ScheduleManager::ToggleCurrentPlayListStepLoop(wxString& msg) {
    PlayList* p = GetRunningPlayList();

    if (p != nullptr) {
        if (p->IsStepLooping()) {
            p->SetStepLooping(false);
        } else {
            p->SetStepLooping(true);
        }
        return true;
    } else {
        msg = "No playlist currently playing.";
        return false;
    }
}

bool ScheduleManager::IsOutputToLights() const {
    return _outputManager != nullptr && _outputManager->IsOutputting();
}

int ScheduleManager::GetNonStoppedCount() const {
    int count = 0;
    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        if (!(*it)->IsStopped()) {
            count++;
        }
    }
    return count;
}

RunningSchedule* ScheduleManager::GetRunningSchedule() const {
    if (_immediatePlay != nullptr)
        return nullptr;

    bool highPrioritySchedule = false;
    for (const auto& it : _activeSchedules) {
        if (it->GetPlayList()->IsRunning() && !it->GetPlayList()->IsSuspended() && it->GetSchedule()->GetPriority() > Schedule::GetMaxSchedulePriorityBelowQueued()) {
            highPrioritySchedule = true;
            break;
        }
    }

    if (!highPrioritySchedule && _queuedSongs->IsRunning())
        return nullptr;
    if (_activeSchedules.size() == 0)
        return nullptr;

    for (const auto& it : _activeSchedules) {
        if (it->GetPlayList()->IsRunning() && !it->GetPlayList()->IsSuspended()) {
            return it;
        }
    }

    return nullptr;
}

bool ScheduleManager::IsScheduleActive(Schedule* schedule) {
    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        if ((*it)->GetSchedule()->GetId() == schedule->GetId())
            return true;
    }

    return false;
}

RunningSchedule* ScheduleManager::GetRunningSchedule(Schedule* schedule) const {
    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        if ((*it)->GetSchedule()->GetId() == schedule->GetId())
            return *it;
    }

    return nullptr;
}

RunningSchedule* ScheduleManager::GetRunningSchedule(const std::string& schedulename) const {
    for (auto it = _activeSchedules.begin(); it != _activeSchedules.end(); ++it) {
        if (wxString((*it)->GetSchedule()->GetName()).Lower() == wxString(schedulename).Lower())
            return *it;
    }

    return nullptr;
}

void ScheduleManager::SetOutputToLights(xScheduleFrame* frame, bool otl, bool interactive) {
    // prevent this function being reentered
    static bool reenter = false;
    if (reenter)
        return;
    reenter = true;

    wxStopWatch sw;

    if (_outputManager != nullptr) {
        if (otl) {
            if (!IsOutputToLights()) {
                if (ScheduleConfig::GetBool("OutputActive", false) && interactive) {
                    wxMessageBox("Warning: Lights output is already open in another process. This will cause issues.", "WARNING", 4 | wxCENTRE, frame);
                }
                DisableRemoteOutputs();
                bool success = StartOutputToLights();
                if (success) ScheduleConfig::SetBool("OutputActive", true);
#ifdef __WXMSW__
                ::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif
                StartVirtualMatrices();
                ManageBackground();
                spdlog::debug("Turned on output to lights {}ms", sw.Time());
                GetListenerManager()->ProcessPacket("State", "Lights On");
                if (!success) {
                    GetListenerManager()->ProcessPacket("State", "Output Open Error");
                }
            }
        } else {
            if (IsOutputToLights()) {
                _outputManager->StopOutput();
                ScheduleConfig::SetBool("OutputActive", false);
#ifdef __WXMSW__
                ::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
                StopVirtualMatrices();
                ManageBackground();
                spdlog::debug("Turned off output to lights {}ms", sw.Time());
                GetListenerManager()->ProcessPacket("State", "Lights Off");
            }
        }
    }

    reenter = false;
}

void ScheduleManager::ManualOutputToLightsClick(xScheduleFrame* frame) {
    _manualOTL++;
    if (_manualOTL > 1)
        _manualOTL = -1;
    if (_manualOTL == 1) {
        if (ScheduleConfig::GetBool("OutputActive", false)) {
            wxMessageBox("Warning: Lights output is already open in another process. This will cause issues.", "WARNING", 4 | wxCENTRE, frame);
        }
        DisableRemoteOutputs();
        if (StartOutputToLights()) ScheduleConfig::SetBool("OutputActive", true);
#ifdef __WXMSW__
        ::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif
        StartVirtualMatrices();
        ManageBackground();
        GetListenerManager()->ProcessPacket("State", "Lights On");
    } else if (_manualOTL == 0) {
        _outputManager->StopOutput();
        ScheduleConfig::SetBool("OutputActive", false);
#ifdef __WXMSW__
        ::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif
        StopVirtualMatrices();
        ManageBackground();
        GetListenerManager()->ProcessPacket("State", "Lights Off");
    }
}

bool ScheduleManager::ToggleOutputToLights(xScheduleFrame* frame, wxString& msg, bool interactive) {
    if (_outputManager->IsOutputting()) {
        _manualOTL = 0;
        SetOutputToLights(frame, false, interactive);
    } else {
        _manualOTL = 1;
        SetOutputToLights(frame, true, interactive);
    }

    return true;
}

void ScheduleManager::SuppressVM(bool suppress) {
    auto v = GetOptions()->GetVirtualMatrices();

    for (auto it = v->begin(); it != v->end(); ++it) {
        (*it)->Suppress(suppress);
    }
}

void ScheduleManager::SetVolume(int volume) {
    int cv = volume;
    if (cv < 0)
        cv = 0;
    if (cv > 100)
        cv = 100;
    AudioManager::SetGlobalVolume(cv);
}

void ScheduleManager::AdjustVolumeBy(int volume) {
    int cv = GetVolume();
    cv += volume;
    SetVolume(cv);
}

int ScheduleManager::GetVolume() const {
    return AudioManager::GetGlobalVolume();
}

size_t ScheduleManager::GetTotalChannels() const {
    if (_outputManager != nullptr)
        return _outputManager->GetTotalChannels();

    return 0;
}

void ScheduleManager::ToggleMute() {
    static int savevolume = 100;
    if (GetVolume() > 0) {
        savevolume = GetVolume();
        SetVolume(0);
    } else {
        SetVolume(savevolume);
    }
}

void ScheduleManager::ToggleBrightness() {
    static int savebrightness = 100;
    if (GetBrightness() > 0) {
        savebrightness = GetBrightness();
        SetBrightness(0);
    } else {
        SetBrightness(savebrightness);
    }
}

void ScheduleManager::SetMode(int mode, REMOTEMODE remote) {
    _mode = mode;
    _remoteMode = remote;

    auto* config = GetXLightsConfig();
    config->Write("SyncMode", (long)_mode);
    config->Write("RemoteMode", (long)_remoteMode);
    config->Flush();

    _syncManager->Start(mode, remote, GetForceLocalIP());
}

PlayList* ScheduleManager::GetPlayList(int id) const {
    for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
        if ((*it)->GetId() == (wxUint32)id)
            return *it;
    }

    return nullptr;
}

void ScheduleManager::SetBackgroundPlayList(PlayList* playlist) {

    if (playlist == nullptr && _backgroundPlayList != nullptr) {
        spdlog::debug("Background playlist stopped and deleted. {}.", _backgroundPlayList->GetNameNoTime());
        _backgroundPlayList->Stop();
        delete _backgroundPlayList;
        _backgroundPlayList = nullptr;
        _changeCount++;
    } else if (playlist != nullptr) {
        if (_backgroundPlayList == nullptr) {
            _backgroundPlayList = new PlayList(*playlist);
            spdlog::debug("Background playlist loaded. {}.", _backgroundPlayList->GetNameNoTime());
            _changeCount++;
        } else {
            if (playlist->GetId() != _backgroundPlayList->GetId()) {
                spdlog::debug("Background playlist stopped and deleted. {}.", _backgroundPlayList->GetNameNoTime());
                _backgroundPlayList->Stop();
                delete _backgroundPlayList;
                _backgroundPlayList = new PlayList(*playlist);
                spdlog::debug("Background playlist loaded. {}.", _backgroundPlayList->GetNameNoTime());
                _changeCount++;
            }
        }
    }
}

void ScheduleManager::ManageBackground() {
    if (_backgroundPlayList != nullptr) {
        if (IsOutputToLights()) {
            spdlog::debug("Background playlist stopped and deleted. {}.", _backgroundPlayList->GetNameNoTime());
            _backgroundPlayList->Stop();
            int id = _backgroundPlayList->GetId();
            delete _backgroundPlayList;
            _backgroundPlayList = nullptr;
            PlayList* pl = GetPlayList(id);
            if (pl != nullptr) {
                _backgroundPlayList = new PlayList(*pl);
                _backgroundPlayList->Start(true);
                spdlog::debug("Background playlist loaded and started. {}.", _backgroundPlayList->GetNameNoTime());
            }
        } else {
            spdlog::debug("Background playlist stopped. {}.", _backgroundPlayList->GetNameNoTime());
            _backgroundPlayList->Stop();
            AllOff();
        }
    }
}

void LogAndWrite(wxFile& f, const std::string& msg) {

    spdlog::debug("CheckSchedule: " + msg);
    if (f.IsOpened()) {
        f.Write(msg + "\r\n");
    }
}

std::string ScheduleManager::GetOurIP() const {
    std::string ip = "UNKNOWN IP";

    wxDatagramSocket* testSocket;
    wxIPV4address addr;
    if (GetForceLocalIP() != "") {
        addr.Hostname(GetForceLocalIP());
        testSocket = new wxDatagramSocket(addr, wxSOCKET_NOWAIT);
    } else {
        addr.AnyAddress();
        testSocket = new wxDatagramSocket(addr, wxSOCKET_NOWAIT);
        addr.Hostname(wxGetFullHostName());
        if (addr.IPAddress() == "255.255.255.255") {
            addr.Hostname(wxGetHostName());
        }
    }

    ip = addr.IPAddress().ToStdString();

    if (testSocket == nullptr || !testSocket->IsOk() || testSocket->Error()) {
        ip = "UNKNOWN IP";
    }

    if (testSocket != nullptr) {
        delete testSocket;
    }

    return ip;
}

bool ScheduleManager::ShowDirectoriesMatch() const {
    std::string xlsd = xLightsShowDir();
    std::string xssd = xScheduleShowDir();

    if (xlsd != "" && xssd != "" && !ScheduleConfig::IsSameFolder(xlsd, xssd)) {
        return false;
    }

    return true;
}

void ScheduleManager::CheckScheduleIntegrity(bool display) {

    int errcount = 0;
    int warncount = 0;
    int errcountsave = 0;
    int warncountsave = 0;

    wxFile f;
    wxString filename = wxFileName::CreateTempFileName("xLightsCheckSchedule") + ".txt";

    if (display) {
        f.Open(filename, wxFile::write);
        if (!f.IsOpened()) {
            spdlog::warn("Unable to create results file for Check Schedule. Aborted.");
            wxMessageBox(_("Unable to create results file for Check Schedule. Aborted."), _("Error"));
            return;
        }
    }

    LogAndWrite(f, "Checking schedule.");
    wxDatagramSocket* testSocket;
    wxIPV4address addr;
    if (GetForceLocalIP() != "") {
        LogAndWrite(f, "Forced local IP address." + GetForceLocalIP());
        addr.Hostname(GetForceLocalIP());
        testSocket = new wxDatagramSocket(addr, wxSOCKET_NOWAIT);
    } else {
        addr.AnyAddress();
        testSocket = new wxDatagramSocket(addr, wxSOCKET_NOWAIT);
        addr.Hostname(wxGetFullHostName());
        if (addr.IPAddress() == "255.255.255.255") {
            addr.Hostname(wxGetHostName());
        }
    }

    LogAndWrite(f, "");
    LogAndWrite(f, "Full host name: " + wxGetFullHostName().ToStdString());
    LogAndWrite(f, "IP Address we are outputing data from: " + addr.IPAddress().ToStdString());
    LogAndWrite(f, "If your PC has multiple network connections (such as wired and wireless) this should be the IP Address of the adapter your controllers are connected to. If it isnt your controllers may not receive output data.");
    LogAndWrite(f, "If you are experiencing this problem you may need to set the local IP address to use.");

    if (testSocket == nullptr || !testSocket->IsOk() || testSocket->Error()) {
        wxString msg("    ERR: Cannot create socket on IP address '");
        msg += addr.IPAddress();
        msg += "'. Is the network connected?    ";
        if (testSocket != nullptr && testSocket->IsOk()) {
            msg = msg + wxString::Format("Error %d : ", testSocket->LastError()) + DecodeIPError(testSocket->LastError());
        }
        LogAndWrite(f, msg.ToStdString());
        errcount++;
    }

    if (testSocket != nullptr) {
        delete testSocket;
    }

    errcountsave = errcount;
    warncountsave = warncount;

    // Missing networks file
    LogAndWrite(f, "");
    LogAndWrite(f, "Missing networks file");

    wxString file = GetShowDir() + "/" + _outputManager->GetNetworksFileName();

    if (!wxFile::Exists(file)) {
        wxString msg = wxString::Format("    ERR: %s not found. It should be here '%s'.", _outputManager->GetNetworksFileName(), file);
        LogAndWrite(f, msg.ToStdString());
        errcount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // xlights and xschedule show directories match
    LogAndWrite(f, "");
    LogAndWrite(f, "xSchedule show directory matches xLights");

    std::string xlsd = xLightsShowDir();
    std::string xssd = xScheduleShowDir();

    if (xlsd != "" && xssd != "" && !ScheduleConfig::IsSameFolder(xlsd, xssd)) {
        wxString msg = wxString::Format("    ERR: xLights show directory %s does not match xSchedule show directory %s.", xlsd.c_str(), xssd.c_str());
        LogAndWrite(f, msg.ToStdString());
        errcount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    LogAndWrite(f, "");
    LogAndWrite(f, "Inactive Outputs");

    // Check for inactive outputs
    for (const auto& it : _outputManager->GetControllers()) {
        if (!it->IsActive()) {
            wxString msg = wxString::Format("    WARN: Inactive controller %s:%s:%s.",
                                            it->GetName().c_str(), it->GetColumn1Label().c_str(), it->GetColumn2Label().c_str());
            LogAndWrite(f, msg.ToStdString());
            warncount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // multiple outputs to same universe and same IP
    LogAndWrite(f, "");
    LogAndWrite(f, "Multiple outputs sending to same destination");

    std::list<std::string> used;
    auto outputs = _outputManager->GetAllOutputs();
    for (auto n = outputs.begin(); n != outputs.end(); ++n) {
        if ((*n)->IsIpOutput()) {
            std::string usedval = (*n)->GetIP() + "|" + (*n)->GetUniverseString();

            if (std::find(used.begin(), used.end(), usedval) != used.end()) {
                wxString msg = wxString::Format("    ERR: Multiple outputs being sent to the same controller (%s) and universe %s.", (*n)->GetIP().c_str(), (*n)->GetUniverseString().c_str());
                LogAndWrite(f, msg.ToStdString());
                errcount++;
            } else {
                used.push_back(usedval);
            }
        } else if ((*n)->IsSerialOutput()) {
            if (std::find(used.begin(), used.end(), (*n)->GetCommPort()) != used.end()) {
                wxString msg = wxString::Format("    ERR: Multiple outputs being sent to the same comm port %s %s.", (*n)->GetType().c_str(), (*n)->GetCommPort().c_str());
                LogAndWrite(f, msg.ToStdString());
                errcount++;
            } else {
                used.push_back((*n)->GetCommPort());
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // duplicate playlist names
    LogAndWrite(f, "");
    LogAndWrite(f, "Duplicate playlist names");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto n1 = n;
        ++n1;

        while (n1 != _playLists.end()) {
            if ((*n1)->GetNameNoTime() == (*n)->GetNameNoTime()) {
                wxString msg = wxString::Format("    ERR: Multiple PlayLists named '%s'. Commands which rely on playlist name may pick up the wrong one.", (*n)->GetNameNoTime().c_str());
                LogAndWrite(f, msg.ToStdString());
                errcount++;
            }

            ++n1;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Duplicate step names within a playlist
    LogAndWrite(f, "");
    LogAndWrite(f, "Duplicate step names within a playlist");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            auto s1 = s;
            ++s1;

            while (s1 != steps.end()) {
                if ((*s1)->GetNameNoTime() == (*s)->GetNameNoTime()) {
                    wxString msg = wxString::Format("    ERR: Multiple playlists steps named '%s' exist in playlist '%s'. Commands which rely on step name may pick up the wrong one.", (*s)->GetNameNoTime().c_str(), (*n)->GetNameNoTime().c_str());
                    LogAndWrite(f, msg.ToStdString());
                    errcount++;
                }

                ++s1;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Duplicate schedule names within a playlist
    LogAndWrite(f, "");
    LogAndWrite(f, "Duplicate schedule names within a playlist");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto scheds = (*n)->GetSchedules();
        for (auto s = scheds.begin(); s != scheds.end(); ++s) {
            auto s1 = s;
            ++s1;

            while (s1 != scheds.end()) {
                if ((*s1)->GetName() == (*s)->GetName()) {
                    wxString msg = wxString::Format("    ERR: Multiple playlists schedules named '%s' exist in playlist '%s'. Commands which rely on schedule name may pick up the wrong one.", (*s)->GetName().c_str(), (*n)->GetNameNoTime().c_str());
                    LogAndWrite(f, msg.ToStdString());
                    errcount++;
                }

                ++s1;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Empty Playlists
    LogAndWrite(f, "");
    LogAndWrite(f, "Empty playlists");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        if ((*n)->GetSteps().size() == 0) {
            wxString msg = wxString::Format("    WARN: Playlist '%s' has no steps.", (*n)->GetNameNoTime().c_str());
            LogAndWrite(f, msg.ToStdString());
            warncount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Empty Steps
    LogAndWrite(f, "");
    LogAndWrite(f, "Empty steps");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            if ((*s)->GetItems().size() == 0) {
                wxString msg = wxString::Format("    WARN: Playlist '%s' has step '%s' with no items.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str());
                LogAndWrite(f, msg.ToStdString());
                warncount++;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Multiple FSEQ in one step
    LogAndWrite(f, "");
    LogAndWrite(f, "Steps with multiple FSEQs");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            int fseqcount = 0;

            auto items = (*s)->GetItems();

            for (auto i = items.begin(); i != items.end(); ++i) {
                if (wxString((*i)->GetTitle()).Contains("FSEQ")) {
                    fseqcount++;
                }
            }

            if (fseqcount > 1) {
                wxString msg = wxString::Format("    WARN: Playlist '%s' has step '%s' with more than one FSEQ item.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str());
                LogAndWrite(f, msg.ToStdString());
                warncount++;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // FSEQ files with wrong number of channels
    LogAndWrite(f, "");
    long totalChannels = _outputManager->GetTotalChannels();
    std::string title = wxString::Format("FSEQs without %ld channels", totalChannels).ToStdString();
    LogAndWrite(f, title);
    totalChannels = roundTo4(totalChannels);

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            auto items = (*s)->GetItems();

            for (auto i = items.begin(); i != items.end(); ++i) {
                if (wxString((*i)->GetTitle()).Contains("FSEQ")) {
                    long ch = roundTo4((*i)->GetFSEQChannels());
                    if (ch < totalChannels) {
                        wxString msg = wxString::Format("    ERR: Playlist '%s' has step '%s' with FSEQ item %s with %ld channels when it should be %ld channels.",
                                                        (*n)->GetNameNoTime().c_str(),
                                                        (*s)->GetNameNoTime().c_str(),
                                                        (*i)->GetNameNoTime().c_str(),
                                                        ch,
                                                        totalChannels);
                        LogAndWrite(f, msg.ToStdString());
                        errcount++;
                    } else if (ch > totalChannels) {
                        wxString msg = wxString::Format("    WARN: Playlist '%s' has step '%s' with FSEQ item %s with %ld channels when only %ld channels are configured to be sent out.",
                                                        (*n)->GetNameNoTime().c_str(),
                                                        (*s)->GetNameNoTime().c_str(),
                                                        (*i)->GetNameNoTime().c_str(),
                                                        ch,
                                                        totalChannels);
                        LogAndWrite(f, msg.ToStdString());
                        warncount++;
                    }
                }
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Multiple Audio in one step
    LogAndWrite(f, "");
    LogAndWrite(f, "Steps with multiple audio");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            int audiocount = 0;

            auto items = (*s)->GetItems();

            for (auto i = items.begin(); i != items.end(); ++i) {
                if ((*i)->GetTitle() == "Audio" ||
                    ((*i)->GetTitle() == "FSEQ" && ((PlayListItemFSEQ*)(*i))->GetAudioFilename() != "") ||
                    ((*i)->GetTitle() == "FSEQ & Video" && ((PlayListItemFSEQVideo*)(*i))->GetAudioFilename() != "")) {
                    audiocount++;
                }
            }

            if (audiocount > 1) {
                wxString msg = wxString::Format("    WARN: Playlist '%s' has step '%s' with more than one item playing audio.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str());
                LogAndWrite(f, msg.ToStdString());
                warncount++;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Excessive video in one step
    LogAndWrite(f, "");
    LogAndWrite(f, "Steps with many videos");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            int videocount = 0;

            auto items = (*s)->GetItems();

            for (auto i = items.begin(); i != items.end(); ++i) {
                if (wxString((*i)->GetTitle()).Contains("Video")) {
                    videocount++;
                }
            }

            if (videocount > 3) {
                wxString msg = wxString::Format("    WARN: Playlist '%s' has step '%s' with more than 3 videos ... this can cause performance issues.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str());
                LogAndWrite(f, msg.ToStdString());
                warncount++;
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Duplicate matrix names
    LogAndWrite(f, "");
    LogAndWrite(f, "Duplicate matrix names");

    used.clear();
    auto m = GetOptions()->GetMatrices();
    for (auto n = m->begin(); n != m->end(); ++n) {
        if (std::find(used.begin(), used.end(), (*n)->GetName()) == used.end()) {
            used.push_back((*n)->GetName());
        } else {
            wxString msg = wxString::Format("    ERR: Duplicate matrix '%s'.", (*n)->GetName().c_str());
            LogAndWrite(f, msg.ToStdString());
            errcount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Matrices greater than available channels
    LogAndWrite(f, "");
    LogAndWrite(f, "Matrices greater than available channels");

    m = GetOptions()->GetMatrices();
    for (auto n = m->begin(); n != m->end(); ++n) {
        if ((*n)->GetStartChannelAsNumber() + (*n)->GetChannels() >= (size_t)_outputManager->GetTotalChannels()) {
            wxString msg = wxString::Format("    ERR: Matrix '%s' is meant to finish at channel %ld but last available channel is %ld.", (*n)->GetName().c_str(), (long)((*n)->GetStartChannelAsNumber() + (*n)->GetChannels()), (long)_outputManager->GetTotalChannels());
            LogAndWrite(f, msg.ToStdString());
            errcount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Virtual Matrices greater than available channels
    LogAndWrite(f, "");
    LogAndWrite(f, "Virtual matrices greater than available channels");

    auto vm = GetOptions()->GetVirtualMatrices();
    for (auto n = vm->begin(); n != vm->end(); ++n) {
        if ((*n)->GetStartChannelAsNumber() + (*n)->GetChannels() - 1 > (size_t)_outputManager->GetTotalChannels()) {
            wxString msg = wxString::Format("    ERR: Virtual Matrix '%s' is meant to finish at channel %ld but last available channel is %ld.", (*n)->GetName().c_str(), (long)((*n)->GetStartChannelAsNumber() + (*n)->GetChannels() - 1), (long)_outputManager->GetTotalChannels());
            LogAndWrite(f, msg.ToStdString());
            errcount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Duplicate virtual matrix names
    LogAndWrite(f, "");
    LogAndWrite(f, "Duplicate virtual matrix names");

    used.clear();
    vm = GetOptions()->GetVirtualMatrices();
    for (auto n = vm->begin(); n != vm->end(); ++n) {
        if (std::find(used.begin(), used.end(), (*n)->GetName()) == used.end()) {
            used.push_back((*n)->GetName());
        } else {
            wxString msg = wxString::Format("    ERR: Duplicate virtual matrix '%s'.", (*n)->GetName().c_str());
            LogAndWrite(f, msg.ToStdString());
            errcount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Output processing present
    LogAndWrite(f, "");
    LogAndWrite(f, "Output processing present");

    if (_outputProcessing.size() > 0) {
        wxString msg = wxString::Format("    WARN: %d Output Processes present. This would explain why xSchedule output looks different to xlights output.", (int)_outputProcessing.size());
        LogAndWrite(f, msg.ToStdString());
        warncount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Multiple schedules with the same priority
    LogAndWrite(f, "");
    LogAndWrite(f, "Multiple schedules with the same priority");

    std::vector<int> priorities;
    priorities.resize(20);

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto scheds = (*n)->GetSchedules();
        for (auto s = scheds.begin(); s != scheds.end(); ++s) {
            priorities[(*s)->GetPriority()]++;
        }
    }

    for (uint32_t i = 0; i < 20; ++i) {
        if (priorities[i] > 1) {
            wxString msg = wxString::Format("    WARN: More than one schedule has priority %u. If these trigger at the same time then it is not certain which we will choose.", i);
            LogAndWrite(f, msg.ToStdString());
            warncount++;
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // No web password
    LogAndWrite(f, "");
    LogAndWrite(f, "No password set on website");

    if (_scheduleOptions->GetPassword() == "") {
        wxString msg = wxString::Format("    WARN: Website does not have a password set.");
        LogAndWrite(f, msg.ToStdString());
        warncount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Non standard web port
    LogAndWrite(f, "");
    LogAndWrite(f, "Non standard web port");

    if (_scheduleOptions->GetWebServerPort() != 80) {
        wxString msg = wxString::Format("    WARN: Website is listening on a non standard port %d.", _scheduleOptions->GetWebServerPort());
        LogAndWrite(f, msg.ToStdString());
        warncount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // API Only mode
    LogAndWrite(f, "");
    LogAndWrite(f, "Web in API only mode");

    if (_scheduleOptions->GetAPIOnly()) {
        wxString msg = wxString::Format("    WARN: Website is listening only for API calls. It will not allow connection from a browser.");
        LogAndWrite(f, msg.ToStdString());
        warncount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Web server folder blank
    LogAndWrite(f, "");
    LogAndWrite(f, "Web server folder blank");

    if (_scheduleOptions->GetWWWRoot() == "") {
        wxString msg = wxString::Format("    ERR: Website folder is set to blank.");
        LogAndWrite(f, msg.ToStdString());
        errcount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Missing index.html
    LogAndWrite(f, "");
    LogAndWrite(f, "Missing index.html");

    wxString d;
#ifdef __WXMSW__
    d = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
#elif __LINUX__
    d = wxStandardPaths::Get().GetDataDir();
    if (!wxDir::Exists(d)) {
        d = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
    }
#else
    d = wxStandardPaths::Get().GetResourcesDir();
#endif

    file = d + "/" + _scheduleOptions->GetWWWRoot() + "/index.html";

    if (_scheduleOptions->GetWWWRoot() != "" && !wxFile::Exists(file)) {
        wxString msg = wxString::Format("    ERR: index.html not found. It should be here '%s'.", file);
        LogAndWrite(f, msg.ToStdString());
        errcount++;
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }
    errcountsave = errcount;
    warncountsave = warncount;

    // Missing referenced files
    LogAndWrite(f, "");
    LogAndWrite(f, "Missing referenced files");

    for (auto n = _playLists.begin(); n != _playLists.end(); ++n) {
        auto steps = (*n)->GetSteps();
        for (auto s = steps.begin(); s != steps.end(); ++s) {
            auto items = (*s)->GetItems();

            for (auto i = items.begin(); i != items.end(); ++i) {
                auto missing = (*i)->GetMissingFiles();

                for (auto ff = missing.begin(); ff != missing.end(); ++ff) {
                    wxString msg = wxString::Format("    ERR: Playlist '%s' step '%s' item '%s' references file '%s' which does not exist.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str(), (*i)->GetNameNoTime().c_str(), ff->c_str());
                    LogAndWrite(f, msg.ToStdString());
                    errcount++;

                    for (size_t j = 0; j < ff->length(); ++j) {
                        if (ff->at(j) < 32) {
                            msg = wxString::Format("    ERR: Playlist '%s' step '%s' item '%s' references file '%s' which contains illegal character 0x%x.", (*n)->GetNameNoTime().c_str(), (*s)->GetNameNoTime().c_str(), (*i)->GetNameNoTime().c_str(), ff->c_str(), (int)ff->at(j));
                            LogAndWrite(f, msg.ToStdString());
                            errcount++;
                        }
                    }
                }
            }
        }
    }

    if (errcount + warncount == errcountsave + warncountsave) {
        LogAndWrite(f, "    No problems found");
    }

    LogAndWrite(f, "");
    LogAndWrite(f, "Check schedule done.");
    LogAndWrite(f, wxString::Format("Errors: %d. Warnings: %d", errcount, warncount).ToStdString());

    if (f.IsOpened()) {
        f.Close();

        wxFileType* ft = wxTheMimeTypesManager->GetFileTypeFromExtension("txt");
        if (ft) {
            wxString command = ft->GetOpenCommand(filename);

            spdlog::debug("Viewing xSchedule Check Schedule results {}.", filename.ToStdString());

            wxExecute(command);
            delete ft;
        } else {
            spdlog::warn("Unable to view xSchedule Check Schedule results {}.", filename.ToStdString());
            wxMessageBox(_("Unable to show xSchedule Check Schedule results."), _("Error"));
        }
    }
}

void ScheduleManager::ImportxLightsSchedule(const std::string& filename) {

    wxFileName fn(filename);
    std::string base = fn.GetPath().ToStdString();

    wxXmlDocument doc;
    doc.Load(filename);

    if (doc.IsOk()) {
        for (wxXmlNode* n = doc.GetRoot()->GetChildren(); n != nullptr; n = n->GetNext()) {
            if (n->GetName() == "playlists") {
                for (wxXmlNode* n1 = n->GetChildren(); n1 != nullptr; n1 = n1->GetNext()) {
                    if (n1->GetName() == "playlist") {
                        std::string name = n1->GetAttribute("name", "").ToStdString();
                        PlayList* p = new PlayList();
                        p->SetName(name);
                        for (wxXmlNode* n2 = n1->GetChildren(); n2 != nullptr; n2 = n2->GetNext()) {
                            if (n2->GetName() == "listitem") {
                                std::string itemname = n2->GetAttribute("name", "").ToStdString();
                                int delay = wxAtoi(n2->GetAttribute("delay", "0"));
                                PlayListStep* step = new PlayListStep();
                                step->SetName(itemname);
                                std::string ext = wxFileName(itemname).GetExt().Lower().ToStdString();
                                if (PlayListItemAudio::IsAudio(ext)) {
                                    PlayListItemAudio* pli = new PlayListItemAudio();
                                    pli->SetAudioFile(base + "/" + itemname);
                                    pli->SetDelay(delay * 1000);
                                    step->AddItem(pli);
                                } else if (PlayListItemVideo::IsVideo(ext)) {
                                    PlayListItemVideo* pli = new PlayListItemVideo(_scheduleOptions);
                                    pli->SetVideoFile(base + "/" + itemname);
                                    pli->SetDelay(delay * 1000);
                                    step->AddItem(pli);
                                } else if (ext == "fseq") {
                                    PlayListItemFSEQ* pli = new PlayListItemFSEQ(_outputManager);
                                    pli->SetFSEQFileName(base + "/" + itemname);
                                    pli->SetDelay(delay * 1000);
                                    step->AddItem(pli);
                                }
                                p->AddStep(step, p->GetSteps().size());
                            }
                        }
                        AddPlayList(p);
                    }
                }
            }
        }
    } else {
        spdlog::error("Invalid xLights schedule file.");
        wxMessageBox("Invalid xLights schedule file.");
    }
}

bool ScheduleManager::DoXyzzy(const wxString& command, const wxString& parameters, wxString& result, const wxString& reference) {
    _lastXyzzyCommand = wxDateTime::Now();

    if (command == "highscore") {
        Xyzzy::GetHighScoreJSON(result, reference);
        return true;
    } else if (command == "highscore2") {
        Xyzzy2::GetHighScoreJSON(result, reference);
        return true;
    }

    if (_xyzzy == nullptr) {
        if (command.EndsWith("2")) {
            _xyzzy = new Xyzzy2();
        } else {
            _xyzzy = new Xyzzy();
        }
        wxCommandEvent event(EVT_SCHEDULECHANGED);
        wxPostEvent(wxGetApp().GetTopWindow(), event);
    }

    if (_xyzzy != nullptr) {
        wxString c = command;
        if (c.EndsWith("2"))
            c = c.SubString(0, c.Length() - 2);

        if (c == "initialise") {
            _xyzzy->Initialise(parameters, result, reference, _outputManager);
        } else if (c == "close") {
            // clear the screen
            _xyzzy->DrawBlack(_buffer, _outputManager->GetTotalChannels());

            _xyzzy->Close(result, reference);
            delete _xyzzy;
            _xyzzy = nullptr;

            wxCommandEvent event(EVT_SCHEDULECHANGED);
            wxPostEvent(wxGetApp().GetTopWindow(), event);
        } else {
            _xyzzy->Action(c, parameters, result, reference);
        }

        if (_xyzzy != nullptr && !_xyzzy->IsOk()) {
            delete _xyzzy;
            _xyzzy = nullptr;
        }
    } else {
        return false;
    }

    return true;
}

PixelData::PixelData(size_t startChannel, const wxString& data, APPLYMETHOD blendMode) {
    _data = nullptr;
    _startChannel = startChannel;
    _blendMode = blendMode;

    ExtractData(data);
}

PixelData::PixelData(size_t startChannel, size_t channels, const wxColor& c, APPLYMETHOD blendMode) {
    _startChannel = startChannel;
    _blendMode = blendMode;
    _size = channels;
    _data = (uint8_t*)malloc(_size);

    SetColor(c, blendMode);
}

void PixelData::SetColor(const wxColor& c, APPLYMETHOD blendMode) {
    _blendMode = blendMode;
    size_t size3 = (_size / 3) * 3;

    for (size_t i = 0; i < size3; i += 3) {
        _data[i] = c.Red();
        _data[i + 1] = c.Green();
        _data[i + 2] = c.Blue();
    }

    // handle weird channel counts
    if (_size > size3) {
        _data[size3] = c.Red();
        if (_size > size3 + 1) {
            _data[size3 + 1] = c.Green();
        }
    }
}

PixelData::~PixelData() {
    if (_data != nullptr) {
        free(_data);
    }
}

void PixelData::Set(uint8_t* buffer, size_t size) {
    if (_data != nullptr) {
        Blend(buffer, size, _data, _size, _blendMode, _startChannel - 1);
    }
}

void PixelData::ExtractData(const wxString& data) {
    std::vector<unsigned char> dout;
    base64_decode(data, dout);
    _size = dout.size();

    if (_data != nullptr) {
        delete _data;
    }
    _data = (uint8_t*)malloc(_size);
    if (_data != nullptr) {
        for (size_t i = 0; i < _size; ++i) {
            *(_data + i) = dout[i];
        }
    }
}

void PixelData::SetData(const wxString& data, APPLYMETHOD blendMode) {
    _blendMode = blendMode;

    ExtractData(data);
}

bool ScheduleManager::DoText(PlayListItemText* pliText, const wxString& text, const wxString& properties) {
    bool valid = true;

    if (pliText == nullptr)
        return false;

    pliText->SetText(text);

    wxArrayString p = wxSplit(properties, '|');

    for (auto it = p.begin(); it != p.end(); ++it) {
        wxArrayString pv = wxSplit(*it, '=');

        if (pv.size() == 2) {
            wxString pvl = pv[0].Lower();

            if (pvl == "color" || pvl == "colour") {
                wxColour c(pv[1]);
                pliText->SetColour(c);
            } else if (pvl == "blendmode") {
                pliText->SetBlendMode(pv[1]);
            } else if (pvl == "speed") {
                pliText->SetSpeed(wxAtoi(pv[1]));
            } else if (pvl == "orientation") {
                pliText->SetOrientation(pv[1]);
            } else if (pvl == "movement") {
                pliText->SetOrientation(pv[1]);
            } else if (pvl == "font") {
                wxFont font;
                font.SetNativeFontInfoUserDesc(pv[1]);
                auto f = wxTheFontList->FindOrCreateFont(font.GetPointSize(), font.GetFamily(), font.GetStyle(), font.GetWeight(), font.GetUnderlined(), font.GetFaceName(), font.GetEncoding());
                pliText->SetFont(f);
            } else if (pvl == "x") {
                pliText->SetX(wxAtoi(pv[1]));
            } else if (pvl == "y") {
                pliText->SetY(wxAtoi(pv[1]));
            } else {
                valid = false;
            }
        } else {
            valid = false;
        }
    }

    return valid;
}

void ScheduleManager::StartVirtualMatrices() {
    auto v = GetOptions()->GetVirtualMatrices();

    for (auto it = v->begin(); it != v->end(); ++it) {
        (*it)->Start();
    }
}

void ScheduleManager::StopVirtualMatrices() {
    auto v = GetOptions()->GetVirtualMatrices();

    for (auto it = v->begin(); it != v->end(); ++it) {
        (*it)->Stop();
    }
}

std::string ScheduleManager::FindStepForFSEQ(const std::string& fseq) const {

    for (auto it : _playLists) {
        PlayListStep* pls = it->GetStepWithFSEQ(fseq);

        if (pls != nullptr) {
            return pls->GetNameNoTime();
        }
    }

    spdlog::debug("No step with fseq file '{}' found.", fseq);
    return "";
}

void ScheduleManager::StartTiming(const std::string timingName) {

    // find this fseq file and run it
    PlayList* pl = GetRunningPlayList();
    PlayListStep* pls = nullptr;
    if (pl != nullptr) {
        pls = pl->GetStepWithTimingName(timingName);
        StopPlayList(pl, false);
    }

    if (pls == nullptr) {
        for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
            pls = (*it)->GetStepWithTimingName(timingName);

            if (pls != nullptr) {
                pl = *it;
                break;
            }
        }
    }

    if (pl != nullptr && pls != nullptr) {
        spdlog::debug("... Starting {} {}.", pl->GetNameNoTime(), pls->GetNameNoTime());

        size_t rate = 0;
        PlayPlayList(pl, rate, false, pls->GetNameNoTime(), true);

        if (_overrideMS != 0) {
            rate = _overrideMS;
        }

        wxCommandEvent event1(EVT_FRAMEMS);
        event1.SetInt(rate);
        wxPostEvent(wxGetApp().GetTopWindow(), event1);

        wxCommandEvent event2(EVT_SCHEDULECHANGED);
        wxPostEvent(wxGetApp().GetTopWindow(), event2);
    }
}

void ScheduleManager::StartStep(const std::string stepName) {

    // find this step and run it
    PlayList* pl = GetRunningPlayList();
    PlayListStep* pls = nullptr;
    if (pl != nullptr) {
        pls = pl->GetStep(stepName);
        StopPlayList(pl, false);
    }

    if (pls == nullptr) {
        for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
            pls = (*it)->GetStep(stepName);

            if (pls != nullptr) {
                pl = *it;
                break;
            }
        }
    }

    if (pl != nullptr && pls != nullptr) {
        spdlog::debug("... Starting {} {}.", pl->GetNameNoTime(), pls->GetNameNoTime());

        size_t rate = 0;
        PlayPlayList(pl, rate, false, pls->GetNameNoTime(), true);

        if (_overrideMS != 0) {
            rate = _overrideMS;
        }

        wxCommandEvent event1(EVT_FRAMEMS);
        event1.SetInt(rate);
        wxPostEvent(wxGetApp().GetTopWindow(), event1);

        wxCommandEvent event2(EVT_SCHEDULECHANGED);
        wxPostEvent(wxGetApp().GetTopWindow(), event2);
    }
}

PlayListItem* ScheduleManager::FindRunProcessNamed(const std::string& item) const {
    PlayListItem* pli = nullptr;

    for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
        pli = (*it)->FindRunProcessNamed(item);

        if (pli != nullptr)
            break;
    }

    return pli;
}

PlayListStep* ScheduleManager::GetStepContainingPlayListItem(wxUint32 id) const {
    PlayListStep* pls = nullptr;

    for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
        pls = (*it)->GetStepContainingPlayListItem(id);

        if (pls != nullptr)
            break;
    }

    return pls;
}

std::string ScheduleManager::DecodePlayList(const std::string& playlistparameter) {
    if (wxString(playlistparameter).Lower().StartsWith("id:")) {
        wxUint32 id = wxAtoi(wxString(playlistparameter).Mid(3));
        auto p = GetPlayList(id);
        if (p != nullptr) {
            return p->GetNameNoTime();
        }
        return "";
    }
    return playlistparameter;
}

std::string ScheduleManager::DecodeStep(const std::string& stepparameter) {
    if (wxString(stepparameter).Lower().StartsWith("id:")) {
        wxUint32 id = wxAtoi(wxString(stepparameter).Mid(3));
        return PlayListStep::GetStepNameWithId(id);
    }

    return stepparameter;
}

std::string ScheduleManager::DecodeSchedule(const std::string& scheduleparameter) {
    if (wxString(scheduleparameter).Lower().StartsWith("id:")) {
        wxUint32 id = wxAtoi(wxString(scheduleparameter).Mid(3));
        for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
            auto s = (*it)->GetSchedule(id);
            if (s != nullptr) {
                return s->GetName();
            }
        }

        return "";
    }

    return scheduleparameter;
}

std::string ScheduleManager::DecodeItem(const std::string& itemparameter) {
    if (wxString(itemparameter).Lower().StartsWith("id:")) {
        wxUint32 id = wxAtoi(wxString(itemparameter).Mid(3));
        for (auto it = _playLists.begin(); it != _playLists.end(); ++it) {
            auto i = (*it)->GetItem(id);
            if (i != nullptr) {
                return i->GetNameNoTime();
            }
        }

        return "";
    }

    return itemparameter;
}

std::string ScheduleManager::DecodeButton(const std::string& buttonlabelparameter) {
    if (wxString(buttonlabelparameter).Lower().StartsWith("id:")) {
        wxUint32 id = wxAtoi(wxString(buttonlabelparameter).Mid(3));
        auto b = _scheduleOptions->GetButton(id);
        if (b != nullptr) {
            return b->GetLabel();
        }
        return "";
    }
    return buttonlabelparameter;
}

std::string ScheduleManager::xLightsShowDir() {
    return ScheduleConfig::GetXLightsSetting("LastDir");
}

std::string ScheduleManager::xScheduleShowDir() {
    wxString showDir = "";

    auto* config = GetXLightsConfig();
    if (config != nullptr) {
        config->Read("SchedulerLastDir", &showDir);
    }

    return showDir.ToStdString();
}

bool ScheduleManager::IsTest() const {
    return _testMode;
}

void ScheduleManager::TestFrame(uint8_t* buffer, long totalChannels, long msec) {
    if (GetOptions() == nullptr || GetOptions()->GetTestOptions() == nullptr || totalChannels == 0)
        return;

    auto mode = GetOptions()->GetTestOptions()->GetModeCode();
    auto interval = GetOptions()->GetTestOptions()->GetInterval();
    auto level1 = GetOptions()->GetTestOptions()->GetLevel1();
    auto level2 = GetOptions()->GetTestOptions()->GetLevel2();
    long start = 0;
    long end = totalChannels - 1;
    if (GetOptions()->GetTestOptions()->HasBounds()) {
        GetOptions()->GetTestOptions()->GetBounds(start, end);
    }

    if (mode == TESTMODE::TEST_ALTERNATE) {
        auto v1 = level1;
        auto v2 = level2;
        int pos = msec % (2 * interval);
        if (pos >= interval) {
            v1 = level2;
            v2 = level1;
        }
        for (size_t i = (size_t)start; i <= (size_t)end; ++i) {
            if (i % 2 == 0) {
                buffer[i] = v1;
            } else {
                buffer[i] = v2;
            }
        }
    } else if (mode == TESTMODE::TEST_LEVEL1) {
        memset(&buffer[start], level1, end - start + 1);
    } else {
        uint8_t a = level2;
        uint8_t b = level2;
        uint8_t c = level2;
        if (mode == TESTMODE::TEST_A_B_C) {
            int pos = msec % (3 * interval);
            if (pos < interval) {
                a = level1;
            } else if (pos < 2 * interval) {
                b = level1;
            } else {
                c = level1;
            }
        } else if (mode == TESTMODE::TEST_A_B_C_ALL) {
            int pos = msec % (4 * interval);
            if (pos < interval) {
                a = level1;
            } else if (pos < 2 * interval) {
                b = level1;
            } else if (pos < 3 * interval) {
                c = level1;
            } else {
                a = level1;
                b = level1;
                c = level1;
            }
        } else if (mode == TESTMODE::TEST_A_B_C_ALL_NONE) {
            int pos = msec % (5 * interval);
            if (pos < interval) {
                a = level1;
            } else if (pos < 2 * interval) {
                b = level1;
            } else if (pos < 3 * interval) {
                c = level1;
            } else if (pos < 4 * interval) {
                a = level1;
                b = level1;
                c = level1;
            }
        } else if (mode == TESTMODE::TEST_A) {
            a = level1;
        } else if (mode == TESTMODE::TEST_B) {
            b = level1;
        } else if (mode == TESTMODE::TEST_C) {
            c = level1;
        }
        size_t tc = ((size_t)end / 3) * 3;
        for (size_t i = (size_t)start; i <= tc; i += 3) {
            buffer[i] = a;
            buffer[i + 1] = b;
            buffer[i + 2] = c;
        }

        if (tc == (size_t)(end - 1)) {
            buffer[tc] = a;
        } else if (tc == (size_t)(end - 2)) {
            buffer[tc + 1] = b;
        }
    }
}

void ScheduleManager::ForceLocalIP(const std::string& forceLocalIP) {
    // if blank then we do nothing
    if (forceLocalIP == "")
        return;

    _outputManager->SetGlobalForceLocalIP(forceLocalIP);
    for (auto& it : _outputManager->GetControllers()) {
        if (dynamic_cast<ControllerEthernet*>(it) != nullptr)
            dynamic_cast<ControllerEthernet*>(it)->SetForceLocalIP(""); // clear any controller force local ips
    }
}

void ScheduleManager::SetForceLocalIP(const std::string& forceLocalIP) {
    auto* config = GetXLightsConfig();
    config->Write("xLightsLocalIP", wxString(forceLocalIP));
    config->Flush();

    if (forceLocalIP != "") {
        ForceLocalIP(forceLocalIP);
    } else {
        // here we have an issue ... the networks file essentially needs to be reloaded to restore all the forced ips
        bool outputting = false;
        if (_outputManager->IsOutputting()) {
            _outputManager->StopOutput();
            ScheduleConfig::SetBool("OutputActive", false);
        }
        _outputManager->Load(_showDir);
        if (outputting) {
            if (StartOutputToLights()) ScheduleConfig::SetBool("OutputActive", true);
        }
    }
}

std::string ScheduleManager::GetForceLocalIP() const {
    auto* config = GetXLightsConfig();
    wxString localIP;
    config->Read("xLightsLocalIP", &localIP, "");
    if (localIP != "") {
        if (ip_utils::IsValidLocalIP(localIP.ToStdString())) {
            spdlog::info("Forcing output via {}.", localIP.ToStdString());
        } else {
            spdlog::warn("Forcing output via {} IGNORED as the IP does not exist on this machine at this time.", localIP.ToStdString());
        }
    }
    return localIP;
}


