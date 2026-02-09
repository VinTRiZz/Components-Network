#pragma once

#include <memory>

#include "httptypes.hpp"

namespace boost::asio
{
class io_context;
}

namespace HTTP
{

class Client
{
public:
    Client(boost::asio::io_context& ioc, bool isSecure = false, bool verifyCertificate = true);
    Client(bool isSecure = false, bool verifyCertificate = true);
    ~Client();

    void setClientName(const std::string& clientName);
    void setMaxFileSize(uint32_t fileSizeByte);

    void setHost(const std::string& host, const uint16_t port = 80);
    Packet request(MethodType method, Packet &&pkt);
    Packet request(MethodType method, const Packet &pkt);

    bool downloadFile(const std::string& target, const std::string& saveFilePath);
    bool uploadFile(const std::string& target, const std::string& filePath);

private:
    struct Impl;
    std::shared_ptr<Impl> d;

    bool connectToHost();
    bool isConnected();
    bool disconnectFromHost();
};

}
