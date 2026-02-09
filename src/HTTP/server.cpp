#include "server.hpp"

namespace HTTP {

ThreadPool::ThreadPool(size_t threads) : stop_(false) {
    for(size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex_);
                    condition_.wait(lock, [this] {
                        return stop_ || !tasks_.empty();
                    });

                    if(stop_ && tasks_.empty()) return;

                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for(std::thread &worker: workers_)
        worker.join();
}



Session::Session(tcp::socket socket, ThreadPool &pool, RequestHandler handler)
    : socket_(std::move(socket))
    , pool_(pool)
    , request_handler_(std::move(handler))
{}

void Session::start() {
    read_request();
}

void Session::read_request() {
    auto self = shared_from_this();

    http::async_read(socket_, buffer_, request_,
                     [self](beast::error_code ec, std::size_t bytes_transferred) {
        if(!ec) {
            self->process_request();
        }
    });
}

void Session::process_request() {
    // В этом месте можно добавлять проверки полей входящих пакетов
    // перед обработкой. Например:
    // 1. Проверка обязательных заголовков
    // 2. Валидация метода
    // 3. Проверка размера тела
    // 4. Аутентификация по заголовкам

    // Преобразование запроса в HTTPPacket
    HTTPPacket packet;
    packet.method = request_.method();
    packet.target = std::string(request_.target());
    packet.version = request_.version();
    packet.body = beast::buffers_to_string(request_.body().data());

    for(const auto& field : request_) {
        packet.setHeader(std::string(field.name_string()),
                         std::string(field.value()));
    }

    // Передача в пул потоков для обработки
    pool_.enqueue([this, packet = std::move(packet)]() mutable {
        request_handler_(std::move(packet),
                         [this](HTTPPacket&& response_packet) {
            send_response(std::move(response_packet));
        });
    });
}

void Session::send_response(HTTPPacket &&packet) {
    auto self = shared_from_this();

    // Создание HTTP ответа
    http::response<http::string_body> res{
        static_cast<http::status>(std::stoi(packet.getHeader("status").empty() ? packet.getHeader("status") : "200")),
                packet.version
    };

    res.body() = packet.body;
    res.prepare_payload();

    // Установка заголовков
    for(const auto& [key, value] : packet.headers) {
        if (key != "status") {
            res.set(key, value);
        }
    }

    res.set(http::field::server, "Boost.Beast HTTP Server");

    http::async_write(socket_, res,
                      [self](beast::error_code ec, std::size_t) {
        self->socket_.shutdown(tcp::socket::shutdown_send, ec);
    });
}



Server::Server()
    : acceptor_(ioc_)
    , pool_(std::thread::hardware_concurrency())
    , client_(ioc_)
{}

bool Server::start(uint16_t port, const std::string &address) {
    try {
        auto const endpoint = tcp::endpoint(
                    asio::ip::make_address(address), port
                    );

        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        server_thread_ = std::thread([this]() {
            accept_connection();
            ioc_.run();
        });

        return true;
    } catch (const std::exception& e) {
        if(error_callback_) {
            error_callback_(ErrorType::BIND_FAILED, e.what());
        }
        return false;
    }
}

void Server::stop() {
    ioc_.stop();
    if(server_thread_.joinable()) {
        server_thread_.join();
    }
}

void Server::setRequestHandler(RequestHandler handler) {
    request_handler_ = std::move(handler);
}

void Server::setErrorCallback(ErrorCallback callback) {
    error_callback_ = std::move(callback);
}

void Server::setResponseHeader(const std::string &key, const std::string &value) {
    response_packet_.setHeader(key, value);
}

void Server::setResponseBody(const std::string &body) {
    response_packet_.body = body;
}

void Server::setResponseStatus(int status) {
    response_packet_.setHeader("status", std::to_string(status));
}

Client &Server::getClient() {
    return client_;
}

void Server::accept_connection() {
    acceptor_.async_accept(
                [this](beast::error_code ec, tcp::socket socket) {
        if(!ec) {
            std::make_shared<Session>(
                        std::move(socket), pool_, request_handler_
                        )->start();
        }
        accept_connection();
    });
}

} // namespace HTTP
