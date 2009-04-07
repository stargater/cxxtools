/*
 * Copyright (C) 2005-2006 by Marc Boris Duerner
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
#include "cxxtools/unit/testsuite.h"
#include "cxxtools/unit/registertest.h"
#include "cxxtools/unit/testmain.h"
#include "cxxtools/xmlrpc/service.h"
#include "cxxtools/xmlrpc/client.h"
#include "cxxtools/xmlrpc/fault.h"
#include "cxxtools/xmlrpc/remoteprocedure.h"
#include "cxxtools/http/server.h"
#include "cxxtools/eventloop.h"
#include "cxxtools/thread.h"


struct Color
{
    int red;
    int green;
    int blue;
};


void operator >>=(const cxxtools::SerializationInfo& si, Color& color)
{
    color.red = si.getValue<int>("red");
    color.green = si.getValue<int>("green");
    color.blue = si.getValue<int>("blue");
}


void operator <<=(cxxtools::SerializationInfo& si, const Color& color)
{
    si.addMember("red") <<= color.red;
    si.addMember("green") <<= color.green;
    si.addMember("blue") <<= color.blue;
}


class XmlRpcTest : public cxxtools::unit::TestSuite
{
    private:
        cxxtools::EventLoop* _loop;
        cxxtools::http::Server* _server;

    public:
        XmlRpcTest()
        : cxxtools::unit::TestSuite("cxxtools-xmlrpc-Test")
        {
            this->registerMethod("Fault", *this, &XmlRpcTest::Fault);
            this->registerMethod("Nothing", *this, &XmlRpcTest::Nothing);
            this->registerMethod("Boolean", *this, &XmlRpcTest::Boolean);
            this->registerMethod("Integer", *this, &XmlRpcTest::Integer);
            this->registerMethod("Double", *this, &XmlRpcTest::Double);
            this->registerMethod("String", *this, &XmlRpcTest::String);
            this->registerMethod("Array", *this, &XmlRpcTest::Array);
            this->registerMethod("Struct", *this, &XmlRpcTest::Struct);
        }

        void failTest()
        {
            throw cxxtools::unit::Assertion("test timed out", CXXTOOLS_SOURCEINFO);
        }

        void setUp()
        {
            _loop = new cxxtools::EventLoop();
            _loop->setIdleTimeout(2000);
            connect(_loop->timeout, *this, &XmlRpcTest::failTest);
            connect(_loop->timeout, *_loop, &cxxtools::EventLoop::exit);

            _server = new cxxtools::http::Server("127.0.0.1", 8001);
        }

        void tearDown()
        {
            delete _loop;
            delete _server;
        }

        void Fault()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::throwFault);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<bool> multiply(client, "multiply");
            connect( multiply.fault, *this, &XmlRpcTest::onFault );
            multiply.begin();

            _loop->run();
            _server->terminate();
        }

        void onFault(const cxxtools::xmlrpc::Fault& fault)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(fault.rc(), 7)
            CXXTOOLS_UNIT_ASSERT_EQUALS(fault.text(), "Fault")
            _loop->exit();
        }

        void Nothing()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyNothing);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<bool> multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onNothingFinished );

            multiply.begin();

            _loop->run();
            _server->terminate();
        }

        void onNothingFinished(const bool& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r, false)

            _loop->exit();
        }

        void Boolean()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyBoolean);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<bool, bool, bool> multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onBooleanFinished );

            multiply.begin(true, true);

            _loop->run();
            _server->terminate();
        }

        void onBooleanFinished(const bool& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r, true)

            _loop->exit();
        }

        void Integer()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyInt);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<int, int, int> multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onIntegerFinished );

            multiply.begin(2, 3);

            _loop->run();
            _server->terminate();
        }

        void onIntegerFinished(const int& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r, 6)

            _loop->exit();
        }

        void Double()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyDouble);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<double, double, double> multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onDoubleFinished );

            multiply.begin(2.0, 3.0);

            _loop->run();
            _server->terminate();
        }

        void onDoubleFinished(const double& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r, 6.0)

            _loop->exit();
        }

        void String()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyString);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure<std::string, std::string, std::string> multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onStringFinished );

            multiply.begin("2", "3");

            _loop->run();
            _server->terminate();
        }

        void onStringFinished(const std::string& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r, "6")

            _loop->exit();
        }

        void Array()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyVector);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure< std::vector<int>, std::vector<int>, std::vector<int> > multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onArrayFinished );

            std::vector<int> vec;
            vec.push_back(10);
            vec.push_back(20);

            multiply.begin(vec, vec);

            _loop->run();
            _server->terminate();
        }

        void onArrayFinished(const std::vector<int>& r)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(r.size(), 2)
            CXXTOOLS_UNIT_ASSERT_EQUALS(r.at(0), 100)
            CXXTOOLS_UNIT_ASSERT_EQUALS(r.at(1), 400)

            _loop->exit();
        }

        void Struct()
        {
            cxxtools::xmlrpc::Service service;
            service.registerMethod("multiply", *this, &XmlRpcTest::multiplyColor);
            _server->addService("/calc", service);

            cxxtools::AttachedThread serverThread( cxxtools::callable(*_server, &cxxtools::http::Server::run) );
            serverThread.start();
            cxxtools::Thread::sleep(500);

            cxxtools::xmlrpc::Client client(*_loop, "127.0.0.1", 8001, "/calc");
            cxxtools::xmlrpc::RemoteProcedure< Color, Color, Color > multiply(client, "multiply");
            connect( multiply.finished, *this, &XmlRpcTest::onStuctFinished );

            Color a;
            a.red = 2;
            a.green = 3;
            a.blue = 4;

            Color b;
            b.red = 3;
            b.green = 4;
            b.blue = 5;

            multiply.begin(a, b);

            _loop->run();
            _server->terminate();
        }

        void onStuctFinished(const Color& color)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(color.red, 6)
            CXXTOOLS_UNIT_ASSERT_EQUALS(color.green, 12)
            CXXTOOLS_UNIT_ASSERT_EQUALS(color.blue, 20)

            _loop->exit();
        }

        bool throwFault()
        {
            throw cxxtools::xmlrpc::Fault("Fault", 7);
            return false;
        }

        bool multiplyNothing()
        {
            return false;
        }

        bool multiplyBoolean(bool a, bool b)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(a, true)
            CXXTOOLS_UNIT_ASSERT_EQUALS(b, true)
            return true;
        }

        int multiplyInt(int a, int b)
        {
            return a*b;
        }

        double multiplyDouble(double a, double b)
        {
            return a*b;
        }

        std::string multiplyString(std::string a, std::string b)
        {
            CXXTOOLS_UNIT_ASSERT_EQUALS(a, "2")
            CXXTOOLS_UNIT_ASSERT_EQUALS(b, "3")
            return "6";
        }

        std::vector<int> multiplyVector(const std::vector<int>& a, const std::vector<int>& b)
        {
            std::vector<int> r;
            r.push_back( a.at(0) * b.at(0) );
            r.push_back( a.at(1) * b.at(1) );
            return r;
        }

        Color multiplyColor(const Color& a, const Color& b)
        {
            Color color;
            color.red = a.red * b.red;
            color.green = a.green * b.green;
            color.blue = a.blue * b.blue;
            return color;
        }
};

cxxtools::unit::RegisterTest<XmlRpcTest> register_XmlRpcTest;