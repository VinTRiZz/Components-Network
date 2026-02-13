#include "websocketsession.h"

#include <Components/Logger/Logger.h>

#include <boost/asio/buffers_iterator.hpp>

namespace WebSocket
{

Session::Session(boost::asio::io_context &ioc)
    : m_resolver {net::make_strand(ioc)},
      m_ws{net::make_strand(ioc)}
{

}

Session::~Session()
{
    close();
}

void Session::start(const std::string &host, const std::string &port)
{
    m_host = host;

    // Look up the domain name
    m_resolver.async_resolve(
                host,
                port,
                beast::bind_front_handler(
                    &Session::on_resolve,
                    shared_from_this()));
}

bool Session::isConnected() const
{
    return m_isConnected;
}

void Session::on_resolve(beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results) {
    if(ec) return COMPLOG_ERROR("Resolve:", ec.message());

    // Set the timeout for the operation
    beast::get_lowest_layer(m_ws).expires_after(std::chrono::seconds(10));

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(m_ws).async_connect(
                results,
                beast::bind_front_handler(
                    &Session::on_connect,
                    shared_from_this()));
}

void Session::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
{
    if(ec) {
        m_isConnected = false;
        COMPLOG_ERROR("Connection:", ec.message());
        if (m_closeCallback) {
            m_closeCallback();
        }
        return;
    }

    COMPLOG_DEBUG("Connected");

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(m_ws).expires_never();

    // Set suggested timeout settings for the websocket
    m_ws.set_option(websocket::stream_base::timeout::suggested(
                    beast::role_type::client));

//    // Set a decorator to change the User-Agent of the handshake
//    m_ws.set_option(
//        websocket::stream_base::decorator(
//            [](websocket::request_type& req) {
//               req.set(http::field::user_agent,
//               "ws-test-client");
//            }
//        )
//    );

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    m_host += ':' + std::to_string(ep.port());

    // Perform the websocket handshake
    m_ws.async_handshake(m_host, "/",
                        beast::bind_front_handler(
                            &Session::on_handshake,
                            shared_from_this()));
    m_isConnected = true;
}

void Session::on_handshake(beast::error_code ec)
{
    if(ec) return COMPLOG_ERROR("Handshake:", ec.message());

    COMPLOG_DEBUG("HANDSHAKE");

    // Send the message
//    m_ws.async_write(
//                net::buffer("A - TEST STRING - A"),
//                beast::bind_front_handler(
//                    &Session::on_write,
//                    shared_from_this()));

    m_isConnected = true;
    m_ws.async_read( m_buffer,
                beast::bind_front_handler(
                    &Session::on_read,
                    shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec) {
        COMPLOG_ERROR("Receive:", ec.message());
        if (m_closeCallback) {
            m_closeCallback();
        }
        return;
    }

    COMPLOG_DEBUG("Received data:");
    COMPLOG_DEBUG("=============================");
    COMPLOG_DEBUG(beast::make_printable(m_buffer.data()));
    COMPLOG_DEBUG("=============================");

    if (m_readCallback) {
        auto bufferData = m_buffer.cdata();
        m_readCallback({boost::asio::buffers_begin(bufferData), boost::asio::buffers_end(bufferData)});
    }

    m_ws.async_read( m_buffer,
                beast::bind_front_handler(
                    &Session::on_read,
                    shared_from_this()));
}

bool Session::send(const std::string &iStr)
{
    COMPLOG_DEBUG("Sending data:");
    COMPLOG_DEBUG("=============================");
    COMPLOG_DEBUG(iStr);
    COMPLOG_DEBUG("=============================");
    return (m_ws.write(boost::asio::buffer(iStr)) != 0);
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
    if (!m_isConnected) {
        return;
    }
    m_resolver.cancel();
    m_ws.close(websocket::close_code::normal);
}




SecureSession::SecureSession(boost::asio::io_context &ioc)
    : m_resolver {net::make_strand(ioc)},
      m_ctx{WebSocket::ssl::context::tls_client},
      m_ws{net::make_strand(ioc), m_ctx}
{
    m_ctx.set_verify_mode(WebSocket::ssl::verify_none);
}

SecureSession::~SecureSession()
{
    close();
}

void SecureSession::start(const std::string &host, const std::string &port)
{
    m_host = host;
    COMPLOG_INFO("Connecting to host:", m_host);

    auto const results = m_resolver.resolve(host, port);
    net::connect(m_ws.next_layer().next_layer(), results);
    m_ws.next_layer().handshake(ssl::stream_base::client);

    m_ws.async_handshake(m_host, "/",
        [this](beast::error_code ec) {
        if(ec) return COMPLOG_ERROR("Handshake:", ec.message());

        // Send the message
        //    m_ws.async_write(
        //                net::buffer("A - TEST STRING - A"),
        //                beast::bind_front_handler(
        //                    &Session::on_write,
        //                    shared_from_this()));

        m_isConnected = true;

        m_ws.async_read(m_buffer, [this](beast::error_code ec, size_t bytes) {
            on_read(ec, bytes);
        });
    });
}

void SecureSession::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec) {
        COMPLOG_ERROR("Receive:", ec.message());
        if (m_closeCallback) {
            m_closeCallback();
        }
        return;
    }

//    COMPLOG_DEBUG("Received data:");
//    COMPLOG_DEBUG("=============================");
//    COMPLOG_DEBUG(beast::make_printable(m_buffer.data()));
//    COMPLOG_DEBUG("=============================");

    if (m_readCallback) {
        auto bufferData = m_buffer.cdata();
        m_readCallback({boost::asio::buffers_begin(bufferData), boost::asio::buffers_end(bufferData)});
    }

    m_ws.async_read(m_buffer, [this](beast::error_code ec, size_t bytes) {
        on_read(ec, bytes);
    });
}

bool SecureSession::send(const std::string &iStr)
{
//    COMPLOG_DEBUG("Sending data:");
//    COMPLOG_DEBUG("=============================");
//    COMPLOG_DEBUG(iStr);
//    COMPLOG_DEBUG("=============================");
    return (m_ws.write(boost::asio::buffer(iStr)) != 0);
}

void SecureSession::setReadCallback(const std::function<void (const std::string &)> &readCallback)
{
    m_readCallback = readCallback;
}

void SecureSession::setCloseCallback(const std::function<void ()> closeCallback)
{
    m_closeCallback = closeCallback;
}

void SecureSession::close()
{
    if (!m_isConnected) {
        return;
    }
    m_resolver.cancel();
    m_ws.close(websocket::close_code::normal);
}


}
