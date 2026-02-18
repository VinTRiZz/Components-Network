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

    Client m_client;
    std::unique_ptr<std::thread> m_ioThread;
    websocketpp::lib::asio::io_service m_ioService;
    ConnectionHdl m_connection;
    bool m_connected {false};
    mutable std::mutex m_mutex;

    struct PendingPing {
        std::promise<int> promise;
        std::unique_ptr<websocketpp::lib::asio::steady_timer> timer;
    };
    std::map<uint64_t, PendingPing> m_pendingPings;
    uint64_t m_nextPingId = 0;
    std::mutex m_pingMutex;

    Impl() {
        m_client.init_asio(&m_ioService);
        setHandlers();
    }

    ~Impl() {
        disconnect(DisconnectReason::Normal);
        if (m_ioThread && m_ioThread->joinable()) {
            m_ioService.stop();
            m_ioThread->join();
        }
    }

    void setHandlers() {
        m_client.set_open_handler([this](ConnectionHdl hdl) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connection = hdl;
                m_connected = true;
            }
            COMPLOG_OK("[WS] Connected to server");

            // Отправка двух обязательных сообщений: текст и JSON
            try {
                m_client.send(hdl, "hello", websocketpp::frame::opcode::text);
                nlohmann::json j;
                j["test"] = "hello";
                m_client.send(hdl, j.dump(), websocketpp::frame::opcode::text);
            } catch (const std::exception& e) {
                COMPLOG_ERROR("[WS] Failed to send initial messages:", e.what());
            }
        });


        m_client.set_message_handler([this](ConnectionHdl hdl, MessagePtr msg) {
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


        m_client.set_close_handler([this](ConnectionHdl hdl) {
            DisconnectReason reason = DisconnectReason::ServerClosed; // по умолчанию
            websocketpp::close::status::value code;
            std::string reasonStr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto con = m_client.get_con_from_hdl(hdl);
                code = con->get_remote_close_code();
                reasonStr = con->get_remote_close_reason();
                m_connected = false;
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


        m_client.set_fail_handler([this](ConnectionHdl hdl) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connected = false;
            }
            auto con = m_client.get_con_from_hdl(hdl);
            auto ec = con->get_ec();
            COMPLOG_ERROR("[WS] Connection failed:", ec.message());
        });


        m_client.set_pong_handler([this](ConnectionHdl hdl, std::string payload) {
            if (payload.size() >= sizeof(uint64_t)) {
                uint64_t id;
                std::memcpy(&id, payload.data(), sizeof(id));
                std::lock_guard<std::mutex> lock(m_pingMutex);
                auto it = m_pendingPings.find(id);
                if (it != m_pendingPings.end()) {
                    it->second.promise.set_value(1); // здесь можно вычислить RTT, но для примера ставим 1
                    it->second.timer->cancel();
                    m_pendingPings.erase(it);
                }
            }
        });
    }

    void disconnect(DisconnectReason reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_connected) return;

        websocketpp::close::status::value code = websocketpp::close::status::normal;
        switch (reason) {
            case DisconnectReason::Normal:        code = websocketpp::close::status::normal; break;
            case DisconnectReason::ServerClosed:  code = websocketpp::close::status::going_away; break;
            case DisconnectReason::ProtocolError: code = websocketpp::close::status::protocol_error; break;
            case DisconnectReason::ConnectionLost:code = websocketpp::close::status::abnormal_close; break;
        }

        try {
            m_client.close(m_connection, code, "Disconnect by user");
        } catch (const std::exception& e) {
            COMPLOG_ERROR("[WS] Error during disconnect:", e.what());
        }
        m_connected = false;
    }
};


Client::Client() : d(std::make_unique<Impl>()) {}

Client::~Client() = default;

void Client::connect(const std::string& host, uint16_t port) {
    std::string uri = "ws://" + host + ":" + std::to_string(port);
    websocketpp::lib::error_code ec;
    auto con = d->m_client.get_connection(uri, ec);
    if (ec) {
        COMPLOG_ERROR("[WS] Connection error:", ec.message());
        return;
    }
    d->m_client.connect(con);

    if (!d->m_ioThread) {
        d->m_ioThread = std::make_unique<std::thread>([this]() {
            d->m_client.run();
        });
    }
}

bool Client::isConnected() const {
    std::lock_guard<std::mutex> lock(d->m_mutex);
    return d->m_connected;
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
        std::lock_guard<std::mutex> lock(d->m_pingMutex);
        id = d->m_nextPingId++;
        auto& pending = d->m_pendingPings[id];
        pending.promise = std::move(promise);
        pending.timer = std::make_unique<websocketpp::lib::asio::steady_timer>(d->m_ioService);
        pending.timer->expires_after(std::chrono::milliseconds(timeoutMs));
        pending.timer->async_wait([this, id](const websocketpp::lib::asio::error_code& ec) {
            if (ec) return; // таймер был отменён
            std::lock_guard<std::mutex> lock(d->m_pingMutex);
            auto it = d->m_pendingPings.find(id);
            if (it != d->m_pendingPings.end()) {
                it->second.promise.set_value(-1); // таймаут
                d->m_pendingPings.erase(it);
            }
        });
    }
    std::string payload(bytes, '\0');
    std::copy_n(&payload[0], sizeof(id), &id);

    websocketpp::lib::error_code ec;
    d->m_client.ping(d->m_connection, payload, ec);
    if (ec) {
        // Ping send error
        std::lock_guard<std::mutex> lock(d->m_pingMutex);
        auto it = d->m_pendingPings.find(id);
        if (it != d->m_pendingPings.end()) {
            it->second.promise.set_value(-1);
            d->m_pendingPings.erase(it);
        }
    }

    return future;
}

void Client::disconnect(DisconnectReason reason) {
    d->disconnect(reason);
}

} // namespace WebSockets
