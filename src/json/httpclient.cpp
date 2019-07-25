/*
 * Copyright (C) 2011 by Tommi Meakitalo
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

#include "cxxtools/json/httpclient.h"
#include "cxxtools/net/uri.h"
#include "cxxtools/net/addrinfo.h"
#include "httpclientimpl.h"
#include <stdexcept>

namespace cxxtools
{

namespace json
{

HttpClientImpl* HttpClient::getImpl()
{
    if (_impl == 0)
    {
        _impl = new HttpClientImpl();
        _impl->addRef();
    }

    return _impl;
}

HttpClient::HttpClient(const HttpClient& other)
: _impl(other._impl)
{
    if (_impl)
        _impl->addRef();
}

HttpClient& HttpClient::operator= (const HttpClient& other)
{
    if (_impl == other._impl)
        return *this;

    if (_impl && _impl->release() <= 0)
        delete _impl;

    _impl = other._impl;

    if (_impl)
        _impl->addRef();

    return *this;
}

HttpClient::~HttpClient()
{
    if (_impl && _impl->release() <= 0)
        delete _impl;
}

void HttpClient::prepareConnect(const net::AddrInfo& addrinfo, const std::string& url, bool ssl)
{
    getImpl()->prepareConnect(addrinfo, url, ssl);
}

void HttpClient::prepareConnect(const net::AddrInfo& addrinfo, const std::string& url, const std::string& sslCertificate)
{
    getImpl()->prepareConnect(addrinfo, url, sslCertificate);
}

void HttpClient::prepareConnect(const std::string& host, unsigned short int port, const std::string& url, bool ssl)
{
    prepareConnect(net::AddrInfo(host, port), url, ssl);
}

void HttpClient::prepareConnect(const std::string& host, unsigned short int port, const std::string& url, const std::string& sslCertificate)
{
    prepareConnect(net::AddrInfo(host, port), url, sslCertificate);
}

void HttpClient::prepareConnect(const net::Uri& uri)
{
#ifdef WITH_SSL
    if (uri.protocol() != "http" && uri.protocol() != "https")
        throw std::runtime_error("only protocols \"http\" and \"https\" are supported by http json rpc client");
    prepareConnect(net::AddrInfo(uri.host(), uri.port()), uri.protocol() == "https", uri.path());
#else
    if (uri.protocol() != "http")
        throw std::runtime_error("only protocol \"http\" is supported by http json rpc client");
    prepareConnect(net::AddrInfo(uri.host(), uri.port()), uri.path());
#endif
}

void HttpClient::prepareConnect(const net::Uri& uri, const std::string& sslCertificate)
{
#ifdef WITH_SSL
    if (uri.protocol() != "http" && uri.protocol() != "https")
        throw std::runtime_error("only protocols http and https are supported by http json client");
    prepareConnect(net::AddrInfo(uri.host(), uri.port()), uri.path(), sslCertificate);
#else
    if (uri.protocol() != "http")
        throw std::runtime_error("only protocol \"http\" is supported by json http client");
    prepareConnect(net::AddrInfo(uri.host(), uri.port()), uri.path());
#endif
}

void HttpClient::connect()
{
    getImpl()->connect();
}

void HttpClient::url(const std::string& url)
{
    getImpl()->url(url);
}

void HttpClient::auth(const std::string& username, const std::string& password)
{
    getImpl()->auth(username, password);
}

void HttpClient::clearAuth()
{
    getImpl()->clearAuth();
}

void HttpClient::setSelector(SelectorBase* selector)
{
    getImpl()->setSelector(selector);
}

void HttpClient::setSelector(SelectorBase& selector)
{
    getImpl()->setSelector(&selector);
}

void HttpClient::setSslVerify(int level, const std::string& ca)
{
    getImpl()->setSslVerify(level, ca);
}

void HttpClient::beginCall(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc)
{
    _impl->beginCall(r, method, argv, argc);
}

void HttpClient::endCall()
{
    _impl->endCall();
}

void HttpClient::call(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc)
{
    _impl->call(r, method, argv, argc);
}

Milliseconds HttpClient::timeout() const
{
    return getImpl()->timeout();
}

void HttpClient::timeout(Milliseconds t)
{
    getImpl()->timeout(t);
}

Milliseconds HttpClient::connectTimeout() const
{
    return getImpl()->connectTimeout();
}

void HttpClient::connectTimeout(Milliseconds t)
{
    getImpl()->connectTimeout(t);
}

const std::string& HttpClient::url() const
{
    return getImpl()->url();
}

const IRemoteProcedure* HttpClient::activeProcedure() const
{
    return _impl == 0 ? 0 : _impl->activeProcedure();
}

void HttpClient::cancel()
{
    if (_impl)
        _impl->cancel();
}

void HttpClient::wait(Milliseconds msecs)
{
    _impl->wait(msecs);
}

}

}
