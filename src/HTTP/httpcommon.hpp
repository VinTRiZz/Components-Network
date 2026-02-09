#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <vector>

namespace HTTP
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

enum class ErrorType {
    CONNECTION_FAILED,
    RESOLVE_FAILED,
    WRITE_FAILED,
    READ_FAILED,
    TIMEOUT,
    INVALID_RESPONSE,
    BIND_FAILED,
    ACCEPT_FAILED,
    UNKNOWN
};

struct HTTPPacket;
using ErrorCallback = std::function<void(ErrorType, const std::string&)>;
using RequestHandler = std::function<void(HTTPPacket&&, std::function<void(HTTPPacket&&)>)>;

struct ServerInfo {
    std::string host;
    uint16_t port;
    std::string scheme; // http или https
    int timeout_seconds = 30;

    std::string getFullUrl() const {
        return scheme + "://" + host + ":" + std::to_string(port);
    }
};

struct HTTPPacket {
    http::verb method = http::verb::get;
    std::string target = "/";
    int version = 11; // HTTP/1.1
    std::string body;
    std::map<std::string, std::string> headers;

    void setHeader(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    std::string getHeader(const std::string& key) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }

    bool hasHeader(const std::string& key) const {
        return headers.find(key) != headers.end();
    }
};

}
