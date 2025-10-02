#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>

#include <variant>

#include "../Common/httptypes.hpp"

namespace HTTP
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

class Client
{
    tcp::resolver m_resolver;
    std::variant<beast::tcp_stream, ssl::stream<tcp::socket> > m_socket;
    std::shared_ptr<ssl::context> m_ctx;

    std::string m_host;
    std::string m_port;

    std::string m_clientName {"TestApp"};
    uint32_t m_maxFileSize {1024 * 1024 * 1024};

    bool connectToHost();
    bool isConnected();
    bool disconnectFromHost();

public:
    Client(net::io_context& ioc, bool isSecure = false, bool verifyCertificate = true);
    ~Client();

    void setClientName(const std::string& clientName);
    void setMaxFileSize(uint32_t fileSizeByte);

    void setHost(const std::string& host, const std::string port = "80");
    HttpPacket request(HttpPacket::MethodType method, const HttpPacket& pkt);

    bool downloadFile(const std::string& target, const std::string& saveFilePath);
    bool uploadFile(const std::string& target, const std::string& filePath);
};

}
