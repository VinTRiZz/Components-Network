#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <boost/noncopyable.hpp>

namespace WebSockets {

enum class DisconnectReason {
    Normal,           // нормальное закрытие клиентом
    ServerShutdown,   // сервер остановлен
    ProtocolError,    // ошибка протокола
    ConnectionLost,   // потеря соединения
    Unknown
};

class Server : public boost::noncopyable {
public:
    Server();
    ~Server();

    // Запуск сервера на указанном хосте и порту
    void listen(const std::string& host, uint16_t port);

    // Остановка сервера и закрытие всех соединений
    void stop();

    // Проверка состояния (активно ли прослушивание)
    bool isListening() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};


} // namespace WebSockets
