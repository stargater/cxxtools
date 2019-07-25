/*
 * Copyright (C) 2011 Tommi Maekitalo
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

#include "rpcclientimpl.h"
#include <cxxtools/log.h>
#include <cxxtools/remoteprocedure.h>
#include <cxxtools/jsonformatter.h>
#include <cxxtools/ioerror.h>
#include <cxxtools/clock.h>
#include <stdexcept>

log_define("cxxtools.json.rpcclient.impl")

namespace cxxtools
{
namespace json
{

RpcClientImpl::RpcClientImpl()
    : _stream(_socket, 8192, true),
      _ssl(false),
      _sslVerifyLevel(0),
      _exceptionPending(false),
      _proc(0),
      _count(0),
      _timeout(Selectable::WaitInfinite),
      _connectTimeoutSet(false),
      _connectTimeout(Selectable::WaitInfinite)
{
    cxxtools::connect(_socket.connected, *this, &RpcClientImpl::onConnect);
    cxxtools::connect(_socket.sslConnected, *this, &RpcClientImpl::onSslConnect);
    cxxtools::connect(_stream.buffer().outputReady, *this, &RpcClientImpl::onOutput);
    cxxtools::connect(_stream.buffer().inputReady, *this, &RpcClientImpl::onInput);
}

void RpcClientImpl::connect()
{
    _socket.setTimeout(_connectTimeout);
    _socket.close();
    _socket.connect(_addrInfo);
    if (_ssl)
    {
        if (!_sslCertificate.empty())
            _socket.loadSslCertificateFile(_sslCertificate);
        _socket.setSslVerify(_sslVerifyLevel, _sslCa);
        _socket.sslConnect();
    }
}

void RpcClientImpl::close()
{
    _socket.close();
}

void RpcClientImpl::beginCall(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc)
{
    if (_socket.selector() == 0)
        throw std::logic_error("cannot run async rpc request without a selector");

    if (_proc)
        throw std::logic_error("asyncronous request already running");

    _proc = &method;

    prepareRequest(method.name(), argv, argc);

    try
    {
        if (_socket.isConnected())
        {
            try
            {
                _stream.buffer().beginWrite();
            }
            catch (const IOError&)
            {
                log_debug("write failed, connection is not active any more");
                _socket.beginConnect(_addrInfo);
            }
        }
        else
        {
            log_debug("not yet connected - do it now");
            _socket.beginConnect(_addrInfo);
        }
    }
    catch (const std::exception& )
    {
        IRemoteProcedure* proc = _proc;
        cancel();

        _exceptionPending = true;
        proc->onFinished();

        if (_exceptionPending)
            throw;
    }

    _scanner.begin(_deserializer, r);
}

void RpcClientImpl::endCall()
{
    _proc = 0;

    if (_exceptionPending)
    {
        _exceptionPending = false;
        throw;
    }
}

void RpcClientImpl::call(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc)
{
    try
    {
        _proc = &method;
        StreamBuffer& sb = _stream.buffer();

        if (_socket.isConnected())
        {
            log_debug("socket is connected");

            try
            {
                prepareRequest(_proc->name(), argv, argc);
                _socket.setTimeout(timeout());
                sb.pubsync();

                // try to read from socket to check if still connected
                // sgetc fills the input buffer but do not consume the character
                int ch = sb.sgetc();
                if (ch == StreamBuffer::traits_type::eof())
                {
                    log_debug("reading failed");
                    _socket.close();
                }
            }
            catch (const std::exception& e)
            {
                log_debug("request failed: " << e.what());
                _socket.close();
            }
        }

        if (!_socket.isConnected())
        {
            log_debug("socket is not connected");
            _socket.setTimeout(_connectTimeout);
            _socket.connect(_addrInfo);
            if (_ssl)
            {
                if (!_sslCertificate.empty())
                    _socket.loadSslCertificateFile(_sslCertificate);
                _socket.setSslVerify(_sslVerifyLevel, _sslCa);
                _socket.sslConnect();
            }

            prepareRequest(_proc->name(), argv, argc);
            _socket.setTimeout(timeout());
            sb.pubsync();
        }

        _scanner.begin(_deserializer, r);

        while (true)
        {
            int ch = sb.sbumpc();
            if (ch == StreamBuffer::traits_type::eof())
            {
                cancel();
                throw std::runtime_error("reading result failed");
            }

            if (_deserializer.advance(ch))
            {
                _proc = 0;
                _scanner.finalizeReply();
                break;
            }
        }
    }
    catch (const RemoteException&)
    {
        _proc = 0;
        throw;
    }
    catch (const std::exception& e)
    {
        cancel();
        throw;
    }
}

void RpcClientImpl::cancel()
{
    _socket.close();
    _stream.clear();
    _stream.buffer().discard();
    _proc = 0;
}

void RpcClientImpl::wait(Timespan timeout)
{
    if (_socket.selector() == 0)
        throw std::logic_error("cannot run async rpc request without a selector");

    Clock clock;
    if (timeout >= Timespan(0))
        clock.start();

    Timespan remaining = timeout;

    while (activeProcedure() != 0)
    {
        if (_socket.selector()->wait(remaining) == false)
            throw IOTimeout();

        if (timeout >= Timespan(0))
        {
            remaining = timeout - clock.stop();
            if (remaining < Timespan(0))
                remaining = Timespan(0);
        }
    }
}

void RpcClientImpl::prepareRequest(const String& name, IDecomposer** argv, unsigned argc)
{
    JsonFormatter formatter;

    formatter.begin(_stream);

    formatter.beginObject(std::string(), std::string());

    formatter.addValueStdString("jsonrpc", std::string(), "2.0");
    formatter.addValueString("method", std::string(), String(_prefix) + name);
    formatter.addValueInt("id", "int", ++_count);

    formatter.beginArray("params", std::string());

    for(unsigned n = 0; n < argc; ++n)
    {
        argv[n]->format(formatter);
    }

    formatter.finishArray();

    formatter.finishObject();

    formatter.finish();
}

void RpcClientImpl::onConnect(net::TcpSocket& socket)
{
    try
    {
        log_trace("onConnect");

        socket.endConnect();

        _exceptionPending = false;
        if (_ssl)
        {
            if (!_sslCertificate.empty())
                _socket.loadSslCertificateFile(_sslCertificate);
            _socket.setSslVerify(_sslVerifyLevel, _sslCa);
            socket.beginSslConnect();
            return;
        }

        _stream.buffer().beginWrite();
    }
    catch (const std::exception& )
    {
        IRemoteProcedure* proc = _proc;
        cancel();

        if (!proc)
            throw;

        _exceptionPending = true;
        proc->onFinished();

        if (_exceptionPending)
            throw;
    }
}

void RpcClientImpl::onSslConnect(net::TcpSocket& socket)
{
    try
    {
        log_trace("onSslConnect");

        _exceptionPending = false;
        socket.endSslConnect();

        _stream.buffer().beginWrite();
    }
    catch (const std::exception& )
    {
        IRemoteProcedure* proc = _proc;
        cancel();

        if (!proc)
            throw;

        _exceptionPending = true;
        proc->onFinished();

        if (_exceptionPending)
            throw;
    }
}

void RpcClientImpl::onOutput(StreamBuffer& sb)
{
    try
    {
        _exceptionPending = false;
        sb.endWrite();
        if (sb.out_avail() > 0)
            sb.beginWrite();
        else
            sb.beginRead();
    }
    catch (const std::exception&)
    {
        IRemoteProcedure* proc = _proc;
        cancel();

        if (!proc)
            throw;

        _exceptionPending = true;
        proc->onFinished();

        if (_exceptionPending)
            throw;
    }
}

void RpcClientImpl::onInput(StreamBuffer& sb)
{
    try
    {
        _exceptionPending = false;
        sb.endRead();

        if (sb.device()->eof())
            throw IOError("end of input");

        while (_stream.buffer().in_avail())
        {
            char ch = StreamBuffer::traits_type::to_char_type(_stream.buffer().sbumpc());
            if (_deserializer.advance(ch))
            {
                _scanner.finalizeReply();
                IRemoteProcedure* proc = _proc;
                _proc = 0;
                proc->onFinished();
                return;
            }
        }

        if (!_stream)
        {
            close();
            throw std::runtime_error("reading result failed");
        }

        sb.beginRead();
    }
    catch (const std::exception&)
    {
        IRemoteProcedure* proc = _proc;
        cancel();

        if (!proc)
            throw;

        _exceptionPending = true;
        proc->onFinished();

        if (_exceptionPending)
            throw;
    }
}

}
}
