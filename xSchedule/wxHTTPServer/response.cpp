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

#include <wx/file.h>
#include "wxhttpserver.h"

#include <wx/filename.h>
#include <wx/mimetype.h>
#include <wx/wfstream.h>

HttpResponse::HttpResponse(HttpConnection &connection, HttpRequest &request, HttpStatus::HttpStatusCode code /*= HttpStatus::NotFound*/) :
	_connection(connection),
	_request(request),
	_status(code),
	_version("HTTP/1.1")
{
	MakeFromCode(code);
}

HttpResponse::HttpResponse(HttpConnection &connection, HttpRequest &request, const wxString &fileName, const wxString &charset /*= wxEmptyString*/) :
	_connection(connection),
	_request(request),
	_status(HttpStatus::OK),
	_version("HTTP/1.1")
{
	MakeFromFile(fileName, charset);
}

static const wxString& ServerName()
{
	static const wxString name = wxString::Format(SERVER_NAME "(" SERVER_VERSION ") (%s)", wxGetOsDescription());
	return name;
}

void HttpResponse::MakeFromFile(const wxString &fileName, const wxString &charset /*= wxEmptyString*/)
{
	if (!wxFileName::FileExists(fileName))
		MakeFromCode(HttpStatus::NotFound);
	else
	{
		// with a modification date the browser asks "only if changed" on later loads and gets a 304 instead of the file
		wxString modified = wxFileName(fileName).GetModificationTime().Format("%a, %d %b %Y %H:%M:%S GMT", wxDateTime::GMT0);
		if (!_request["If-Modified-Since"].IsEmpty() && _request["If-Modified-Since"] == modified)
		{
			_status.SetCode(HttpStatus::NotModified);
			_headers.Add("Date", wxDateTime::Now().Format("%a, %d %b %Y %H:%M:%S GMT", wxDateTime::GMT0));
			_headers.Add("Last-Modified", modified);
			_headers.Add("Cache-Control", "private, s-maxage=0, max-age=0, must-revalidate");
			return;
		}

		wxFile file(fileName);

		if (!file.IsOpened())
			MakeFromCode(HttpStatus::BadRequest);
		else
		{
			size_t length = (size_t)file.Length();
			if (length > 0)
			{
				ssize_t read = file.Read(_content.GetWriteBuf(length), length);
				_content.UngetWriteBuf(read < 0 ? 0 : (size_t)read);
			}

			wxFileType *fileType = wxTheMimeTypesManager->GetFileTypeFromExtension(wxFileName(fileName).GetExt());
			wxString    mimeType("text/plain");

			if (fileType)
			{
				fileType->GetMimeType(&mimeType);
				delete fileType;
			}

			_headers.Add("Date", wxDateTime::Now().Format("%a, %d %b %Y %H:%M:%S GMT", wxDateTime::GMT0));
			_headers.Add("Last-Modified", modified);
			_headers.Add("Server", ServerName());
			_headers.Add("Vary", "Accept-Encoding,Cookie");
			_headers.Add("Cache-Control", "private, s-maxage=0, max-age=0, must-revalidate");
			_headers.Add("Content-Type", mimeType + (charset.IsEmpty() ? "" : ("; " + charset)));
			_headers.Add("Content-Length", wxString::Format("%zu", _content.GetDataLen()));
			_headers.Add("Content-Language", "it");
			_headers.Add("Connection", "close");
		}
	}

}

void HttpResponse::MakeFromText(const wxString &text, const wxString &mimeType /*= "text/html"*/)
{
	wxScopedCharBuffer buffer = text.ToUTF8();

	_content.AppendData(buffer.data(), buffer.length());

	_headers.Add("Date", wxDateTime::Now().Format("%a, %d %b %Y %H:%M:%S GMT", wxDateTime::GMT0));
	_headers.Add("Server", ServerName());
	_headers.Add("Vary", "Accept-Encoding,Cookie");
	_headers.Add("Cache-Control", "private, s-maxage=0, max-age=0, must-revalidate");
	_headers.Add("Content-Type", mimeType + "; utf-8");
	_headers.Add("Content-Length", wxString::Format("%zu", _content.GetDataLen()));
	_headers.Add("Content-Language", "it");
	_headers.Add("Connection", "close");
}

void HttpResponse::MakeFromCode(HttpStatus::HttpStatusCode code /*= HttpStatus::NotFound*/)
{
	_status.SetCode(code);

	wxString content;

	switch (code)
	{
	case HttpStatus::BadRequest:
		content = _connection.Server()->Context().ErrorPage400;
		break;
	case HttpStatus::NotFound:
		content = _connection.Server()->Context().ErrorPage404;
		break;
    default:
        break;
	}

	if (!content.IsEmpty())
	{
		content.Replace("%CODE%", wxString::Format("%d", _status.Code()));
		content.Replace("%DESCRIPTION%", _status.Description());
		content.Replace("%METHOD%", _request.Method());
		content.Replace("%URI%", _request.URI());
		content.Replace("%VERSION%", _request.Version());

		wxScopedCharBuffer buffer = content.ToUTF8();
		_content.AppendData(buffer.data(), buffer.length());

		_headers.Add("Content-Type", "text/html; utf-8");
		_headers.Add("Content-Length", wxString::Format("%zu", _content.GetDataLen()));
	}
}
