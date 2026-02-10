#include "connectionsession.hpp"

#include <Components/Logger/Logger.h>

namespace HTTP
{

void ConnectionSession::checkDeadline() {
    m_deadlineTimer->cancel();
    m_deadlineTimer->async_wait( // pSelf = shared_from_this()
        [this, pSelf = shared_from_this()](beast::error_code ec) {
        if(!ec) {
            m_responseErrorPacket.statusCode = static_cast<int>(http::status::gateway_timeout);
            sendResponse(m_responseErrorPacket, http::verb::get);
            closeConnection();
            LOG_WARNING(this, "Closed connection due to timeout");
        }
    }
    );
}

void ConnectionSession::closeConnection()
{
    m_deadlineTimer->cancel();

    LOG_INFO(this, "Closing connection");
    if (!isConnected()) {
        LOG_OK(this, "Not connected");
        return;
    }

    beast::error_code ec;
    if (std::holds_alternative<beast::tcp_stream>(m_socket)) {
        std::get<beast::tcp_stream>(m_socket).socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            LOG_ERROR("Error disconnecting:", ec.message());
            return;
        }
        std::get<beast::tcp_stream>(m_socket).close();

    } else if (std::holds_alternative<ssl::stream<tcp::socket> >(m_socket)) {
        std::get<ssl::stream<tcp::socket> >(m_socket).shutdown(ec);
        if (ec == net::error::eof ||
            ec == net::ssl::error::stream_truncated ||
            ec == boost::asio::error::operation_aborted) {
            ec = {};
        }
        if (ec && ec != boost::asio::error::not_connected) {
            LOG_ERROR("Error in SSL shutdown:", ec.message());
            return;
        }


        std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            LOG_ERROR("Error disconnecting:", ec.message());
            return;
        }
        std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().close();
    }
    LOG_OK(this, "Closed connection");
}

ConnectionSession::ConnectionSession(const std::string &selfServerName,
        tcp::socket &&sock,
        const std::shared_ptr<net::ssl::context> &ctx) :
    m_selfServerName {selfServerName},
    m_socket {beast::tcp_stream(std::move(sock))}
{
    const unsigned timeoutSec {10};

    if (ctx) {
        m_socket.emplace<net::ssl::stream<tcp::socket> >(std::move(std::get<beast::tcp_stream>(m_socket).socket()), *ctx);
        m_deadlineTimer = std::make_shared<net::steady_timer>(std::get<net::ssl::stream<tcp::socket> >(m_socket).get_executor(),
                           std::chrono::seconds(timeoutSec));
    } else {
        m_deadlineTimer = std::make_shared<net::steady_timer>(std::get<beast::tcp_stream>(m_socket).get_executor(),
                           std::chrono::seconds(timeoutSec));
    }
}

ConnectionSession::~ConnectionSession()
{
    closeConnection();
}

void ConnectionSession::handleRequests() {
    if (std::holds_alternative<net::ssl::stream<tcp::socket> >(m_socket)) {
        boost::system::error_code ec;
        std::get<net::ssl::stream<tcp::socket>>(m_socket).handshake(net::ssl::stream_base::server, ec);
        if (ec && (ec != net::ssl::error::stream_truncated)) {
            LOG_ERROR("SSL handshake error, closing connection. Reason:", ec.message());
            std::get<net::ssl::stream<tcp::socket>>(m_socket).lowest_layer().shutdown(net::socket_base::shutdown_both);
            return;
        }
    }

    auto readHandler = [pSelf = shared_from_this(), this]
            (beast::error_code ec, std::size_t bytes_transferred) {
        if (ec == beast::errc::not_connected ||
                ec == net::error::eof ||
                ec == net::error::connection_reset ||
                ec == net::ssl::error::stream_truncated ||
                ec == net::error::operation_aborted ||
                ec.value() == 1) { // TODO: Разобраться чё за код
            return;
        }

        if(ec) {
            LOG_ERROR(this, "Read error:", ec.message());
            closeConnection();
            return;
        }

        Packet request;
        request.target  = m_request.target().to_string();
        request.body    = std::move(beast::buffers_to_string(m_request.body().data()));

        if (m_request.count(http::field::content_type)) {
            auto bodyType = m_request.at(http::field::content_type).to_string();
            request.bodyType = request.fromString(bodyType);
        }

        LOG_INFO(this, "Request:", m_request.method_string(), request.target, "(", request.toString(request.bodyType), ")");

        MethodType targetMethodType;
        switch  (m_request.method())
        {
        case http::verb::get:       targetMethodType = MethodType::Get; break;
        case http::verb::put:       targetMethodType = MethodType::Put; break;
        case http::verb::post:      targetMethodType = MethodType::Post; break;
        case http::verb::delete_:   targetMethodType = MethodType::Delete; break;

        default: // От варнингов
            break;
        }

        if (auto targetProcessor = m_processors.find(targetMethodType); targetProcessor != m_processors.end()) {
            targetProcessor->second(std::move(request));
        } else {
            LOG_WARNING(this, "Unknown method:", m_request.method_string());
            sendErrorResponse(http::status::method_not_allowed, "Invalid method");
        }

        handleRequests();
    };

    std::visit([=](auto& sock){
        http::async_read(sock, m_buffer, m_request, readHandler);
    }, m_socket);
    checkDeadline();
}

void ConnectionSession::sendResponse(const Packet &pkt, http::verb method) {

    LOG_INFO(this, "Sending response with status", pkt.statusCode);
    if (pkt.isFile) {
        m_response = http::response<http::file_body>();

        http::file_body::value_type targetFile;

        boost::system::error_code ec;
        targetFile.open(pkt.target.c_str(), beast::file_mode::read, ec);

        if (ec) {
            LOG_ERROR("Error opening file:", ec.message());
            sendErrorResponse(http::status::bad_request, "Invalid file to download");
            return;
        }
        std::get<http::response<http::file_body> >(m_response).body() = std::move(targetFile);
        std::get<http::response<http::file_body> >(m_response).prepare_payload();
    } else {
        m_response = http::response<http::dynamic_body>();
        beast::ostream(std::get<http::response<http::dynamic_body> >(m_response).body()) << pkt.body;
        std::get<http::response<http::dynamic_body> >(m_response)
                .content_length(std::get<http::response<http::dynamic_body> >(m_response).body().size());
        std::get<http::response<http::dynamic_body> >(m_response).prepare_payload();
    }

    std::visit([&](auto& resp){
        // Etc settings
        resp.version(11);
        resp.set(http::field::server, m_selfServerName);

        // Body
        resp.set(http::field::content_type, pkt.toString(pkt.bodyType));

        // Result of request
        resp.result(pkt.statusCode);
    }, m_response);

    std::visit([&](auto& sock){
        std::visit([&](auto& resp){
            http::async_write(
                sock, resp,
                [self = shared_from_this(), this, targetFilePath = pkt.target](beast::error_code ec, std::size_t) {
                if (ec) {
                    LOG_ERROR(this, "Error sending response:", ec.message());
                } else {
                    LOG_OK(this, "Response sent");
                }
            });
        }, m_response);
    }, m_socket);
}

void ConnectionSession::sendErrorResponse(http::status status, const std::string &errorString)
{
    m_responseErrorPacket = createErrorPacket(static_cast<unsigned>(status));
    sendResponse(m_responseErrorPacket, m_request.method());
}

bool ConnectionSession::isConnected() const
{
    if (std::holds_alternative<beast::tcp_stream>(m_socket)) {
        return std::get<beast::tcp_stream>(m_socket).socket().lowest_layer().is_open();
    }
    return std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().is_open();
}

}
