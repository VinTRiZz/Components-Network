#include "transportserver.h"

#include "connectionsession.h"

#include <boost/thread.hpp>

#include <boost/asio.hpp>
#include <execinfo.h>

#include <memory>
#include <string>

#include <Components/Logger/Logger.h>


using boost::asio::ip::tcp;

namespace TransportLevel
{

struct TransportServerInstance::Impl
{
    uint16_t port {9020};
    ProcessCallbackT m_responseCallback;

    std::vector<std::string> tokenList;
    uint64_t threadCount {1};
    boost::thread_group threadPool;
    boost::asio::io_context ioContext;
    std::shared_ptr<tcp::acceptor> pAcceptor;

    void handleConnections()
    {
        pAcceptor->async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                COMPLOG_OK("Connected client");
                std::make_shared<ConnectionSession>(std::move(socket), m_responseCallback, tokenList)->readSocket(ioContext);
            } else {
                COMPLOG_ERROR("Client connection error");
            }
            handleConnections();
        });
    }
};

TransportServerInstance::TransportServerInstance() :
    d {new Impl}
{

}

TransportServerInstance::~TransportServerInstance()
{

}

void TransportServerInstance::setPort(uint16_t portNo)
{
    d->port = portNo;
}

void TransportServerInstance::setThreadCount(uint64_t threadCount)
{
    d->threadCount = threadCount;
}

void TransportServerInstance::loadTokens(const std::string &filePath)
{
    // TODO: Load lines
    d->tokenList.push_back("DEVELOP_TOKEN");
}

int TransportServerInstance::init(const ProcessCallbackT &procCallback)
{
    d->m_responseCallback = procCallback;
    d->pAcceptor = std::shared_ptr<tcp::acceptor>(new tcp::acceptor(d->ioContext, tcp::endpoint(tcp::v4(), d->port)));
    d->pAcceptor->set_option(boost::asio::socket_base::reuse_address(true));
    d->handleConnections();
    return 0;
}

int TransportServerInstance::start()
{
    COMPLOG_INFO("Starting server");
    for (uint64_t i = 0; i < d->threadCount; i++) {
        d->threadPool.create_thread([this](){
            d->ioContext.run();
        });
    }
    d->threadPool.join_all();
    COMPLOG_INFO("Server exited normally");
    return 0;
}

}
