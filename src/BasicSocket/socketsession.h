#ifndef WEBSOCKETSESSION_H
#define WEBSOCKETSESSION_H

#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace Socket
{

namespace   net = boost::asio;    // from <boost/asio.hpp>
using       tcp = net::ip::tcp;   // from <boost/asio/ip/tcp.hpp>

class Session : public std::enable_shared_from_this<Session>
{
    tcp::resolver m_resolver;
    tcp::socket m_socket;
    std::string m_host;

    static const uint16_t m_bufferSize {4096};
    char m_bufferData[m_bufferSize];

    bool m_isConnected {false};

    std::function<void(const std::string&)> m_readCallback;
    std::function<void()> m_closeCallback;

public:
    // Resolver and socket require an io_context
    explicit
    Session(net::io_context& ioc) :
        m_socket(ioc),
        m_resolver(ioc)
    {

    }

    ~Session() {
        close();
    }

    void start(const std::string& host, const std::string& port);

    bool isConnected() const;

    void read();

    bool send(const std::string& iStr);
    void setReadCallback(const std::function<void(const std::string&)>& readCallback);
    void setCloseCallback(const std::function<void()> closeCallback);
    void close();
};

}

#endif // WEBSOCKETSESSION_H
