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
#include "sha1.h"

#include <wx/buffer.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <wx/base64.h>
#include <wx/filename.h>

HttpConnection::HttpConnection(HttpServer *server, wxSocketBase *socket) :
	_server(server),
	_socket(socket),
	_isWebSocket(false),
	_message(NULL)
{
	_lastActivity = wxGetUTCTimeMillis();
	if (!_socket->GetPeer(_address))
		wxLogMessage(_("accepted a new connection from <unknown> (socket %d)"), (int)socket->GetSocket());
	else
		wxLogMessage(_("accepted a new connection from %s:%u (socket %d)"), _address.IPAddress(), _address.Service(), (int)socket->GetSocket());
}

HttpConnection::~HttpConnection()
{
	delete _message;
}

// more than this waiting to go means the client isn't reading
#define MAX_OUTBOUND (32 * 1024 * 1024)
// websocket messages from the web pages are small
#define MAX_FRAME (16 * 1024 * 1024)

bool HttpConnection::Queue(const void *data, size_t length)
{
	if (_failed || _socket == nullptr)
		return false;
	if (_outbound.GetDataLen() - _outboundSent + length > MAX_OUTBOUND)
	{
		_failed = true;
		return false;
	}
	_outbound.AppendData(data, length);
	FlushOutbound();
	return !_failed;
}

void HttpConnection::FlushOutbound()
{
	if (_socket == nullptr)
		return;

	_socket->SetFlags(wxSOCKET_NOWAIT);
	while (HasOutbound())
	{
		size_t left = _outbound.GetDataLen() - _outboundSent;
		_socket->Write((const char *)_outbound.GetData() + _outboundSent, (wxUint32)std::min(left, (size_t)(64 * 1024)));
		size_t written = _socket->LastWriteCount();
		_outboundSent += written;
		if (_socket->Error() && _socket->LastError() != wxSOCKET_WOULDBLOCK)
		{
			_failed = true;
			break;
		}
		if (written == 0)
			break;
	}

	if (!HasOutbound())
	{
		_outbound.Clear();
		_outboundSent = 0;
		_outboundSince = 0;
	}
	else if (_outboundSince == 0)
	{
		_outboundSince = wxGetUTCTimeMillis();
	}
	UpdateNotify();
}

void HttpConnection::UpdateNotify()
{
	if (_socket != nullptr)
		_socket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG | (HasOutbound() ? wxSOCKET_OUTPUT_FLAG : 0));
}

bool HttpConnection::IsStale(wxLongLong now) const
{
	if (_failed || _socket == nullptr)
		return true;
	if (_closing && !HasOutbound())
		return true;
	if (HasOutbound() && now - _outboundSince > 20000)
		return true;
	// browsers keep idle connections open; they open a new one when they need it
	return !_isWebSocket && !HasOutbound() && now - _lastActivity > 120000;
}

#define MAX_REQUEST_SIZE (64 * 1024 * 1024)

// The length of the first complete request in the buffer, or 0 while more of it is still to arrive. A POST body
// (the web UI's saved settings, for one) is often split over several packets, and used to be cut off and the rest
// read as a new request. A request with a body but no Content-Length is taken as everything received, as before.
static size_t CompleteRequestLength(const wxMemoryBuffer &buffer)
{
	std::string data((const char *)buffer.GetData(), buffer.GetDataLen());

	size_t headerEnd = data.find("\r\n\r\n");
	size_t separator = 4;
	if (headerEnd == std::string::npos)
	{
		headerEnd = data.find("\n\n");
		separator = 2;
	}
	if (headerEnd == std::string::npos)
		return 0;
	size_t bodyStart = headerEnd + separator;

	std::string headers = data.substr(0, headerEnd);
	std::transform(headers.begin(), headers.end(), headers.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	size_t field = headers.find("\ncontent-length:");
	if (field == std::string::npos)
		return data.size();

	size_t contentLength = std::strtoul(headers.c_str() + field + 16, nullptr, 10);
	if (data.size() < bodyStart + contentLength)
		return 0;
	return bodyStart + contentLength;
}

bool HttpConnection::HandleRequest()
{
	_lastActivity = wxGetUTCTimeMillis();
	_socket->SetFlags(wxSOCKET_NOWAIT);

	wxMemoryBuffer input;
	char           buffer[1024];

	do
	{
		_socket->Read(buffer, sizeof(buffer));
		input.AppendData(buffer, _socket->LastReadCount());
	} while (_socket->LastReadCount() > 0);

	if (_isWebSocket)
	{
		return ParseFrame(input);
	}

	_pending.AppendData(input.GetData(), input.GetDataLen());

	bool result = false;
	while (_pending.GetDataLen() > 0 && !_isWebSocket)
	{
		size_t length = CompleteRequestLength(_pending);
		if (length == 0)
		{
			if (_pending.GetDataLen() > MAX_REQUEST_SIZE)
			{
				wxLogMessage(_("request over %d bytes dropped"), (int)MAX_REQUEST_SIZE);
				_pending.Clear();
			}
			// wait for the rest
			return result;
		}

		wxString content((char *)_pending.GetData(), length);
		wxMemoryBuffer rest;
		rest.AppendData((char *)_pending.GetData() + length, _pending.GetDataLen() - length);
		_pending = rest;

		result = HandleHttpRequest(content);
	}

	if (_isWebSocket && _pending.GetDataLen() > 0)
	{
		// frames sent straight after the handshake
		wxMemoryBuffer frames = _pending;
		_pending.Clear();
		result = ParseFrame(frames);
	}

	return result;
}

bool HttpConnection::HandleHttpRequest(const wxString &content)
{
	{
		HttpRequest request(*this, content);

		if (request.Method() == "GET")
		{
			if (request["Upgrade"].CmpNoCase("websocket") == 0)
			{
				if (!request["Host"].IsEmpty() && !request["Connection"].IsEmpty() &&
					!request["Sec-WebSocket-Key"].IsEmpty() && !request["Sec-WebSocket-Version"].IsEmpty())
					return WebSocketHandshake(request);
				else
                {
                    HttpResponse hr(*this, request, HttpStatus::BadRequest);
					return SendResponse(hr);
                }
			}
			else
			{
				if (_server->_context.RequestHandler)
				{
					if (_server->_context.RequestHandler(*this, request))
						return true;
				}

				wxString fileName(_server->_context.DefaultDirectory);
				fileName += wxFILE_SEP_PATH;

				if (request.URI() == "/")
				{
					for (size_t i = 0; i < _server->_context.DefaultDocuments.Count(); i++)
					{
						if (wxFileName::FileExists(fileName + _server->_context.DefaultDocuments[i]))
						{
							fileName += _server->_context.DefaultDocuments[i];
							break;
						}
					}
				}
				else if (request.URI().Contains(".."))
				{
					HttpResponse hr(*this, request, HttpStatus::NotFound);
					return SendResponse(hr);
				}
				else
					fileName += request.URI().Mid(1);

				HttpResponse response(*this, request, fileName);

				return SendResponse(response);
			}
		}
		else
		{
			// all others requests are routed to custom implementations
			if (_server->_context.RequestHandler)
			{
				if (_server->_context.RequestHandler(*this, request))
					return true;
			}
			// without an answer the client waits, holding the connection open
			HttpResponse hr(*this, request, HttpStatus::MethodNotAllowed);
			return SendResponse(hr);
		}
	}

	return false;
}

bool HttpConnection::SendResponse(HttpResponse &response)
{
	wxMemoryBuffer out;
	wxString row = wxString::Format("%s %d %s\r\n", response.Version(), response.Status().Code(), response.Status().Description());
	out.AppendData(row.ToAscii(), row.Length());
	for (size_t i = 0; i < response.Headers().Count(); i++)
	{
		wxString header = response[i];
		out.AppendData(header.ToAscii(), header.Length());
	}
	out.AppendData("\r\n", 2);
	if (!response._content.IsEmpty())
		out.AppendData(response._content.GetData(), response._content.GetDataLen());

	return Queue(out.GetData(), out.GetDataLen());
}

bool HttpConnection::SendMessage(WebSocketMessage &message)
{
	wxMemoryBuffer header;

	header.AppendByte((wxUint8)0x80 | message._type); // final + type

	wxUint64 length = message._content.GetDataLen();
	if (length > 0xFFFF)
	{
		header.AppendByte(127);
		// network byte order
		for (int i = 7; i >= 0; i--)
			header.AppendByte((wxUint8)((length >> (i * 8)) & 0xFF));
	}
	else if (length > 125)
	{
		header.AppendByte(126);
		header.AppendByte((wxUint8)((length >> 8) & 0xFF));
		header.AppendByte((wxUint8)(length & 0xFF));
	}
	else
	{
		header.AppendByte((wxUint8)length);
	}

	header.AppendData(message._content.GetData(), message._content.GetDataLen());
	return Queue(header.GetData(), header.GetDataLen());
}

bool HttpConnection::Close()
{
	wxASSERT(_socket);

	bool result = _socket->Close();
	_socket = NULL;

	return result;
}

bool HttpConnection::ParseFrame(wxMemoryBuffer &buffer)
{
	// a frame can be split across reads, so only whole frames are handled
	_frames.AppendData(buffer.GetData(), buffer.GetDataLen());

	size_t pos = 0;
	while (!_failed)
	{
		const wxUint8 *data = (const wxUint8 *)_frames.GetData() + pos;
		size_t available = _frames.GetDataLen() - pos;
		if (available < 2)
			break;

		bool final = (data[0] & 0x80) != 0;
		if ((data[0] & 0x70) != 0)
		{
			wxLogWarning("reserved bits must be 0");
			_failed = true;
			return false; // Invalid ws message
		}
		WebSocketMessage::Opcode type = (WebSocketMessage::Opcode)(data[0] & 0x0F);

		size_t start = 2;
		wxUint64 length = data[1] & 0x7F;
		if (length == 126)
		{
			if (available < 4)
				break;
			length = ((wxUint64)data[2] << 8) | data[3];
			start = 4;
		}
		else if (length == 127)
		{
			if (available < 10)
				break;
			length = 0;
			for (int i = 0; i < 8; i++)
				length = (length << 8) | data[2 + i];
			start = 10;
		}
		if (length > MAX_FRAME)
		{
			wxLogWarning("websocket frame too large");
			_failed = true;
			return false;
		}

		wxUint8 mask[4] = { 0 };
		bool masked = (data[1] & 0x80) != 0;
		if (masked)
		{
			if (available < start + 4)
				break;
			memcpy(mask, data + start, 4);
			start += 4;
		}
		if (available < start + length)
			break;

		if (!_message)
			_message = new WebSocketMessage(type);
		for (size_t i = 0; i < length; i++)
			_message->_content.AppendByte(data[start + i] ^ mask[i % 4]);
		pos += start + (size_t)length;

		if (final)
		{
			if (_server->_context.MessageHandler)
				_server->_context.MessageHandler(*this, *_message);

			if (_message->_type == WebSocketMessage::Ping)
			{
				WebSocketMessage wsm(WebSocketMessage::Pong);
				wsm._content = _message->_content;
				SendMessage(wsm);
			}
			else if (_message->_type == WebSocketMessage::Close)
			{
				WebSocketMessage wsm(WebSocketMessage::Close);
				SendMessage(wsm);
				_closing = true;
			}

			delete _message;
			_message = nullptr;
		}
	}

	if (pos > 0)
	{
		wxMemoryBuffer rest;
		rest.AppendData((const char *)_frames.GetData() + pos, _frames.GetDataLen() - pos);
		_frames = rest;
	}

	return !_failed;
}

bool HttpConnection::WebSocketHandshake(HttpRequest &request)
{
	static const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	if (request["Sec-WebSocket-Version"] != "13")
    {
        HttpResponse hr(*this, request, HttpStatus::UpgradeRequired);
		return SendResponse(hr);
    }

	wxString handshake = wxString::Format("%s%s", request["Sec-WebSocket-Key"], magic);

	SHA1Context ctx = { 0 };
	uint8_t     sha[20] = { 0 };

	SHA1Reset(&ctx);
	SHA1Input(&ctx, (unsigned char *)(const char *)handshake.c_str(), handshake.Length());
	SHA1Result(&ctx, sha);

	HttpResponse response(*this, request, HttpStatus::SwitchingProtocols);
	response.AddHeader("Upgrade", "websocket");
	response.AddHeader("Connection", "Upgrade");
	if (!request["Sec-WebSocket-Protocol"].IsEmpty())
		response.AddHeader("Sec-WebSocket-Protocol", request["Sec-WebSocket-Protocol"]);
	response.AddHeader("Sec-WebSocket-Accept", wxBase64Encode(sha, 20));

	SendResponse(response);

	_isWebSocket = true;

	return true;
}
