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

#include "PlayListItem.h"

class wxXmlNode;
class wxDateTime;

// A step holding a branch is never played: when the playlist reaches it, the branch picks the step that plays instead.
class PlayListItemBranch : public PlayListItem
{
public:
    enum class Condition { TimeOfDay, Loop, Always };

protected:
    Condition _condition = Condition::TimeOfDay;
    std::string _startTime = "21:00";
    std::string _endTime = "00:00";
    int _loopEvery = 2;
    int _loopFrom = 2;
    std::string _trueStep;
    std::string _truePlayList; // when set, a true result plays this playlist and then carries on
    std::string _falseStep;    // empty carries on with the step after the branch

public:
    PlayListItemBranch(wxXmlNode* node);
    PlayListItemBranch();
    virtual ~PlayListItemBranch() {};
    virtual PlayListItem* Copy(const bool isClone) const override;

    Condition GetCondition() const { return _condition; }
    void SetCondition(Condition condition) { if (_condition != condition) { _condition = condition; _changeCount++; } }
    std::string GetStartTime() const { return _startTime; }
    void SetStartTime(const std::string& time) { if (_startTime != time) { _startTime = time; _changeCount++; } }
    std::string GetEndTime() const { return _endTime; }
    void SetEndTime(const std::string& time) { if (_endTime != time) { _endTime = time; _changeCount++; } }
    int GetLoopEvery() const { return _loopEvery; }
    void SetLoopEvery(int every) { if (_loopEvery != every) { _loopEvery = every; _changeCount++; } }
    int GetLoopFrom() const { return _loopFrom; }
    void SetLoopFrom(int from) { if (_loopFrom != from) { _loopFrom = from; _changeCount++; } }
    std::string GetTrueStep() const { return _trueStep; }
    void SetTrueStep(const std::string& step) { if (_trueStep != step) { _trueStep = step; _changeCount++; } }
    std::string GetTruePlayList() const { return _truePlayList; }
    void SetTruePlayList(const std::string& playlist) { if (_truePlayList != playlist) { _truePlayList = playlist; _changeCount++; } }
    std::string GetFalseStep() const { return _falseStep; }
    void SetFalseStep(const std::string& step) { if (_falseStep != step) { _falseStep = step; _changeCount++; } }

    // loop is the playlist's pass, counting from 1
    bool IsTrue(const wxDateTime& now, int loop) const;
    // minutes after midnight for "HH:MM", or -1
    static int ParseTime(const std::string& time);

    virtual size_t GetDurationMS() const override { return 0; }
    virtual std::string GetTitle() const override;
    virtual wxXmlNode* Save() override;
    void Load(wxXmlNode* node) override;
    virtual void Frame(uint8_t* buffer, size_t size, size_t ms, size_t framems, bool outputframe) override {}
    virtual void Configure(wxNotebook* notebook) override;
};
