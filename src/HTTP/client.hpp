#pragma once

#include <memory>
#include <functional>
#include <optional>

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

    void setLoggingEnabled(bool isEn);

    void setClientName(const std::string& clientName);
    void setMaxFileSize(uint32_t fileSizeByte);

    void setHost(const std::string& host, const uint16_t port = 80);
    Packet request(MethodType method, Packet &&pkt);
    Packet request(MethodType method, const Packet &pkt);

    void requestAsync(MethodType method, Packet&& pkt, std::function<void(std::optional<Packet>&&)>&& cbk);
    void interruptRequestProcessing();

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
