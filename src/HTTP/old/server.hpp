#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <memory>
#include <string>
#include <functional>

namespace HTTPOld
{

const auto serverName {"ManagementServer"};

class Server
{
public:
    Server();
    ~Server();

    void start(uint16_t port);
    void stop();
    bool isRunning() const;

    std::string serverPagesDir() const;
    uint16_t serverPort() const;

    enum class ResponseType {
        ErrorText,
        PlainText,
        HTML,
        JSON
    };
    enum class ResponseResult {
        Ok,
        NotFound,
        BadRequest,
        NotImplemented,
        NoContent,
        Unauthorized
    };
    struct PacketMeta
    {
        // Processing result information
        ResponseResult responseResult   {ResponseResult::BadRequest};
        ResponseType responseType       {ResponseType::PlainText};

        std::string sessionId;

        std::string target;
        std::string body;
    };
    void setProcessorGet(const std::function<void(PacketMeta&)>& proc);
    void setProcessorPost(const std::function<void(PacketMeta&)>& proc);
    void setProcessorPut(const std::function<void(PacketMeta&)>& proc);
    void setProcessorDelete(const std::function<void(PacketMeta&)>& proc);

private:
    bool m_isRunning {false};
    uint16_t m_port {9001};

    struct Impl;
    std::unique_ptr<Impl> d;
};

}

#endif // HTTP_SERVER_H
