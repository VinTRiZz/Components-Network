#ifndef WEBSOCKETSESSION_H
#define WEBSOCKETSESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace WebSocket
{

namespace   beast = boost::beast;         // from <boost/beast.hpp>
namespace   http = beast::http;           // from <boost/beast/http.hpp>
namespace   websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace   ssl = boost::asio::ssl;
namespace   net = boost::asio;            // from <boost/asio.hpp>
using       tcp = boost::asio::ip::tcp;   // from <boost/asio/ip/tcp.hpp>

class Session : public std::enable_shared_from_this<Session>
{
    tcp::resolver m_resolver;
    websocket::stream<beast::tcp_stream> m_ws;
    beast::flat_buffer m_buffer;
    std::string m_host;
    std::string m_bufferText;

    bool m_isConnected {false};

    std::function<void(const std::string&)> m_readCallback;
    std::function<void()> m_closeCallback;

public:
    // Resolver and socket require an io_context
    Session(net::io_context& ioc);
    ~Session();

    void start(const std::string& host, const std::string& port = "80");

    bool isConnected() const;

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);

    void on_handshake(beast::error_code ec);

    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred);

    bool send(const std::string& iStr);
    void setReadCallback(const std::function<void(const std::string&)>& readCallback);
    void setCloseCallback(const std::function<void()> closeCallback);
    void close();
};


class SecureSession : public std::enable_shared_from_this<SecureSession>
{
    tcp::resolver m_resolver;
    ssl::context m_ctx;
    websocket::stream<ssl::stream<tcp::socket> > m_ws;
    beast::flat_buffer m_buffer;
    std::string m_bufferText;

    std::string m_host;
    bool m_isConnected {false};

    std::function<void(const std::string&)> m_readCallback;
    std::function<void()> m_closeCallback;

public:
    // Resolver and socket require an io_context
    SecureSession(net::io_context& ioc);
    ~SecureSession();

    void start(const std::string& host, const std::string& port = "443");

    void on_read(
        beast::error_code ec,
        std::size_t bytes_transferred);

    bool send(const std::string& iStr);
    void setReadCallback(const std::function<void(const std::string&)>& readCallback);
    void setCloseCallback(const std::function<void()> closeCallback);
    void close();
};

}

#endif // WEBSOCKETSESSION_H
