#include "server.hpp"

#include <chrono>
#include <vector>
#include <iostream>
#include <sstream>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include <Components/Logger/Logger.h>

#include "clientsession.hpp"

namespace HTTPOld
{

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

struct Server::Impl
{
    net::io_context ioc {2};
    tcp::acceptor acceptor {ioc};
    tcp::socket socket{ioc};

    std::function<void(PacketMeta& requestMeta)> m_requestGet;
    std::function<void(PacketMeta& requestMeta)> m_requestPost;
    std::function<void(PacketMeta& requestMeta)> m_requestPut;
    std::function<void(PacketMeta& requestMeta)> m_requestDelete;

    void acceptConnections() {
        acceptor.async_accept(socket,
            [&](beast::error_code ec) {
                if (ec) {
                    COMPLOG_ERROR("Error in connection:", ec.message());
                } else {
                    auto pCon = std::shared_ptr<ClientSession>(new ClientSession(std::move(socket)));
                    pCon->m_requestGet = m_requestGet;
                    pCon->m_requestPost = m_requestPost;
                    pCon->m_requestPut = m_requestPut;
                    pCon->m_requestDelete = m_requestDelete;
                    pCon->readRequest();
                }
                acceptConnections();
            }
        );
    }
};

Server::Server() :
    d {new Impl}
{

}

Server::~Server()
{

}

void Server::start(uint16_t port)
{
    m_port = port;

    if (d->acceptor.is_open()) {
        d->acceptor.close();
    }

    // Rebind and listen on the new port
    tcp::endpoint endpoint(tcp::v4(), port);
    d->acceptor.open(endpoint.protocol());
    d->acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    d->acceptor.bind(endpoint);
    d->acceptor.listen();

    d->acceptConnections();

    try {
        d->ioc.run();
    } catch (const std::exception& ex) {
        COMPLOG_ERROR("[start] Exception:", ex.what());
    }
}

void Server::stop()
{
    d->acceptor.cancel();
    d->acceptor.close();
    d->socket.cancel();
    d->socket.close();
    d->ioc.stop();
}

void Server::setProcessorGet(const std::function<void (PacketMeta &)> &proc)
{
    d->m_requestGet = proc;
}

void Server::setProcessorPost(const std::function<void (PacketMeta &)> &proc)
{
    d->m_requestPost = proc;
}

void Server::setProcessorPut(const std::function<void (PacketMeta &)> &proc)
{
    d->m_requestPut = proc;
}

void Server::setProcessorDelete(const std::function<void (PacketMeta &)> &proc)
{
    d->m_requestDelete = proc;
}

}
