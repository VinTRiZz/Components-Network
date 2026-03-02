#pragma once

#include "common.hpp"
#include <memory>

namespace UDP
{

/**
 * @brief The Client class  Инстанция UDP клиента
 */
class Client {
public:
    Client();
    ~Client();

    /**
     * @brief setHost   Задать хост, которому будут отправляться датаграммы
     * @param host      Для broadcast режима 255.255.255.255 или эквивалент
     * @param port
     * @return          true в случае успешного задания
     */
    bool setHost(const std::string& host, uint16_t port);

    /**
     * @brief enableBroadcast   Задать режим Broadcast для отправки сообщений
     * @param enable
     * @return                  true в случае успешного включения
     */
    bool enableBroadcast(bool enable = true);

    /**
     * @brief sendData  Отправить данные на хост
     * @param data
     * @return          true при успешной отправке
     */
    bool sendData(std::string&& data);

    /**
     * @brief setErrorCallback  Задать колбек для обработки ошибок
     * @param callback
     */
    void setErrorCallback(ErrorCallback callback);
    
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

}
