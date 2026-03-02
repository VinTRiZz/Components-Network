#include "server.hpp"

#include <boost/asio.hpp>
#include <thread>
#include <iostream>

#include <Components/Logger/Logger.h>

namespace UDP
{

struct Server::Impl {
    std::array<char, 65507> datagramBuffer;

    boost::asio::io_context ioContext;
    boost::asio::ip::udp::socket socket;
    boost::asio::ip::udp::endpoint senderEndpoint;

    RequestProcessor requestProcessor;
    std::thread ioThread;

    std::atomic<bool> isRunning {false};
    
    Impl() : socket(ioContext), isRunning(false) {}
    
    ~Impl() {
        stop();
    }
    
    void startReceive() {
        socket.async_receive_from(
            boost::asio::buffer(datagramBuffer),
            senderEndpoint,
            [this](boost::system::error_code error, std::size_t bytesReceived) {
                if (!error && bytesReceived > 0 && requestProcessor) {
                    std::vector<uint8_t> data(bytesReceived);
                    std::copy(datagramBuffer.data(), datagramBuffer.data() + bytesReceived, data.data());
                    requestProcessor(std::move(data));
                }
                
                if (isRunning.load(std::memory_order_acquire)) {
                    startReceive();
                }
            }
        );
    }
    
    void run() {
        try {
            ioContext.run();
        }
        catch (const std::exception& e) {
            COMPLOG_ERROR("[UDP] Server error:", e.what());
        }
    }
    
    void stop() {
        isRunning.store(false, std::memory_order_release);
        ioContext.stop();
        
        if (ioThread.joinable()) {
            ioThread.join();
        }
        
        if (socket.is_open()) {
            socket.close();
        }
    }
};

Server::Server() : d(std::make_unique<Impl>())
{

}

Server::~Server() {
    d->stop();
}

bool Server::start(uint16_t port) {
    try {
        if (d->isRunning.load(std::memory_order_acquire)) {
            return false;
        }
        
        d->socket.open(boost::asio::ip::udp::v4());
        d->socket.bind(
            boost::asio::ip::udp::endpoint(
                boost::asio::ip::address_v4::any(),
                port
            )
        );
        
        d->isRunning.store(true, std::memory_order_release);
        d->startReceive();
        
        d->ioThread = std::thread([this]() {
            d->run();
        });
        
        COMPLOG_OK("[UDP] Started server on port", port);
        return true;
    }
    catch (const std::exception& e) {
        COMPLOG_ERROR("[UDP] Failed to start server:", e.what());
        return false;
    }
}

bool Server::isWorking() const
{
    return d->isRunning.load(std::memory_order_acquire);
}

void Server::stop() {
    d->stop();
}

void Server::setRequestProcessor(RequestProcessor &&processor) {
    d->requestProcessor = std::move(processor);
}

}
