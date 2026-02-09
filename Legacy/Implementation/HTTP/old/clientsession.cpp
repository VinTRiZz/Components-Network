#include "clientsession.hpp"

#include <Components/Logger/Logger.h>
#include <Components/Encryption/Hash.h>

#include <boost/scope_exit.hpp>

namespace HTTPOld
{

void ClientSession::restartTimeout()
{
    auto pSelf = shared_from_this();

    m_deadline.cancel();
    m_deadline.async_wait(
                [pSelf](beast::error_code ec) {
        if(!ec) {
            http::response<http::dynamic_body> timeoutResponse;
            timeoutResponse.version(pSelf->m_request.version());
            timeoutResponse.keep_alive(false);
            timeoutResponse.set(http::field::server, serverName);
            timeoutResponse.result(http::status::gateway_timeout);

            http::async_write(
                        pSelf->m_socket,
                        pSelf->m_response,
                        [pSelf](beast::error_code ec, std::size_t) {
                    pSelf->m_socket.shutdown(tcp::socket::shutdown_send);
                    pSelf->m_deadline.cancel();
                }
            );
            LOG_WARNING("Closing connection due to timeout");
        }
    }
    );
}

void ClientSession::readRequest() {

    if (!m_requestGet ||
        !m_requestPost ||
        !m_requestPut ||
        !m_requestDelete) {
        throw std::runtime_error("Not set processors for connection handling");
    }

    auto pSelf = shared_from_this();

    http::async_read(
                m_socket,
                m_buffer,
                m_request,
                [pSelf](beast::error_code ec,
                std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == beast::errc::not_connected ||
            ec == boost::asio::error::eof ||
            ec == boost::asio::error::connection_reset ||
            ec.value() == 1) { // TODO: Разобраться чё за код
            pSelf->m_deadline.cancel();
            return;
        }

        if(ec) {
            pSelf->m_deadline.cancel();
            LOG_INFO("Read error:", ec.value(), "(", ec.message(), ")");
            return;
        }
        pSelf->processRequest();
    });
}

void ClientSession::processRequest()
{
    auto pSelf = shared_from_this();

    m_response.version(m_request.version());
    m_response.keep_alive(false);
    m_response.set(http::field::server, serverName);

    Server::PacketMeta response;
    response.target = std::string(m_request.target());
    response.body = beast::buffers_to_string(m_request.body().cdata());

    auto connectionEndpoint = m_socket.remote_endpoint();
    auto connectionAddress = connectionEndpoint.address().to_string();
    auto connectionHash = Encryption::sha256(connectionAddress);
    response.sessionId = connectionHash;

//    LOG_EMPTY("============ REQUEST ==============");
//    LOG_EMPTY("CON ID:  ", response.sessionId);
//    LOG_EMPTY("TARGET:  ", response.target);
//    LOG_EMPTY("TYPE:    ", m_request.method_string());
//    LOG_EMPTY("BODY:    ", response.body);
//    LOG_EMPTY("==================================");

    switch(m_request.method())
    {
    case http::verb::get:       m_requestGet(response);     break;
    case http::verb::post:      m_requestPost(response);    break;
    case http::verb::put:       m_requestPut(response);     break;
    case http::verb::delete_:   m_requestDelete(response);  break;

    default:
        m_response.result(http::status::bad_request);
        m_response.set(http::field::content_type, "text/plain");
        beast::ostream(m_response.body())
                << "{\"info\":\"Unknown method: "
                << std::string(m_request.method_string())
                << "\"";

        http::async_write(
                    pSelf->m_socket,
                    pSelf->m_response,
                    [pSelf](beast::error_code ec, std::size_t) {
                pSelf->m_deadline.cancel();
                pSelf->m_socket.shutdown(tcp::socket::shutdown_send, ec);
            }
        );

        LOG_ERROR("INVALID REQUEST METHOD GOT:", m_request.method_string());
        return;
    }

    switch (response.responseType) {
        case Server::ResponseType::ErrorText: m_response.set(http::field::content_type, "text/plain"); break;
        case Server::ResponseType::PlainText: m_response.set(http::field::content_type, "text/plain"); break;
        case Server::ResponseType::HTML: m_response.set(http::field::content_type, "text/html"); break;
        case Server::ResponseType::JSON: m_response.set(http::field::content_type, "application/json"); break;
    };

    switch (response.responseResult) {
    case Server::ResponseResult::Ok:
        m_response.result(http::status::ok);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;

    case Server::ResponseResult::BadRequest:
        m_response.result(http::status::bad_request);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;

    case Server::ResponseResult::NotFound:
        m_response.result(http::status::not_found);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;

    case Server::ResponseResult::NotImplemented:
        m_response.result(http::status::not_implemented);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;

    case Server::ResponseResult::NoContent:
        m_response.result(http::status::no_content);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;

    case Server::ResponseResult::Unauthorized:
        m_response.result(http::status::unauthorized);
        beast::ostream(m_response.body()) << response.body;
        m_response.content_length(m_response.body().size());
        break;
    }

//    LOG_EMPTY("=========== RESPONSE =============");
//    LOG_EMPTY("CON ID:      ", response.sessionId);
//    LOG_EMPTY("TYPE:        ", m_response.at(http::field::content_type));
//    LOG_EMPTY("RESULT:      ", m_response.result_int());

//    auto bodyBuf = beast::buffers_to_string(m_response.body().cdata());
//    if (bodyBuf.size() < 100) {
//        LOG_EMPTY("BODY:        ", bodyBuf);
//    } else {
//        LOG_EMPTY("BODY SIZE:   ", bodyBuf.size());
//    }
//    LOG_EMPTY("==================================");

    http::async_write(
                pSelf->m_socket,
                pSelf->m_response,
                [pSelf](beast::error_code ec, std::size_t) {
            pSelf->m_deadline.cancel();
            pSelf->m_socket.shutdown(tcp::socket::shutdown_send, ec);
        }
    );
}

ClientSession::ClientSession(tcp::socket &&sock) :
    m_socket(std::move(sock))
{

}

ClientSession::~ClientSession()
{

}

}
