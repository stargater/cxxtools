/*
 * Copyright (C) 2009 by Marc Boris Duerner, Tommi Maekitalo
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "socket.h"
#include "serverimpl.h"
#include <cxxtools/log.h>
#include <cassert>
#include "config.h"

log_define("cxxtools.http.socket")

namespace cxxtools
{

namespace http
{

void Socket::ParseEvent::onMethod(const std::string& method)
{
    _request.method(method);
}

void Socket::ParseEvent::onUrl(const std::string& url)
{
    _request.url(url);
}

void Socket::ParseEvent::onUrlParam(const std::string& q)
{
    _request.qparams(q);
}

Socket::Socket(ServerImpl& server, net::TcpServer& tcpServer, const std::string& certificateFile, const std::string& privateKeyFile, int sslVerifyLevel, const std::string& sslCa)
    : inputSlot(slot(*this, &Socket::onInput)),
      _tcpServer(tcpServer),
      _certificateFile(certificateFile),
      _privateKeyFile(privateKeyFile),
      _server(server),
      _parseEvent(_request),
      _parser(_parseEvent, false),
      _responder(0),
      _sslVerifyLevel(sslVerifyLevel),
      _sslCa(sslCa),
      _accepted(false)
{
    _stream.attachDevice(*this);
    cxxtools::connect(IODevice::inputReady, *this, &Socket::onIODeviceInput);
    cxxtools::connect(_stream.buffer().outputReady, *this, &Socket::onOutput);
    cxxtools::connect(_timer.timeout, *this, &Socket::onTimeout);
    cxxtools::connect(acceptSslCertificate, *this, &Socket::onAcceptSslCertificate);
}

Socket::Socket(Socket& socket)
    : net::TcpSocket(),
      Connectable(*this),
      inputSlot(slot(*this, &Socket::onInput)),
      _tcpServer(socket._tcpServer),
      _certificateFile(socket._certificateFile),
      _privateKeyFile(socket._privateKeyFile),
      _server(socket._server),
      _parseEvent(_request),
      _parser(_parseEvent, false),
      _responder(0),
      _sslVerifyLevel(socket._sslVerifyLevel),
      _sslCa(socket._sslCa),
      _accepted(false)
{
    _stream.attachDevice(*this);
    cxxtools::connect(IODevice::inputReady, *this, &Socket::onIODeviceInput);
    cxxtools::connect(_stream.buffer().outputReady, *this, &Socket::onOutput);
    cxxtools::connect(_timer.timeout, *this, &Socket::onTimeout);
    cxxtools::connect(acceptSslCertificate, *this, &Socket::onAcceptSslCertificate);
}

Socket::~Socket()
{
    if (_responder)
        _responder->release();
}

void Socket::accept()
{
    net::TcpSocket::accept(_tcpServer, net::TcpSocket::DEFER_ACCEPT);

    if (!_certificateFile.empty())
    {
        loadSslCertificateFile(_certificateFile, _privateKeyFile);
        setSslVerify(_sslVerifyLevel, _sslCa);
        beginSslAccept();
    }
}

void Socket::postAccept()
{
    log_trace("post accept");
    if (!_certificateFile.empty())
    {
        cxxtools::Timespan t = getTimeout();
        setTimeout(cxxtools::Seconds(10));
        endSslAccept();
        setTimeout(t);
    }

    _accepted = true;

    _stream.buffer().beginRead();

    log_debug("accepted");

    _timer.start(_server.readTimeout());
}

void Socket::setSelector(SelectorBase* s)
{
    s->add(*this);
    s->add(_timer);
}

void Socket::removeSelector()
{
    TcpSocket::setSelector(0);
    _timer.setSelector(0);
}

void Socket::onIODeviceInput(IODevice& /*iodevice*/)
{
    log_debug("onIODeviceInput");
    inputReady(*this);
}

void Socket::onInput(StreamBuffer& sb)
{
    log_debug("onInput");

    sb.endRead();

    if (sb.in_avail() == 0 || sb.device()->eof())
    {
        close();
        return;
    }

    _timer.start(_server.readTimeout());
    if ( _responder == 0 )
    {
        _parser.advance(sb);

        if (_parser.fail())
        {
            _responder = _server.getDefaultResponder(_request);
            _responder->replyError(_reply.bodyStream(), _request, _reply,
                std::runtime_error("invalid http header"));
            _responder->release();
            _responder = 0;

            sendReply();

            onOutput(sb);
            return;
        }

        if (_parser.end())
        {
            log_info("request " << _request.method() << ' ' << _request.header().query()
                << " from client " << getPeerAddr());
            _responder = _server.getResponder(_request);
            try
            {
                _responder->beginRequest(*this, _stream, _request);
            }
            catch (const std::exception& e)
            {
                _reply.setHeader("Connection", "close");
                _responder->replyError(_reply.bodyStream(), _request, _reply, e);
                _responder->release();
                _responder = 0;
                sendReply();

                onOutput(sb);
                return;
            }

            _contentLength = _request.header().contentLength();
            log_debug("content length of request is " << _contentLength);
            if (_contentLength == 0)
            {
                _timer.stop();
                doReply();
                return;
            }

        }
        else
        {
            sb.beginRead();
        }
    }

    if (_responder)
    {
        if (sb.in_avail() > 0)
        {
            try
            {
                std::size_t s = _responder->readBody(_stream);
                assert(s > 0);
                _contentLength -= s;
            }
            catch (const std::exception& e)
            {
                _reply.setHeader("Connection", "close");
                _responder->replyError(_reply.bodyStream(), _request, _reply, e);
                _responder->release();
                _responder = 0;
                sendReply();

                onOutput(sb);
                return;
            }
        }

        if (_contentLength <= 0)
        {
            _timer.stop();
            doReply();
        }
        else
        {
            sb.beginRead();
        }
    }
}

bool Socket::doReply()
{
    log_trace("http::Socket::doReply");
    try
    {
        _responder->reply(_reply.bodyStream(), _request, _reply);
    }
    catch (const std::exception& e)
    {
        log_warn("responder reported error: " << e.what());
        _reply.clear();
        _responder->replyError(_reply.bodyStream(), _request, _reply, e);
    }

    _responder->release();
    _responder = 0;

    sendReply();

    return onOutput(_stream.buffer());
}

bool Socket::onOutput(StreamBuffer& sb)
{
    log_trace("onOutput");

    log_debug("send data to " << getPeerAddr());

    try
    {
        sb.endWrite();

        if ( sb.out_avail() )
        {
            sb.beginWrite();
            _timer.start(_server.writeTimeout());
        }
        else
        {
            bool keepAlive = _request.header().keepAlive()
                          && _reply.header().keepAlive();

            if (keepAlive)
            {
                log_debug("do keep alive");
                _timer.start(_server.keepAliveTimeout());
                _request.clear();
                _reply.clear();
                _parser.reset(false);
                if (sb.in_avail())
                    onInput(sb);
                else
                    _stream.buffer().beginRead();
            }
            else
            {
                log_debug("don't do keep alive");
                close();
                return false;
            }
        }
    }
    catch (const std::exception& e)
    {
        log_warn("exception occured when processing request: " << e.what());
        close();
        timeout(*this);
        return false;
    }

    return true;
}

void Socket::onTimeout()
{
    log_debug("timeout");
    timeout(*this);
}

void Socket::sendReply()
{
    const char* contentLength = "Content-Length";
    const char* server = "Server";
    const char* connection = "Connection";
    const char* date = "Date";

    log_info("request " << _request.method() << ' ' << _request.header().query()
        << " ready, returncode " << _reply.httpReturnCode() << ' '
        << _reply.httpReturnText());

    _stream << "HTTP/"
        << _reply.header().httpVersionMajor() << '.'
        << _reply.header().httpVersionMinor() << ' '
        << _reply.header().httpReturnCode() << ' '
        << _reply.header().httpReturnText() << "\r\n";

    for (ReplyHeader::const_iterator it = _reply.header().begin();
        it != _reply.header().end(); ++it)
    {
        _stream << it->first << ": " << it->second << "\r\n";
    }

    if (!_reply.header().hasHeader(contentLength))
    {
        _stream << "Content-Length: " << _reply.bodySize() << "\r\n";
    }

    if (!_reply.header().hasHeader(server))
    {
        _stream << "Server: cxxtools-Http-Server " PACKAGE_VERSION "\r\n";
    }

    if (!_reply.header().hasHeader(connection))
    {
        _stream << "Connection: "
                << (_request.header().keepAlive() ? "keep-alive" : "close")
                << "\r\n";
    }

    if (!_reply.header().hasHeader(date))
    {
        char buffer[50];
        _stream << "Date: " << MessageHeader::htdateCurrent(buffer) << "\r\n";
    }

    _stream << "\r\n";

    _reply.sendBody(_stream);

}

bool Socket::onAcceptSslCertificate(const SslCertificate& cert)
{
    return !_server.acceptSslCertificate.isConnected() || _server.acceptSslCertificate(cert);
}

} // namespace http

} // namespace cxxtools
