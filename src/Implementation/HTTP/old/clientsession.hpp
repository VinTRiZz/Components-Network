#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <chrono>
#include <vector>
#include <iostream>
#include <sstream>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include "server.hpp"

namespace HTTPOld
{

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

struct ClientSession : public std::enable_shared_from_this<ClientSession>
{
    std::chrono::time_point<std::chrono::high_resolution_clock> sessionBeginTime {std::chrono::high_resolution_clock::now()};

    tcp::socket m_socket;
    beast::flat_buffer m_buffer {65535};
    http::request<http::dynamic_body> m_request;
    http::response<http::dynamic_body> m_response;
    net::steady_timer m_deadline {m_socket.get_executor(), std::chrono::seconds(10)};

    std::function<void(Server::PacketMeta& requestMeta)> m_requestGet;
    std::function<void(Server::PacketMeta& requestMeta)> m_requestPost;
    std::function<void(Server::PacketMeta& requestMeta)> m_requestPut;
    std::function<void(Server::PacketMeta& requestMeta)> m_requestDelete;

    void restartTimeout();
    void readRequest();
    void processRequest();
    void setPage(const std::string& pagePath);

    ClientSession(tcp::socket&& sock);
    ~ClientSession();
};

}

#endif // CLIENTSESSION_H
