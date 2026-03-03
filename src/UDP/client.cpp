#include "client.hpp"

#include <boost/asio.hpp>
#include <iostream>

#include <Components/Logger/Logger.h>

namespace UDP
{

struct Client::Impl {
    std::pair<std::string, uint16_t> host;

    boost::asio::io_context ioContext;
    boost::asio::ip::udp::socket socket;
    boost::asio::ip::udp::endpoint serverEndpoint;

    ErrorCallback errorCallback;
    
    Impl() : socket(ioContext) {
        socket.open(boost::asio::ip::udp::v4());
    }
    
    ~Impl() {
        if (socket.is_open()) {
            socket.close();
        }
    }
    
    void handleError(ErrorType errorType, const std::string& message) {
        if (errorCallback) {
            errorCallback(errorType, message);
        }
    }

    template <typename SendT>
    bool sendData(SendT&& iData) {
        if (!socket.is_open()) {
            handleError(ErrorType::SendError,
                           "Socket is closed");
            return false;
        }

        if (host.second == 0) {
            handleError(ErrorType::SendError,
                           "Host did not set");
            return false;
        }

        try {

            size_t bytesSent = socket.send_to(
                        boost::asio::buffer(iData.data(), iData.size()),
                serverEndpoint
            );

            return bytesSent == iData.size();
        }
        catch (const std::exception& e) {
            handleError(ErrorType::SendError, e.what());
            return false;
        }
    }
};

Client::Client() :
    d{new Impl}
{
    d->errorCallback = [](auto errType, const auto& errMsg) -> void {
        COMPLOG_ERROR("[UDP] client error:", errMsg);
    };
}

Client::~Client() = default;

bool Client::setHost(const std::string& host, uint16_t port) {
    try {
        d->host = std::make_pair(host, port);
        
        boost::asio::ip::udp::resolver resolver(d->ioContext);
        boost::asio::ip::udp::resolver::query query(
            boost::asio::ip::udp::v4(),
            host,
            std::to_string(port)
        );
        
        auto endpoints = resolver.resolve(query);
        if (endpoints.empty()) {
            d->handleError(ErrorType::ResolutionError,
                              "Failed to resolve host: " + host);
            return false;
        }
        
        d->serverEndpoint = *endpoints.begin();
        return true;
    }
    catch (const std::exception& e) {
        d->handleError(ErrorType::ResolutionError, e.what());
        return false;
    }
}

bool Client::enableBroadcast(bool enable) {
    try {
        boost::asio::socket_base::broadcast option(enable);
        d->socket.set_option(option);
        return true;
    }
    catch (const std::exception& e) {
        d->handleError(ErrorType::ConnectionError,
                       std::string("Failed to set broadcast option: ") + e.what());
        return false;
    }
}

bool Client::sendData(std::string&& data) {
    return d->sendData(data);
}

bool Client::sendData(const std::string &data)
{
    return d->sendData(data);
}

bool Client::sendByteData(std::vector<uint8_t> &&data)
{
    return d->sendData(data);
}

bool Client::sendByteData(const std::vector<uint8_t> &data)
{
    return d->sendData(data);
}

void Client::setErrorCallback(ErrorCallback callback) {
    d->errorCallback = std::move(callback);
}

}
