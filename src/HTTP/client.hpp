#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include <Components/Network/CommonHTTP.h>

namespace HTTP {

class Client {
public:
    explicit Client(asio::io_context& ioc);

    ~Client();

    bool setHost(const std::string& host, uint16_t port);
    void setTimeout(int seconds);

    void setMethod(http::verb method);
    void setTarget(const std::string& target);
    void setBody(const std::string& body);
    void setHeader(const std::string& key, const std::string& value);
    void setContentType(const std::string& type);
    void setUserAgent(const std::string& agent);
    void setAuthorization(const std::string& token);

    bool sendData(HTTPPacket&& packet);
    const HTTPPacket& getLastResponse() const;

    void setErrorCallback(ErrorCallback callback);

    void close();

private:
    ServerInfo m_serverInfo;
    HTTPPacket m_currentRequest;
    HTTPPacket m_lastResponse;

    tcp::resolver m_resolver;
    beast::tcp_stream m_sendStream;
    asio::steady_timer m_sendTimer;

    ErrorCallback m_errorCallback;
    std::mutex m_mutex;
};

} // namespace HTTP

