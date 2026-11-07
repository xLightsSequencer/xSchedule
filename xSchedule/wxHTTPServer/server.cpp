// Copyright (c) 2014 framerik <framerik@gmail.com>
// All rights reserved
//
// This library is dual-licensed: you can redistribute it and/or modify it under the terms
// of the GNU General Public License version 2 as published by the Free Software Foundation.
// For the terms of this license, see <http://www.gnu.org/licenses/>.
//
// You are free to use this library under the terms of the GNU General Public License, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// If you find this project useful and wish to donate you can do with an
// Amazon Gift Cards - E-mail Delivery https://www.amazon.it/gp/product/B005VG4G3U/gcrnsts

#include "wxhttpserver.h"
#include "spdlog/spdlog.h"

#include <vector>

//#define DETAILED_LOGGING

#define SERVER_ID	100
#define SOCKET_ID	101

#include <wx/arrimpl.cpp>
//WX_DEFINE_EXPORTED_OBJARRAY(HeadersCollection);
WX_DEFINE_OBJARRAY(HeadersCollection)

BEGIN_EVENT_TABLE(HttpServer, wxEvtHandler)
	EVT_SOCKET(SERVER_ID, HttpServer::OnServerEvent)
	EVT_SOCKET(SOCKET_ID, HttpServer::OnSocketEvent)
END_EVENT_TABLE()

HttpServer::HttpServer() :
	_server(NULL)
{
	_sweep.SetOwner(this);
	Bind(wxEVT_TIMER, &HttpServer::OnSweep, this, _sweep.GetId());
}

HttpServer::~HttpServer()
{
    Stop();
}

bool HttpServer::Start(const HttpContext &context)
{
    
    wxASSERT(!_server || (_server && _server->IsClosed()));

	if (_server && _server->IsConnected())
		Stop();

	_context = context;

	// Create the address - defaults to localhost:0 initially
	_address.Service(_context.Port);

	wxLogMessage(_("starting server on %s:%u..."), _address.IPAddress(), _address.Service());
    spdlog::info("starting server on {}:{}...", _address.IPAddress().ToStdString(), _address.Service());

	// Create the socket
	_server = new wxSocketServer(_address, wxSOCKET_REUSEADDR);

	// We use IsOk() here to see if the server is really listening
    if (!_server->IsOk())
    {
        wxLogError(_("unable to start the server on the specified port"));
        spdlog::error("unable to start the server on the specified port");
    }
	else
	{
		IPaddress address;
        if (!_server->GetLocal(address))
        {
            wxLogError(_("unable to retrieve the address to which you are connected"));
            spdlog::error("unable to retrieve the address to which you are connected");
        }
        else
        {
            wxLogMessage(_("server running on %s:%u"), address.IPAddress(), address.Service());
            spdlog::info("server running on {}:{}", address.IPAddress().ToStdString(), address.Service());
        }

		// Setup the event handler and subscribe to connection events
		_server->SetEventHandler(*this, SERVER_ID);
		_server->SetNotify(wxSOCKET_CONNECTION_FLAG);
		_server->Notify(true);
		_sweep.Start(5000);
	}

	return _server->IsOk();
}

bool HttpServer::Stop()
{
    
    
    if (!_server) return false;

    _sweep.Stop();

    // close all open connections
    std::vector<wxSocketBase *> sockets;
    for (auto it = _connections.begin(); it != _connections.end(); ++it)
        sockets.push_back(it->first);
    for (auto socket : sockets)
        DropConnection(socket);

    _server->Notify(false);
	_server->Close();
	delete _server;
    _server = nullptr;

	wxLogMessage(_("closed server on %s:%u"), _address.IPAddress(), _address.Service());
    spdlog::debug("closed server on {}:{}", _address.IPAddress().ToStdString(), _address.Service());

	return true;
}

void HttpServer::DropConnection(wxSocketBase *socket)
{
    auto it = _connections.find(socket);
    if (it == _connections.end())
        return;
    HttpConnection *connection = it->second;
    _connections.erase(it);
    delete connection;

    // Destroy() rather than delete: wxSocket events can still be on their way to us
    socket->Notify(false);
    socket->Destroy();
}

void HttpServer::OnSweep(wxTimerEvent &event)
{
    wxLongLong now = wxGetUTCTimeMillis();
    std::vector<wxSocketBase *> stale;
    for (auto it = _connections.begin(); it != _connections.end(); ++it)
    {
        if (it->second->IsStale(now))
            stale.push_back(it->first);
    }
    for (auto socket : stale)
    {
        spdlog::debug("Web server dropping a connection that is failed, closed, idle or not reading.");
        DropConnection(socket);
    }
}

// Added for xLights
bool HttpServer::IsConnectionValid(HttpConnection* connection) const
{
    for (const auto& it : _connections) {
        if (it.second == connection) {
            return true;
        }
    }

    return false;
}

void HttpServer::OnServerEvent(wxSocketEvent& event)
{
#ifdef DETAILED_LOGGING
    
#endif
    wxStopWatch sw;

    switch (event.GetSocketEvent())
    {
    case wxSOCKET_CONNECTION:
#ifdef DETAILED_LOGGING
        spdlog::info("OnServerEvent: wxSOCKET_CONNECTION");
#endif
        break;
    default:
#ifdef DETAILED_LOGGING
        spdlog::info("OnServerEvent: unexpected event {}", (int)event.GetSocketEvent());
#endif
        break;
    }

    // Accept new connection if there is one in the pending connections queue, else exit. We use Accept(false) for
    // non-blocking accept (although if we got here, there should ALWAYS be a pending connection).
    wxSocketBase *socket = _server->Accept(false);

    if (socket)
    {
#ifdef DETAILED_LOGGING
        spdlog::info("created socket client (socket {})", socket->GetSocket());
#endif

        if (_connections.size() >= 200)
        {
            spdlog::warn("Web server has too many connections; refusing a new one.");
            socket->Destroy();
            return;
        }

        _connections[socket] = new HttpConnection(this, socket);

        socket->SetEventHandler(*this, SOCKET_ID);
        socket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
        socket->Notify(true);
    }
    else
    {
#ifdef DETAILED_LOGGING
        spdlog::info("error, impossible to accept a new connection");
#endif
    }

#ifdef DETAILED_LOGGING
    spdlog::info("OnServerEvent Time {}.", sw.Time());
#endif
}

void HttpServer::OnSocketEvent(wxSocketEvent& event)
{
#ifdef DETAILED_LOGGING
    
#endif
    wxStopWatch sw;

	switch(event.GetSocketEvent())
	{
	case wxSOCKET_INPUT:
#ifdef DETAILED_LOGGING
        spdlog::info("OnSocketEvent: wxSOCKET_INPUT");
#endif
		break;
	case wxSOCKET_LOST:
#ifdef DETAILED_LOGGING
        spdlog::info("OnSocketEvent: wxSOCKET_LOST");
#endif
		break;
	default:
#ifdef DETAILED_LOGGING
        spdlog::info("OnServerEvent: unexpected event {}", event.GetSocketEvent());
#endif
		break;
	}

	wxSocketBase *socket = event.GetSocket();

	auto found = _connections.find(socket);
	if (found == _connections.end())
		return; // already dropped
	HttpConnection *connection = found->second;

	// Now we process the event
	switch(event.GetSocketEvent())
	{
	case wxSOCKET_INPUT:
		{
			// We disable input events, so that the test doesn't trigger wxSocketEvent again.
			socket->SetNotify(wxSOCKET_LOST_FLAG);
			socket->SetFlags(wxSOCKET_NOWAIT);

			connection->HandleRequest();

			// Enable input events again.
			connection->UpdateNotify();
			break;
		}
	case wxSOCKET_OUTPUT:
		connection->FlushOutbound();
		break;
	case wxSOCKET_LOST:
		DropConnection(socket);
		break;
    default:
        break;
	}

#ifdef DETAILED_LOGGING
    spdlog::info("OnSocketEvent Time {}.", sw.Time());
#endif
}
