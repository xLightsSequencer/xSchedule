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

#include <future>
#include <string>

#include <wx/file.h>

#include "../../xlights/src-core/media/AudioManager.h"
#include "../Diagnostics.h"

// Decodes an audio file on another thread. AudioManager decodes the whole file when it is created, and doing that
// on the main thread, which also sends the light frames, held the show up at every song change.
class AudioPreload
{
    std::future<AudioManager*> _future;
    std::string _file;
    std::string _device;

public:
    AudioPreload() = default;
    AudioPreload(const AudioPreload&) = delete;
    AudioPreload& operator=(const AudioPreload&) = delete;
    ~AudioPreload() { Discard(); }

    void Start(const std::string& file, const std::string& device)
    {
        if (_future.valid() && file == _file && device == _device) return;
        Discard();
        if (file.empty() || !wxFile::Exists(file)) return;
        _file = file;
        _device = device;
        _future = std::async(std::launch::async, [file, device]() {
            Diagnostics::ThreadName name("audio preload");
            return new AudioManager(file, -1, device);
        });
    }

    // the preloaded audio when it is for this file, waiting for it if it isn't finished, otherwise a new one
    AudioManager* TakeOrCreate(const std::string& file, const std::string& device)
    {
        if (_future.valid()) {
            AudioManager* am = _future.get();
            if (file == _file && device == _device) return am;
            delete am;
        }
        return new AudioManager(file, -1, device);
    }

    void Discard()
    {
        if (_future.valid()) delete _future.get();
    }
};
