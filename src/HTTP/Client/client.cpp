#include "client.hpp"

#include <Components/Logger/Logger.h>
#include <boost/asio/read_until.hpp>

namespace HTTP
{

bool Client::connectToHost()
{    
    if (isConnected()) {
        return true;
    }

    LOG_INFO("Connecting to host:", m_host, m_port);
    auto const results = m_resolver.resolve(m_host, m_port);

    boost::system::error_code ec;

    if (std::holds_alternative<beast::tcp_stream>(m_socket)) {
        std::get<beast::tcp_stream>(m_socket).connect(results);
    } else if (std::holds_alternative<net::ssl::stream<tcp::socket> >(m_socket)) {
        if(SSL_set_tlsext_host_name(std::get<net::ssl::stream<tcp::socket> >(m_socket).native_handle(), m_host.c_str())) {
            std::get<net::ssl::stream<tcp::socket> >(m_socket).lowest_layer().connect(results->endpoint(), ec);
            std::get<net::ssl::stream<tcp::socket> >(m_socket).handshake(ssl::stream_base::client, ec);
        } else {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        }
    }
    if (ec) {
        LOG_ERROR("Error connecting:", ec.message());
    } else {
        LOG_OK("Connected");
    }
    return (!ec);
}

bool Client::isConnected()
{
    if (std::holds_alternative<beast::tcp_stream>(m_socket)) {
        return std::get<beast::tcp_stream>(m_socket).socket().lowest_layer().is_open();
    }
    return std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().is_open();
}

bool Client::disconnectFromHost()
{
    LOG_INFO("Disconnecting from host");
    if (!isConnected()) {
        LOG_OK("Not connected");
        return true;
    }

    beast::error_code ec;
    if (std::holds_alternative<beast::tcp_stream>(m_socket)) {
        std::get<beast::tcp_stream>(m_socket).socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            LOG_ERROR("Error disconnecting:", ec.message());
            return false;
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
            return false;
        }


        std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            LOG_ERROR("Error disconnecting:", ec.message());
            return false;
        }
        std::get<ssl::stream<tcp::socket> >(m_socket).lowest_layer().close();
    }
    LOG_OK("Disconnected");
    return true;
}

Client::Client(net::io_context &ioc, bool isSecure, bool verifyCertificate) :
    m_resolver{ioc},
    m_socket{beast::tcp_stream(ioc)}
{
    if (isSecure) {
        m_ctx = std::make_shared<ssl::context>(ssl::context::tls_client);
        m_ctx->set_verify_mode(verifyCertificate ? ssl::verify_peer : ssl::verify_none);

        m_socket.emplace<net::ssl::stream<tcp::socket> >(std::move(std::get<beast::tcp_stream>(m_socket).socket()), *m_ctx);
    }
}

Client::~Client()
{
    disconnectFromHost();
}

void Client::setClientName(const std::string &clientName)
{
    m_clientName = clientName;
}

void Client::setMaxFileSize(uint32_t fileSizeByte)
{
    m_maxFileSize = fileSizeByte;
}

void Client::setHost(const std::string &host, const std::string port)
{
    m_host = host;
    m_port = port;
}

HttpPacket Client::request(HttpPacket::MethodType method, const HttpPacket &pkt)
{
    LOG_INFO("Request:", HttpPacket::toString(method), pkt.target);

    http::verb requestMethod;
    switch (method)
    {
    case HttpPacket::Get:       requestMethod = http::verb::get; break;
    case HttpPacket::Put:       requestMethod = http::verb::put; break;
    case HttpPacket::Post:      requestMethod = http::verb::post; break;
    case HttpPacket::Delete:    requestMethod = http::verb::delete_; break;
    default:
        LOG_ERROR("Unknown method to request:", static_cast<int>(method));
        return {};
    }

    http::request<http::dynamic_body> req{requestMethod, pkt.target, 11};
    req.set(http::field::user_agent, m_clientName);
    req.set(http::field::host, m_host);
    req.set(http::field::content_type, pkt.toString(pkt.bodyType));
    beast::ostream(req.body()) << pkt.data;
    req.set(http::field::accept, pkt.toString(pkt.acceptableType));
    req.prepare_payload();

    if (!connectToHost()) {
        return {};
    }

    http::response<http::dynamic_body> res;

    std::visit([&](auto& sock){
        http::write(sock, req);

        beast::flat_buffer buffer;
        http::read(sock, buffer, res);
    }, m_socket);

    HttpPacket resp;
    resp.target = pkt.target;
    resp.acceptableType = pkt.acceptableType;

    resp.statusCode = res.result_int();
    if (resp.statusCode != 200) {
        LOG_ERROR(this, http::obsolete_reason(res.result()));
    }

    if (res.count(http::field::content_type)) {
        resp.bodyType = resp.fromString(res.at(http::field::content_type).to_string());
        if (resp.bodyType != resp.acceptableType) {
            LOG_WARNING("Got packet of inacceptable type (", resp.toString(resp.bodyType), "!=", resp.toString(resp.acceptableType), ")");
        }
    }
    resp.data = beast::buffers_to_string(res.body().data());

    return resp;
}

bool Client::downloadFile(const std::string &target, const std::string &saveFilePath)
{
    LOG_INFO("Downloading file: URL", target, "--->", saveFilePath);

    http::request<http::dynamic_body> req{http::verb::get, target, 11};
    req.set(http::field::user_agent, m_clientName);
    req.set(http::field::host, m_host);
    req.set(http::field::accept, HttpPacket::toString(HttpPacket::BodyType::Bytes));

    req.prepare_payload();

    if (!connectToHost()) {
        return false;
    }

    http::response<http::dynamic_body> res;

    std::visit([&](auto& sock){
        http::write(sock, req);

        beast::flat_buffer buffer;
        http::read(sock, buffer, res);
    }, m_socket);

    HttpPacket resp;
    resp.target = target;
    resp.acceptableType = HttpPacket::BodyType::Bytes;

    resp.statusCode = res.result_int();
    if (resp.statusCode != 200) {
        LOG_ERROR(this, http::obsolete_reason(res.result()));
    }

    if (res.count(http::field::content_type)) {
        resp.bodyType = resp.fromString(res.at(http::field::content_type).to_string());
        if (resp.bodyType != resp.acceptableType) {
            LOG_WARNING("Got packet of inacceptable type (", resp.toString(resp.bodyType), "!=", resp.toString(resp.acceptableType), ")");
        }
    }

    std::ofstream saveFile(saveFilePath, std::ios::binary | std::ios::trunc);
    if (!saveFile.is_open()) {
        LOG_ERROR("Error opening savefile by path: [", saveFilePath, "] reason:", std::strerror(errno));
        return false;
    }
    for (auto const& chunk : res.body().data()) {
        saveFile.write(static_cast<const char*>(chunk.data()), chunk.size());
    }
    return resp.statusCode == 200;
}

bool Client::uploadFile(const std::string &target, const std::string &filePath)
{
    LOG_INFO("Uploading file:", filePath, "---> URL", target);

    http::request<http::file_body> req{http::verb::post, target, 11};
    req.set(http::field::user_agent, m_clientName);
    req.set(http::field::host, m_host);
    req.set(http::field::content_type, HttpPacket::toString(HttpPacket::BodyType::Bytes));

    boost::system::error_code ec;
    http::file_body::value_type targetFile;
    targetFile.open(filePath.c_str(), beast::file_mode::read, ec);
    if (ec) {
        LOG_ERROR("Error opening file:", ec.message());
        return false;
    }
    req.body() = std::move(targetFile);
    req.prepare_payload();

    if (!connectToHost()) {
        return false;
    }

    http::response<http::dynamic_body> res;
    std::visit([&](auto& sock){
        http::write(sock, req);

        beast::flat_buffer buffer;
        http::read(sock, buffer, res);
    }, m_socket);

    return (res.result_int() == 200);
}

}
