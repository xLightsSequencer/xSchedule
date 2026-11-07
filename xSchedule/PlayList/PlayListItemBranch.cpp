/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "PlayListItemBranch.h"
#include "PlayListItemBranchPanel.h"

#include <wx/datetime.h>
#include <wx/notebook.h>
#include <wx/xml/xml.h>

#include <algorithm>
#include <cstdlib>

namespace {
const char* ConditionName(PlayListItemBranch::Condition condition)
{
    switch (condition) {
    case PlayListItemBranch::Condition::Loop:
        return "Loop";
    case PlayListItemBranch::Condition::Always:
        return "Always";
    default:
        return "Time";
    }
}
}

PlayListItemBranch::PlayListItemBranch(wxXmlNode* node) : PlayListItem(node)
{
    PlayListItemBranch::Load(node);
}

PlayListItemBranch::PlayListItemBranch() : PlayListItem()
{
    _type = "PLIBranch";
    SetName("Branch");
}

void PlayListItemBranch::Load(wxXmlNode* node)
{
    PlayListItem::Load(node);
    wxString condition = node->GetAttribute("Condition", "Time");
    _condition = condition == "Loop" ? Condition::Loop : condition == "Always" ? Condition::Always : Condition::TimeOfDay;
    _startTime = node->GetAttribute("StartTime", "21:00").ToStdString();
    _endTime = node->GetAttribute("EndTime", "00:00").ToStdString();
    _loopEvery = std::max(1, wxAtoi(node->GetAttribute("LoopEvery", "2")));
    _loopFrom = std::max(1, wxAtoi(node->GetAttribute("LoopFrom", "2")));
    _trueStep = node->GetAttribute("TrueStep", "").ToStdString();
    _truePlayList = node->GetAttribute("TruePlayList", "").ToStdString();
    _falseStep = node->GetAttribute("FalseStep", "").ToStdString();
}

PlayListItem* PlayListItemBranch::Copy(const bool isClone) const
{
    PlayListItemBranch* res = new PlayListItemBranch();
    res->_condition = _condition;
    res->_startTime = _startTime;
    res->_endTime = _endTime;
    res->_loopEvery = _loopEvery;
    res->_loopFrom = _loopFrom;
    res->_trueStep = _trueStep;
    res->_truePlayList = _truePlayList;
    res->_falseStep = _falseStep;
    PlayListItem::Copy(res, isClone);
    return res;
}

wxXmlNode* PlayListItemBranch::Save()
{
    wxXmlNode* node = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, GetType());
    node->AddAttribute("Condition", ConditionName(_condition));
    node->AddAttribute("StartTime", _startTime);
    node->AddAttribute("EndTime", _endTime);
    node->AddAttribute("LoopEvery", wxString::Format("%d", _loopEvery));
    node->AddAttribute("LoopFrom", wxString::Format("%d", _loopFrom));
    node->AddAttribute("TrueStep", _trueStep);
    node->AddAttribute("TruePlayList", _truePlayList);
    node->AddAttribute("FalseStep", _falseStep);
    PlayListItem::Save(node);
    return node;
}

std::string PlayListItemBranch::GetTitle() const
{
    return "Branch";
}

void PlayListItemBranch::Configure(wxNotebook* notebook)
{
    notebook->AddPage(new PlayListItemBranchPanel(notebook, this), GetTitle(), true);
}

int PlayListItemBranch::ParseTime(const std::string& time)
{
    char* end = nullptr;
    long h = std::strtol(time.c_str(), &end, 10);
    if (end == time.c_str() || *end != ':') return -1;
    const char* minutes = end + 1;
    long m = std::strtol(minutes, &end, 10);
    if (end == minutes || *end != '\0' || h < 0 || h > 23 || m < 0 || m > 59) return -1;
    return (int)(h * 60 + m);
}

bool PlayListItemBranch::IsTrue(const wxDateTime& now, int loop) const
{
    switch (_condition) {
    case Condition::Always:
        return true;
    case Condition::Loop:
        return loop >= _loopFrom && (loop - _loopFrom) % _loopEvery == 0;
    case Condition::TimeOfDay: {
        int start = ParseTime(_startTime);
        int end = ParseTime(_endTime);
        if (start < 0 || end < 0) return false;
        int m = now.GetHour() * 60 + now.GetMinute();
        // equal times mean all day, as for schedules
        if (start == end) return true;
        if (start < end) return m >= start && m < end;
        return m >= start || m < end;
    }
    }
    return false;
}
