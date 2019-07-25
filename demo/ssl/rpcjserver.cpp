/*
 * Copyright (C) 2017 Tommi Maekitalo
 *
 */

#include <iostream>
#include <cxxtools/json/rpcserver.h>
#include <cxxtools/sslcertificate.h>
#include <cxxtools/eventloop.h>
#include <cxxtools/arg.h>
#include <cxxtools/log.h>

bool doAccept(const cxxtools::SslCertificate& cert)
{
    std::cout << "cert \"" << cert.getSubject() << "\" accepted" << std::endl;
    return true;
}

bool noAccept(const cxxtools::SslCertificate& cert)
{
    std::cout << "cert \"" << cert.getSubject() << "\" not accepted" << std::endl;
    return false;
}

std::string echo(const std::string& msg)
{
    return msg;
}

int add(int a, int b)
{
    return a + b;
}

int main(int argc, char* argv[])
{
    try
    {
        log_init();
        cxxtools::Arg<std::string> ip(argc, argv, 'i');
        cxxtools::Arg<unsigned short> port(argc, argv, 'j', 7004);
        cxxtools::Arg<std::string> cert(argc, argv, 'c');
        cxxtools::Arg<std::string> ca(argc, argv, 'C', "ca.crt");
        cxxtools::Arg<bool> deny(argc, argv, 'd');
        cxxtools::Arg<bool> nodeny(argc, argv, 'D');

        cxxtools::EventLoop loop;

        cxxtools::json::RpcServer server(loop, ip, port, cert, 2, ca);
        server.registerFunction("echo", echo);
        server.registerFunction("add", add);

        if (deny)
            cxxtools::connect(server.acceptSslCertificate(), noAccept);
        else if (nodeny)
            cxxtools::connect(server.acceptSslCertificate(), doAccept);

        loop.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
}

