#pragma once

#include <memory>

#include "common.hpp"

namespace UDP
{

/**
 * @brief The Server class  Инстанция UDP сервера
 */
class Server {
public:
    Server();
    ~Server();

    bool start(uint16_t port);
    bool isWorking() const;
    void stop();

    /**
     * @brief setRequestProcessor   Задать обработчик для запросов
     * @param processor
     */
    void setRequestProcessor(RequestProcessor&& processor);
    
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

}
