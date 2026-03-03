#pragma once

#include <memory>
#include <string>
#include <future>
#include <cstdint>
#include <vector>
#include <boost/noncopyable.hpp>

namespace WebSockets {

enum class DisconnectReason {
    Normal,           // нормальное закрытие
    ServerClosed,     // сервер закрыл соединение
    ConnectionLost,   // потеря соединения
    ProtocolError,    // ошибка протокола
};

class Client : public boost::noncopyable {
public:
    Client();
    ~Client();

    /**
     * @brief connect   Подключиться к серверу
     * @param host      IP
     * @param port      Порт сервера
     */
    void connect(const std::string& host, uint16_t port);

    /**
     * @brief sendText  Отправить текстовые данные
     * @param data      Текст
     * @return          true в случае отправки
     */
    bool sendText(std::string&& data);

    /**
     * @brief sendJson  Отправить JSON данные
     * @param data      сериализованные в JSON данные
     * @return          true в случае отправки
     */
    bool sendJson(std::string&& data);

    /**
     * @brief sendBinary    Отправить байты
     * @param data          Данные
     * @return              true в случае отправки
     */
    bool sendBinary(std::vector<uint8_t>&& data);

    /**
     * @brief setReceiveCallback    Задать колбек для полученных текстовых и JSON данных
     * @param cbk
     */
    void setReceiveCallback(std::function<void(std::string&&)>&& cbk);

    /**
     * @brief setReceiveByteCallback    Задать колбек для полученных байтов
     * @param cbk
     */
    void setReceiveByteCallback(std::function<void(std::vector<uint8_t>&&)>&& cbk);

    /**
     * @brief isConnected   Проверка подключения
     * @return              true если клиент подключен
     */
    bool isConnected() const;

    /**
     * @brief ping      Пинг сервера
     * @param bytes     Количество байт для пинга. Не должно быть меньше 8
     * @param timeoutMs
     * @return          Фьючерс для ожидания пинга. Если его значение будет -1, то пинг неудачен, иначе -- время ответа в мс
     * @exception       std::invalid_argument при bytes < 8
     */
    std::future<int> ping(size_t bytes = 8, int timeoutMs = 1000);

    /**
     * @brief disconnect    Отключение от сервера
     * @param reason
     */
    void disconnect(DisconnectReason reason = DisconnectReason::Normal);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace WebSockets

