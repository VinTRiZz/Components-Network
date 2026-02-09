#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include <Components/Network/CommonHTTP.h>
#include <Components/Network/ClientHTTP.h>

namespace HTTP {

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }

    ~ThreadPool();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};


// Сессия соединения
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, ThreadPool& pool, RequestHandler handler);

    void start();

private:
    void read_request();
    void process_request();
    void send_response(HTTPPacket&& packet);

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    ThreadPool& pool_;
    RequestHandler request_handler_;
};

class Server {
public:
    Server();

    bool start(uint16_t port, const std::string& address = "0.0.0.0");
    void stop();

    void setRequestHandler(RequestHandler handler);
    void setErrorCallback(ErrorCallback callback);

    void setResponseHeader(const std::string& key, const std::string& value);
    void setResponseBody(const std::string& body);
    void setResponseStatus(int status);

    Client& getClient();

private:
    void accept_connection();

    asio::io_context ioc_;
    tcp::acceptor acceptor_;
    ThreadPool pool_;
    Client client_;

    RequestHandler request_handler_;
    ErrorCallback error_callback_;
    HTTPPacket response_packet_;

    std::thread server_thread_;
};

} // namespace HTTP

