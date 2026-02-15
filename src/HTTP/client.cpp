#include "client.hpp"

#include <variant>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>

#include <Components/Logger/Logger.h>
#include <boost/asio/read_until.hpp>

namespace HTTP
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

struct Client::Impl
{
    net::io_context defaultCtx;
    std::thread defaultCtxThread;

    tcp::resolver resolver;
    std::variant<beast::tcp_stream, ssl::stream<tcp::socket> > socket;
    std::shared_ptr<ssl::context> ctx;

    std::string host;
    std::string port;

    std::string clientName {"TestApp"};
    uint32_t maxFileSize {1024 * 1024 * 1024};

    Impl(net::io_context& ioc) :
        resolver{ioc},
        socket{beast::tcp_stream(ioc)} {

    }

    Impl() :
        resolver{defaultCtx},
        socket{beast::tcp_stream(defaultCtx)} {
        defaultCtxThread = std::thread([this](){
            defaultCtx.run();
        });
    }
    ~Impl() {
        if (defaultCtxThread.joinable()) {
            defaultCtx.stop();
            defaultCtxThread.join();
        }
    }
};

Client::Client(boost::asio::io_context &ioc, bool isSecure, bool verifyCertificate) :
    d {new Impl(ioc)}
{
    if (isSecure) {
        d->ctx = std::make_shared<ssl::context>(ssl::context::tls_client);
        d->ctx->set_verify_mode(verifyCertificate ? ssl::verify_peer : ssl::verify_none);

        d->socket.emplace<net::ssl::stream<tcp::socket> >(std::move(std::get<beast::tcp_stream>(d->socket).socket()), *d->ctx);
    }
}

Client::Client(bool isSecure, bool verifyCertificate) :
    d {new Impl()}
{

}

Client::~Client()
{
    disconnectFromHost();
}

void Client::setClientName(const std::string &clientName)
{
    d->clientName = clientName;
}

void Client::setMaxFileSize(uint32_t fileSizeByte)
{
    d->maxFileSize = fileSizeByte;
}

void Client::setHost(const std::string &host, const uint16_t port)
{
    d->host = host;
    d->port = std::to_string(port);
}

Packet Client::request(MethodType method, Packet &&pkt)
{
    COMPLOG_INFO("Request:", toString(method), pkt.target);

    http::verb requestMethod;
    switch (method)
    {
    case Get:       requestMethod = http::verb::get; break;
    case Put:       requestMethod = http::verb::put; break;
    case Post:      requestMethod = http::verb::post; break;
    case Delete:    requestMethod = http::verb::delete_; break;
    default:
        COMPLOG_ERROR("Unknown method to request:", static_cast<int>(method));
        return {};
    }

    http::request<http::dynamic_body> req{requestMethod, pkt.target, 11};
    req.set(http::field::user_agent, d->clientName);
    req.set(http::field::host, d->host);
    req.set(http::field::content_type, pkt.toString(pkt.bodyType));
    beast::ostream(req.body()) << std::move(pkt.body);
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
    }, d->socket);

    pkt.statusCode = res.result_int();
    if (pkt.statusCode != 200) {
        COMPLOG_ERROR(this, http::obsolete_reason(res.result()));
    }

    if (res.count(http::field::content_type)) {
        pkt.bodyType = pkt.fromString(to_string(res.at(http::field::content_type)));
        if (pkt.bodyType != pkt.acceptableType) {
            COMPLOG_WARNING("Got packet of inacceptable type (", pkt.toString(pkt.bodyType), "!=", pkt.toString(pkt.acceptableType), ")");
        }
    }
    pkt.body = beast::buffers_to_string(res.body().data());
    return pkt;
}

Packet Client::request(MethodType method, const Packet &pkt)
{
    COMPLOG_INFO("Request:", toString(method), pkt.target);

    http::verb requestMethod;
    switch (method)
    {
    case Get:       requestMethod = http::verb::get; break;
    case Put:       requestMethod = http::verb::put; break;
    case Post:      requestMethod = http::verb::post; break;
    case Delete:    requestMethod = http::verb::delete_; break;
    default:
        COMPLOG_ERROR("Unknown method to request:", static_cast<int>(method));
        return {};
    }

    http::request<http::dynamic_body> req{requestMethod, pkt.target, 11};
    req.set(http::field::user_agent, d->clientName);
    req.set(http::field::host, d->host);
    req.set(http::field::content_type, pkt.toString(pkt.bodyType));
    beast::ostream(req.body()) << pkt.body;
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
    }, d->socket);

    Packet resp;
    resp.target = pkt.target;
    resp.acceptableType = pkt.acceptableType;

    resp.statusCode = res.result_int();
    if (resp.statusCode != 200) {
        COMPLOG_ERROR(this, http::obsolete_reason(res.result()));
    }

    if (res.count(http::field::content_type)) {
        resp.bodyType = resp.fromString(to_string(res.at(http::field::content_type)));
        if (resp.bodyType != resp.acceptableType) {
            COMPLOG_WARNING("Got packet of inacceptable type (", resp.toString(resp.bodyType), "!=", resp.toString(resp.acceptableType), ")");
        }
    }
    resp.body = beast::buffers_to_string(res.body().data());

    return resp;
}

bool Client::downloadFile(const std::string &target, const std::string &saveFilePath)
{
    COMPLOG_INFO("Downloading file: URL", target, "--->", saveFilePath);

    http::request<http::dynamic_body> req{http::verb::get, target, 11};
    req.set(http::field::user_agent, d->clientName);
    req.set(http::field::host, d->host);
    req.set(http::field::accept, Packet::toString(Packet::BodyType::Bytes));

    req.prepare_payload();

    if (!connectToHost()) {
        return false;
    }

    http::response<http::dynamic_body> res;

    std::visit([&](auto& sock){
        http::write(sock, req);

        beast::flat_buffer buffer;
        http::read(sock, buffer, res);
    }, d->socket);

    Packet resp;
    resp.target = target;
    resp.acceptableType = Packet::BodyType::Bytes;

    resp.statusCode = res.result_int();
    if (resp.statusCode != 200) {
        COMPLOG_ERROR(this, http::obsolete_reason(res.result()));
    }

    if (res.count(http::field::content_type)) {
        resp.bodyType = resp.fromString(to_string(res.at(http::field::content_type)));
        if (resp.bodyType != resp.acceptableType) {
            COMPLOG_WARNING("Got packet of inacceptable type (", resp.toString(resp.bodyType), "!=", resp.toString(resp.acceptableType), ")");
        }
    }

    std::ofstream saveFile(saveFilePath, std::ios::binary | std::ios::trunc);
    if (!saveFile.is_open()) {
        COMPLOG_ERROR("Error opening savefile by path: [", saveFilePath, "] reason:", std::strerror(errno));
        return false;
    }
    for (auto const& chunk : res.body().data()) {
        saveFile.write(static_cast<const char*>(chunk.data()), chunk.size());
    }
    return resp.statusCode == 200;
}

bool Client::uploadFile(const std::string &target, const std::string &filePath)
{
    COMPLOG_INFO("Uploading file:", filePath, "---> URL", target);

    http::request<http::file_body> req{http::verb::post, target, 11};
    req.set(http::field::user_agent, d->clientName);
    req.set(http::field::host, d->host);
    req.set(http::field::content_type, Packet::toString(Packet::BodyType::Bytes));

    boost::system::error_code ec;
    http::file_body::value_type targetFile;
    targetFile.open(filePath.c_str(), beast::file_mode::read, ec);
    if (ec) {
        COMPLOG_ERROR("Error opening file:", ec.message());
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
    }, d->socket);

    return (res.result_int() == 200);
}

bool Client::connectToHost()
{
    if (isConnected()) {
        return true;
    }

    COMPLOG_INFO("Connecting to host:", d->host, d->port);
    auto const results = d->resolver.resolve(d->host, d->port);

    boost::system::error_code ec;

    if (std::holds_alternative<beast::tcp_stream>(d->socket)) {
        std::get<beast::tcp_stream>(d->socket).connect(results);
    } else if (std::holds_alternative<net::ssl::stream<tcp::socket> >(d->socket)) {
        if(SSL_set_tlsext_host_name(std::get<net::ssl::stream<tcp::socket> >(d->socket).native_handle(), d->host.c_str())) {
            std::get<net::ssl::stream<tcp::socket> >(d->socket).lowest_layer().connect(results->endpoint(), ec);
            std::get<net::ssl::stream<tcp::socket> >(d->socket).handshake(ssl::stream_base::client, ec);
        } else {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        }
    }
    if (ec) {
        COMPLOG_ERROR("Error connecting:", ec.message());
    } else {
        COMPLOG_OK("Connected");
    }
    return (!ec);
}

bool Client::isConnected()
{
    if (std::holds_alternative<beast::tcp_stream>(d->socket)) {
        return std::get<beast::tcp_stream>(d->socket).socket().lowest_layer().is_open();
    }
    return std::get<ssl::stream<tcp::socket> >(d->socket).lowest_layer().is_open();
}

bool Client::disconnectFromHost()
{
    COMPLOG_INFO("Disconnecting from host");
    if (!isConnected()) {
        COMPLOG_OK("Not connected");
        return true;
    }

    beast::error_code ec;
    if (std::holds_alternative<beast::tcp_stream>(d->socket)) {
        std::get<beast::tcp_stream>(d->socket).socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            COMPLOG_ERROR("Error disconnecting:", ec.message());
            return false;
        }
        std::get<beast::tcp_stream>(d->socket).close();

    } else if (std::holds_alternative<ssl::stream<tcp::socket> >(d->socket)) {
        std::get<ssl::stream<tcp::socket> >(d->socket).shutdown(ec);
        if (ec == net::error::eof ||
            ec == net::ssl::error::stream_truncated ||
            ec == boost::asio::error::operation_aborted) {
            ec = {};
        }
        if (ec && ec != boost::asio::error::not_connected) {
            COMPLOG_ERROR("Error in SSL shutdown:", ec.message());
            return false;
        }


        std::get<ssl::stream<tcp::socket> >(d->socket).lowest_layer().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected) {
            COMPLOG_ERROR("Error disconnecting:", ec.message());
            return false;
        }
        std::get<ssl::stream<tcp::socket> >(d->socket).lowest_layer().close();
    }
    COMPLOG_OK("Disconnected");
    return true;
}

}
