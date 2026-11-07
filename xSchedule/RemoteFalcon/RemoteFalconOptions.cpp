/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "RemoteFalconOptions.h"
#include "RemoteFalconApp.h"
#include "../SettingsFiles.h"

#include <wx/xml/xml.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/wxcrt.h>

#include "../../xlights/src-core/utils/UtilFunctions.h"
#include "../../xlights/src-ui-wx/shared/utils/wxUtilities.h"

RemoteFalconOptions::RemoteFalconOptions()
{
}

void RemoteFalconOptions::Load()
{
    const auto& config = RemoteFalconSettings();
    _token = config.Read("RemoteFalconToken", "");
    _playDuring = config.Read("RemoteFalconPlayDuring", "");
    _playlist = config.ReadLong("RemoteFalconPlaylist", -1);
    _immediatelyInterrupt = config.ReadBool("RemoteFalconImmediatelyInterrupt", true);
    _sendEnableDisable = config.ReadBool("RemoteFalconSendEnableDisable", true);
    _leadTime = config.ReadLong("RemoteFalconLeadTime", 5);
    _effectPlaylist = config.ReadBool("RemoteFalconEffectPlaylist", false);
    _effectMode = (EFFECT_MODE)config.ReadLong("RemoteFalconEffectMode", (int)EFFECT_MODE::EM_PLAY_IMMEDIATELY);
    _messageLevel = (MESSAGE_LEVEL)config.ReadLong("RemoteFalconMessageLevel", (int)MESSAGE_LEVEL::ML_WARN);
    _sendPlayingEffect = config.ReadBool("RemoteFalconSendPlayingEffect", false);
    ClearDirty();
}

void RemoteFalconOptions::Save()
{
    auto& config = RemoteFalconSettings();
    config.Write("RemoteFalconToken", _token);
    config.Write("RemoteFalconPlayDuring", _playDuring);
    config.Write("RemoteFalconPlaylist", _playlist);
    config.Write("RemoteFalconLeadTime", _leadTime);
    config.Write("RemoteFalconImmediatelyInterrupt", _immediatelyInterrupt);
    config.Write("RemoteFalconSendEnableDisable", _sendEnableDisable);
    config.Write("RemoteFalconEffectPlaylist", _effectPlaylist);
    config.Write("RemoteFalconEffectMode", (int)_effectMode);
    config.Write("RemoteFalconMessageLevel", (int)_messageLevel);
    config.Write("RemoteFalconSendPlayingEffect", _sendPlayingEffect);
    config.Flush();
    ClearDirty();
}

bool RemoteFalconOptions::IsValid() const
{
    return _token != "" && _playlist != -1;
}

bool RemoteFalconOptions::IsPlayDuring(const std::string& playlist)
{
	return _playDuring == "" || Contains(_playDuring, "|" + playlist + "|");
}

bool RemoteFalconOptions::IsDirty() const
{
    return _lastSavedChangeCount != _changeCount;
}

void RemoteFalconOptions::ClearDirty()
{
    _lastSavedChangeCount = _changeCount;
}
