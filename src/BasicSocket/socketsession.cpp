#include "socketsession.h"

#include <Components/Logger/Logger.h>
#include <boost/asio/buffers_iterator.hpp>

namespace Socket
{

void Session::start(const std::string &host, const std::string &port)
{
    m_host = host;

    // Look up the domain name
    net::async_connect(
        m_socket,
        m_resolver.resolve(host, port),
        [pThis = shared_from_this()](boost::system::error_code ec, tcp::endpoint){
        if (ec) {
            COMPLOG_ERROR("Connection error:", ec.message());
            return;
        }
        COMPLOG_OK("Connected to", pThis->m_host);
        pThis->read();
        pThis->m_isConnected = true;
    });
}

bool Session::isConnected() const
{
    return m_isConnected;
}

void Session::read()
{
    m_socket.async_read_some(
        boost::asio::buffer(m_bufferData, m_bufferSize),
        [pThis = shared_from_this()](boost::system::error_code ec, std::size_t length) {
            if (ec) {
                COMPLOG_ERROR("Read:", ec.message());
                return;
            }

            auto response = std::string(pThis->m_bufferData, length);
            if (pThis->m_readCallback) {
                pThis->m_readCallback(response);
            }
            pThis->read();
    });
}

bool Session::send(const std::string &iStr)
{
    auto res = (m_socket.write_some(net::buffer(iStr + "\n")) != 0);
    return res;
}

void Session::setReadCallback(const std::function<void (const std::string &)> &readCallback)
{
    m_readCallback = readCallback;
}

void Session::setCloseCallback(const std::function<void ()> closeCallback)
{
    m_closeCallback = closeCallback;
}

void Session::close()
{
    m_socket.cancel();
    m_socket.shutdown(tcp::socket::shutdown_both);
    m_socket.close();
    COMPLOG_INFO("Closed connection to", m_host);
}



}
