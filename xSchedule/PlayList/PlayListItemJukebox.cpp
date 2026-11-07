/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/xml/xml.h>

#include <nlohmann/json.hpp>

#include <thread>

#include "PlayListItemJukebox.h"
#include "PlayListItemJukeboxPanel.h"
#include "../xlights/src-core/utils/UtilFunctions.h"
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"

#include <log.h>
#include "../Diagnostics.h"

PlayListItemJukebox::PlayListItemJukebox(wxXmlNode* node) :
    PlayListItem(node)
{
    _started = false;
    _jukeboxButton = 1;
    _port = "A";
    SetEnumState(ENUMJUKEBOX::ENUM_STATE_DONE);
    PlayListItemJukebox::Load(node);
}

void PlayListItemJukebox::Load(wxXmlNode* node)
{
    PlayListItem::Load(node);
    _jukeboxButton = wxAtoi(node->GetAttribute("Button", "1"));
    _port = node->GetAttribute("Port", "A");
}

PlayListItemJukebox::PlayListItemJukebox() :
    PlayListItem()
{
    _type = "PLIJukebox";
    _started = false;
    _jukeboxButton = 1;
    _port = "A";
    SetEnumState(ENUMJUKEBOX::ENUM_STATE_DONE);
}

PlayListItem* PlayListItemJukebox::Copy(const bool isClone) const
{
    PlayListItemJukebox* res = new PlayListItemJukebox();
    res->_jukeboxButton = _jukeboxButton;
    res->_port = _port;
    res->_started = false;
    PlayListItem::Copy(res, isClone);

    return res;
}

wxXmlNode* PlayListItemJukebox::Save()
{
    wxXmlNode* node = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, GetType());

    node->AddAttribute("Button", wxString::Format("%d", _jukeboxButton));
    node->AddAttribute("Port", _port);

    PlayListItem::Save(node);

    return node;
}

std::string PlayListItemJukebox::GetTitle() const
{
    return "Play xLights Jukebox Button";
}

void PlayListItemJukebox::Configure(wxNotebook* notebook)
{
    notebook->AddPage(new PlayListItemJukeboxPanel(notebook, this), GetTitle(), true);
}

std::string PlayListItemJukebox::GetNameNoTime() const
{
    if (_name != "")
        return _name;

    return "Play xLights Jukebox Button";
}

void PlayListItemJukebox::Frame(uint8_t* buffer, size_t size, size_t ms, size_t framems, bool outputframe)
{
    if (ms >= _delay && !_started) {
        _started = true;

        spdlog::info("Launching xLights Jukebox Button {}.", _jukeboxButton);

        // xLights can take a while to answer (its request timeout is 30 minutes), so ask from another thread rather
        // than holding up the frame loop and the lights with it
        int port = GetPort();
        int button = _jukeboxButton;
        std::thread([port, button]() {
            Diagnostics::ThreadName name("jukebox");
            // an unexpected reply from xLights must not throw
            try {
                nlohmann::json result = xLightsRequest(port, "{\"cmd\":\"lightsOn\"}");
                if (result.value("res", 0) != 200) {
                    spdlog::error("Failed to turn on output to lights: {}", result.value("msg", std::string()));
                }

                result = xLightsRequest(port, wxString::Format("{\"cmd\":\"playJukebox\",\"button\":%d}", button));
                if (result.value("res", 0) != 200) {
                    spdlog::error("Failed to send jukebox button press: {}", result.value("msg", std::string()));
                }
            } catch (const std::exception& e) {
                spdlog::error("Unexpected reply from xLights for jukebox button {}: {}", button, e.what());
            }
        }).detach();
    }
}

void PlayListItemJukebox::Start(long stepLengthMS)
{
    PlayListItem::Start(stepLengthMS);

    _started = false;
}
