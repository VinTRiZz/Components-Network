#include "server.hpp"

#include <Components/Logger/Logger.h>

#include "connectionsession.hpp"

#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>

namespace HTTP
{

struct Server::Impl
{
    std::string m_serverName;

    boost::asio::io_context m_ioc;
    boost::asio::ip::tcp::socket m_socket;
    boost::asio::ip::tcp::acceptor m_acceptor;

    std::shared_ptr<boost::asio::ssl::context> ctx;

    Impl(const std::string& srv,
         const boost::asio::ip::address& addr,
         const uint16_t port,
         int threadCount,
         const SecureConnectionParameters& securePars) :
        m_serverName{srv}, m_ioc {threadCount}, m_socket {m_ioc}, m_acceptor {m_ioc, {addr, port}}
    {
        if (!securePars.certFile.empty()) {
            ctx = std::make_unique<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv13_server);
            ctx->use_certificate_chain_file(securePars.certFile);
            ctx->use_private_key_file(securePars.privKeyFile, boost::asio::ssl::context::pem);
        }
    }

    void handleConnections(std::unordered_map<std::string, std::map<HttpPacket::MethodType, ProcessingCallback> >& processors) {
        m_acceptor.async_accept(m_socket,
            [&](beast::error_code ec) {
                if(ec) {
                    LOG_WARNING("Error accepting connection");
                } else {
                    auto pConnection = std::make_shared<ConnectionSession>(m_serverName, std::move(m_socket), ctx);

                    auto handleMethod = [this, pConnection, &processors](const HttpPacket& pkt, HttpPacket::MethodType methType){
                        auto targetProcessor = processors.find(pkt.target);
                        if (targetProcessor == processors.end()) {
                            LOG_WARNING("No processors set for method:", HttpPacket::toString(methType), "and target:", pkt.target);
                            pConnection->sendErrorResponse(http::status::not_implemented, "501 - Not implemented");
                            return;
                        }

                        auto targetMethodProcessor = targetProcessor->second.find(methType);
                        if (targetMethodProcessor == targetProcessor->second.end()) {
                            LOG_WARNING("Skipped packet of method:", HttpPacket::toString(methType), "and target:", pkt.target);
                            pConnection->sendErrorResponse(http::status::not_found, "404 - Not found");
                            return;
                        }

                        targetMethodProcessor->second(pkt,
                            [pConnection](const HttpPacket& pkt){
                            pConnection->sendResponse(pkt, http::verb::get);
                        });
                    };

                    auto addMethod = [pConnection, &handleMethod](HttpPacket::MethodType methType){
                        pConnection->m_processors[methType] = [methType, handleMethod](const HttpPacket& pkt){
                            return handleMethod(pkt, methType);
                        };
                    };

                    addMethod(HttpPacket::Get);
                    addMethod(HttpPacket::Put);
                    addMethod(HttpPacket::Post);
                    addMethod(HttpPacket::Delete);
                    pConnection->handleRequests();
                }
                handleConnections(processors);
        });
    }
};

Server::Server(const std::string &serverName,
               const SecureConnectionParameters& securePars) :
    m_serverName {serverName},
    m_httpsParameters{securePars}
{

}

Server::~Server()
{
    stop();
}

void Server::setGetHandler(const std::string &target, ProcessingCallback &&cbk)
{
    m_processors[target][HttpPacket::MethodType::Get] = cbk;
    LOG_OK("Registered handler for GET", target);
}

void Server::setPostHandler(const std::string &target, ProcessingCallback &&cbk)
{
    m_processors[target][HttpPacket::MethodType::Post] = cbk;
    LOG_OK("Registered handler for POST", target);
}

void Server::setPutHandler(const std::string &target, ProcessingCallback &&cbk)
{
    m_processors[target][HttpPacket::MethodType::Put] = cbk;
    LOG_OK("Registered handler for PUT", target);
}

void Server::setDeleteHandler(const std::string &target, ProcessingCallback &&cbk)
{
    m_processors[target][HttpPacket::MethodType::Delete] = cbk;
    LOG_OK("Registered handler for DELETE", target);
}

void Server::start(uint16_t port, uint16_t threadCount)
{
    LOG_INFO("Starting server [", m_serverName, "]", m_httpsParameters.certFile.empty() ? "(HTTP)" : "(HTTPS)");
    d = std::make_unique<Impl>(m_serverName,
                               boost::asio::ip::make_address(std::string("0.0.0.0")),
                               port,
                               threadCount,
                               m_httpsParameters);

    d->handleConnections(m_processors);
    try {
        d->m_ioc.run();
    } catch (const std::exception& ex) {
        LOG_ERROR_SYNC("Server exception:", ex.what());
    }
}

void Server::stop()
{
    LOG_INFO("Requesting server stop...");
    if (isRunning()) {
        d->m_acceptor.cancel();
        d->m_socket.close();
        d.reset();
    }
}

bool Server::isRunning() const
{
    if (!d) {
        return !d->m_ioc.stopped();
    }
    return false;
}

}
