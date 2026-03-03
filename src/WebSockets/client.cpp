#include "client.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <iostream>

#include <Components/Logger/Logger.h>

namespace WebSockets {

// ========================== pImpl implementation ==========================
struct Client::Impl {
    using Client = websocketpp::client<websocketpp::config::asio_client>;
    using ConnectionHdl = websocketpp::connection_hdl;
    using MessagePtr = websocketpp::config::asio_client::message_type::ptr;

    Client client;
    std::unique_ptr<std::thread> ioThread;
    websocketpp::lib::asio::io_service ioService;
    ConnectionHdl connection;
    bool connected {false};
    mutable std::mutex mutex;

    std::function<void (std::string &&)> stringDataProcessor;
    std::function<void (std::vector<uint8_t> &&)> byteDataProcessor;

    struct PendingPing {
        std::promise<int> promise;
        std::unique_ptr<websocketpp::lib::asio::steady_timer> timer;
    };
    std::map<uint64_t, PendingPing> pendingPings;
    uint64_t nextPingId = 0;
    std::mutex pingMutex;

    Impl() {
        client.init_asio(&ioService);
        setHandlers();
    }

    ~Impl() {
        disconnect(DisconnectReason::Normal);
        if (ioThread && ioThread->joinable()) {
            ioService.stop();
            ioThread->join();
        }
    }

    void setHandlers() {
        client.set_open_handler([this](ConnectionHdl hdl) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                connection = hdl;
                connected = true;
            }
            COMPLOG_OK("[WS] Connected to server");

            // Отправка двух обязательных сообщений: текст и JSON
            try {
                client.send(hdl, "hello", websocketpp::frame::opcode::text);
                nlohmann::json j;
                j["test"] = "hello";
                client.send(hdl, j.dump(), websocketpp::frame::opcode::text);
            } catch (const std::exception& e) {
                COMPLOG_ERROR("[WS] Failed to send initial messages:", e.what());
            }
        });


        client.set_message_handler([this](ConnectionHdl hdl, MessagePtr msg) {
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string payload = msg->get_payload();
                COMPLOG_DEBUG("[WS] Text got:", payload);
//                try {
//                    auto j = nlohmann::json::parse(payload);
//                } catch (nlohmann::json::exception& ex) {

//                }
            } else {
                COMPLOG_INFO("[WS] Binary message received, size:", msg->get_payload().size());
            }
        });


        client.set_close_handler([this](ConnectionHdl hdl) {
            DisconnectReason reason = DisconnectReason::ServerClosed; // по умолчанию
            websocketpp::close::status::value code;
            std::string reasonStr;
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto con = client.get_con_from_hdl(hdl);
                code = con->get_remote_close_code();
                reasonStr = con->get_remote_close_reason();
                connected = false;
            }
            // Преобразование кода закрытия в enum DisconnectReason (упрощённо)
            if (code == websocketpp::close::status::normal) {
                reason = DisconnectReason::Normal;
            } else if (code == websocketpp::close::status::going_away) {
                reason = DisconnectReason::ServerClosed;
            } else if (code == websocketpp::close::status::protocol_error) {
                reason = DisconnectReason::ProtocolError;
            } else {
                reason = DisconnectReason::ConnectionLost;
            }
            COMPLOG_WARNING("[WS] Disconnected:", reasonStr, "code:", code);
        });


        client.set_fail_handler([this](ConnectionHdl hdl) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                connected = false;
            }
            auto con = client.get_con_from_hdl(hdl);
            auto ec = con->get_ec();
            COMPLOG_ERROR("[WS] Connection failed:", ec.message());
        });


        client.set_pong_handler([this](ConnectionHdl hdl, std::string payload) {
            if (payload.size() >= sizeof(uint64_t)) {
                uint64_t id;
                std::memcpy(&id, payload.data(), sizeof(id));
                std::lock_guard<std::mutex> lock(pingMutex);
                auto it = pendingPings.find(id);
                if (it != pendingPings.end()) {
                    it->second.promise.set_value(1); // здесь можно вычислить RTT, но для примера ставим 1
                    it->second.timer->cancel();
                    pendingPings.erase(it);
                }
            }
        });
    }

    void disconnect(DisconnectReason reason) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!connected) return;

        websocketpp::close::status::value code = websocketpp::close::status::normal;
        switch (reason) {
            case DisconnectReason::Normal:        code = websocketpp::close::status::normal; break;
            case DisconnectReason::ServerClosed:  code = websocketpp::close::status::going_away; break;
            case DisconnectReason::ProtocolError: code = websocketpp::close::status::protocol_error; break;
            case DisconnectReason::ConnectionLost:code = websocketpp::close::status::abnormal_close; break;
        }

        try {
            client.close(connection, code, "Disconnect by user");
        } catch (const std::exception& e) {
            COMPLOG_ERROR("[WS] Error during disconnect:", e.what());
        }
        connected = false;
    }
};


Client::Client() : d(std::make_unique<Impl>()) {}

Client::~Client() = default;

void Client::connect(const std::string& host, uint16_t port) {
    std::string uri = "ws://" + host + ":" + std::to_string(port);
    websocketpp::lib::error_code ec;
    auto con = d->client.get_connection(uri, ec);
    if (ec) {
        COMPLOG_ERROR("[WS] Connection error:", ec.message());
        return;
    }
    d->client.connect(con);

    if (!d->ioThread) {
        d->ioThread = std::make_unique<std::thread>([this]() {
            d->client.run();
        });
    }
}

bool Client::sendText(std::string &&data)
{

}

bool Client::sendJson(std::string &&data)
{

}

bool Client::sendBinary(std::vector<uint8_t> &&data)
{

}

void Client::setReceiveCallback(std::function<void (std::string &&)> &&cbk)
{

}

void Client::setReceiveByteCallback(std::function<void (std::vector<uint8_t> &&)> &&cbk)
{

}

bool Client::isConnected() const {
    std::lock_guard<std::mutex> lock(d->mutex);
    return d->connected;
}

std::future<int> Client::ping(size_t bytes, int timeoutMs) {
    if (!isConnected()) {
        std::promise<int> p;
        p.set_value(-1);
        return p.get_future();
    }

    uint64_t id {0};
    if (bytes >= sizeof(id)) {
        throw std::invalid_argument("Invalid ping bytes count (must be >=8)");
    }

    std::promise<int> promise;
    auto future = promise.get_future();
    {
        std::lock_guard<std::mutex> lock(d->pingMutex);
        id = d->nextPingId++;
        auto& pending = d->pendingPings[id];
        pending.promise = std::move(promise);
        pending.timer = std::make_unique<websocketpp::lib::asio::steady_timer>(d->ioService);
        pending.timer->expires_after(std::chrono::milliseconds(timeoutMs));
        pending.timer->async_wait([this, id](const websocketpp::lib::asio::error_code& ec) {
            if (ec) return; // таймер был отменён
            std::lock_guard<std::mutex> lock(d->pingMutex);
            auto it = d->pendingPings.find(id);
            if (it != d->pendingPings.end()) {
                it->second.promise.set_value(-1); // таймаут
                d->pendingPings.erase(it);
            }
        });
    }
    std::string payload(bytes, '\0');
    std::copy_n(&payload[0], sizeof(id), &id);

    websocketpp::lib::error_code ec;
    d->client.ping(d->connection, payload, ec);
    if (ec) {
        // Ping send error
        std::lock_guard<std::mutex> lock(d->pingMutex);
        auto it = d->pendingPings.find(id);
        if (it != d->pendingPings.end()) {
            it->second.promise.set_value(-1);
            d->pendingPings.erase(it);
        }
    }

    return future;
}

void Client::disconnect(DisconnectReason reason) {
    d->disconnect(reason);
}

} // namespace WebSockets
