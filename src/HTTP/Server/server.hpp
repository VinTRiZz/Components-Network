#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>

#include "../Common/httptypes.hpp"

namespace HTTP
{

struct SecureConnectionParameters
{
    std::string certFile {};
    std::string privKeyFile {};
};

class Server
{
public:
    Server(const std::string& serverName, const SecureConnectionParameters &securePars = {});
    ~Server();

    void setGetHandler(const std::string& target, ProcessingCallback&& cbk);
    void setPostHandler(const std::string& target, ProcessingCallback&& cbk);
    void setPutHandler(const std::string& target, ProcessingCallback&& cbk);
    void setDeleteHandler(const std::string& target, ProcessingCallback&& cbk);

    void start(uint16_t port, uint16_t threadCount = 1);
    void stop();
    bool isRunning() const;

private:
    std::unordered_map<std::string, std::map<HttpPacket::MethodType, ProcessingCallback> > m_processors;
    SecureConnectionParameters m_httpsParameters;

    struct Impl;
    std::unique_ptr<Impl> d;

    std::string m_serverName;
};


}
