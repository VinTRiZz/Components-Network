#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ssl.hpp>

#include <map>
#include <variant>

#include "httptypes.hpp"

namespace HTTP
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

class ConnectionSession : public std::enable_shared_from_this<ConnectionSession>
{
public:
    ConnectionSession(const std::string& selfServerName,
                      tcp::socket &&sock,
                      const std::shared_ptr<net::ssl::context>& ctx);
    ~ConnectionSession();

    void handleRequests();
    void sendResponse(const Packet& pkt, http::verb method);
    void sendErrorResponse(http::status status, const std::string& errorString);
    bool isConnected() const;

    std::map<MethodType, RequestProcessor> m_processors;

private:
    std::variant<beast::tcp_stream, net::ssl::stream<tcp::socket> > m_socket;
    std::shared_ptr<net::ssl::context> m_ctx;

    beast::flat_buffer                  m_buffer {32768};
    http::request<http::dynamic_body>   m_request;
    std::variant<
        http::response<http::dynamic_body>,
        http::response<http::file_body> >   m_response;
    Packet                              m_responseErrorPacket;

    std::string m_selfServerName {"Unknown server"};
    std::shared_ptr<net::steady_timer> m_deadlineTimer;

    void checkDeadline();
    void closeConnection();
};

}
