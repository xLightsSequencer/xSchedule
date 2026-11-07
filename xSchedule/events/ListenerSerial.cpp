/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "ListenerSerial.h"
#include <log.h>
#include "../../xlights/src-core/outputs/serial.h"
#include "../../xlights/src-core/outputs/SerialOutput.h"
#include "ListenerManager.h"

ListenerSerial::ListenerSerial(ListenerManager* listenerManager, const std::string& commPort, const std::string& serialConfig, int baudRate, const std::string& protocol) : ListenerBase(listenerManager, "")
{
    _serial = nullptr;
    _commPort = commPort;
    _serialConfig = serialConfig;
    _baudRate = baudRate;
    _protocol = protocol;
    memset(_buffer, 0x00, sizeof(_buffer));
    _valid = 0;
}

void ListenerSerial::Start()
{
    spdlog::debug("Serial listener starting.");
    _thread = new ListenerThread(this, _localIP);
}

void ListenerSerial::Stop()
{
    if (!_stop)
    {
        spdlog::debug("Serial listener stopping.");
        if (_thread != nullptr)
        {
            _stop = true;
            _thread->Stop();
            _thread->Delete();
            delete _thread;
            _thread = nullptr;
        }
    }
}

void ListenerSerial::StartProcess(const std::string& localIP)
{
    if (_commPort == "Not Connected") return;

    _serial = new SerialPort();

    spdlog::debug("Opening serial port {}. Baud rate = {}. Config = {}.", (const char*)_commPort.c_str(), _baudRate, (const char*)_serialConfig.c_str());

    int errcode = _serial->Open(_commPort, _baudRate, _serialConfig.c_str());
    if (errcode < 0)
    {
        spdlog::warn("Unable to open serial port {}. Error code = {}", (const char*)_commPort.c_str(), errcode);
        delete _serial;
        _serial = nullptr;

        std::string p = "";
        auto ports = SerialOutput::GetAvailableSerialPorts();
        for (auto it = ports.begin(); it != ports.end(); ++it)
        {
            if (p != "") p += ", ";
            p += *it;
        }

        [[maybe_unused]] wxString msg = wxString::Format(_("Error occurred while connecting to %s network on %s (Available Ports %s) \n\n") +
            _("Things to check:\n") +
            _("1. Are all required cables plugged in?\n") +
            _("2. Is there another program running that is accessing the port (like the LOR Control Panel)? If so, then you must close the other program and then restart xLights.\n") +
            _("3. If this is a USB dongle, are the FTDI Virtual COM Port drivers loaded?\n\n") +
            _("Unable to open serial port %s. Error code = %d"),
            (const char *)GetType().c_str(),
            (const char *)GetCommPort().c_str(),
            (const char *)p.c_str(),
            (const char *)_commPort.c_str(),
            errcode);
        //wxMessageBox(msg, _("Communication Error"), wxOK);
    }
    else
    {
        spdlog::debug("    Serial port {} open.", (const char*)_commPort.c_str());
        _isOk = true;
    }
}

void ListenerSerial::StopProcess()
{
    if (_serial != nullptr)
    {
        _serial->Close();
        delete _serial;
        _serial = nullptr;
    }
    _isOk = false;
}

void ListenerSerial::ReopenPort()
{
    _lastReopen = wxGetUTCTimeMillis();
    SerialPort* serial = new SerialPort();
    if (serial->Open(_commPort, _baudRate, _serialConfig.c_str()) < 0) {
        delete serial;
        return;
    }
    spdlog::info("Serial port {} is back and has been reopened.", _commPort);
    _serial = serial;
    _portLost = false;
    _valid = 0;
}

void ListenerSerial::Poll()
{
    if (_serial == nullptr) {
        if (_portLost && wxGetUTCTimeMillis() - _lastReopen > 5000) {
            ReopenPort();
        }
        wxMilliSleep(50);
        return;
    }

    if (_valid >= (int)sizeof(_buffer)) {
        // a full buffer that never made a packet is noise; start again
        _valid = 0;
    }
    int read = _serial->Read((char*)(_buffer + _valid), sizeof(_buffer) - _valid);
    if (_stop) return;
    if (read < 0) {
        // logged once rather than on every pass
        spdlog::warn("Serial port {} stopped responding; it will be reopened when it comes back.", _commPort);
        _serial->Close();
        delete _serial;
        _serial = nullptr;
        _portLost = true;
        _lastReopen = wxGetUTCTimeMillis();
        _valid = 0;
        return;
    }
    if (read == 0) {
        // the port returns straight away when nothing has arrived; without a pause this loop used a whole core
        wxMilliSleep(2);
        return;
    }
    _valid += read;

    if (_protocol == "DMX")
    {
        if (_valid < 5) return;

        int i = 0;
        while (i < _valid && *(_buffer + i) != 0x7E)
        {
            i++;
        }
        memmove(_buffer, (_buffer + i), sizeof(_buffer) - i);
        _valid -= i;

        // At this point the start byte is at the start of the buffer
        if (_valid < 5) return;

        int len = (_buffer[2] << 8) + _buffer[3];

        if (_valid < len + 5 + 1) return;

        wxASSERT(_buffer[0] == 0x7E);
        wxASSERT(_buffer[1] == 0x06);
        wxASSERT(_buffer[4] == 0x00);
        wxASSERT(_buffer[5 + len] == 0xE7);

        _listenerManager->ProcessPacket(GetType(), _commPort, &_buffer[5], len, 0);
        memmove(_buffer, _buffer + 5 + len + 1, sizeof(_buffer) -5 - len - 1);
        _valid -= (5 + len + 1);
    }
    else if (_protocol == "OpenDMX")
    {
        if (_valid < 1) return;

        int i = 0;
        while (i < _valid && *(_buffer + i) != 0x00)
        {
            i++;
        }
        memmove(_buffer, (_buffer + i), sizeof(_buffer) - i);
        _valid -= i;

        // At this point the start byte is at the start of the buffer
        if (_valid < 1) return;

        wxASSERT(_buffer[0] == 0x00);

        int len = std::min(512, _valid - 1);
        _listenerManager->ProcessPacket(GetType(), _commPort, &_buffer[1], len, 0);
        memmove(_buffer, _buffer + len + 1, sizeof(_buffer) - len - 1);
        _valid -= (len + 1);
    }
}
