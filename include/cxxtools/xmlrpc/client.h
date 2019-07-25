/*
 * Copyright (C) 2009 by Dr. Marc Boris Duerner, Tommi Maekitalo
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
#ifndef cxxtools_xmlrpc_Client_h
#define cxxtools_xmlrpc_Client_h

#include <cxxtools/remoteclient.h>
#include <string>

namespace cxxtools {

namespace xmlrpc {

class ClientImpl;

class Client : public RemoteClient
{
        ClientImpl* _impl;

    protected:
        Client(Client& c) : _impl(c._impl) { }
        Client& operator= (const Client& c) { _impl = c._impl; return *this; }

        void impl(ClientImpl* i) { _impl = i; }

    public:
        Client()
        : _impl(0)
        { }

        virtual ~Client();

        /// This method is used internally to initiate a rpc request.
        void beginCall(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc);

        /// This method is used internally to finalize a rpc request.
        void endCall();

        /// This method is used internally to initiate a rpc request.
        void call(IComposer& r, IRemoteProcedure& method, IDecomposer** argv, unsigned argc);

        /// Returns the timeout for syncronous requests.
        /// When the timeout exprires before the request returns, a cxxtools::IOTimeout is thrown.
        /// On negative timeout the client will wait infinitely. This is the default.
        Milliseconds timeout() const;
        /// Sets the timeout for syncronous requests.
        void timeout(Milliseconds t);

        Milliseconds connectTimeout() const;
        void connectTimeout(Milliseconds t);

        std::string url() const;

        const IRemoteProcedure* activeProcedure() const;

        void cancel();
};

}

}

#endif
