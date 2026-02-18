#include "server.hpp"

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <mutex>
#include <set>
#include <iostream>

#include <Components/Logger/Logger.h>

namespace WebSockets {

struct Server::Impl {
    using Server = websocketpp::server<websocketpp::config::asio>;
    using ConnectionHdl = websocketpp::connection_hdl;
    using MessagePtr = websocketpp::config::asio::message_type::ptr;

    Server m_server;
    std::unique_ptr<std::thread> m_ioThread;
    websocketpp::lib::asio::io_service m_ioService;
    std::set<ConnectionHdl, std::owner_less<ConnectionHdl>> m_connections;
    mutable std::mutex m_mutex;
    bool m_listening = false;

    Impl() {
        m_server.init_asio(&m_ioService);
        setHandlers();
    }

    ~Impl() {
        stop();
    }

    void setHandlers() {
        m_server.set_open_handler([this](ConnectionHdl hdl) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connections.insert(hdl);
            }
            // Получаем информацию о клиенте (удалённый адрес)
            auto con = m_server.get_con_from_hdl(hdl);
            auto remote = con->get_remote_endpoint();
            COMPLOG_OK("[WS] Client connected:", remote);
        });


        m_server.set_close_handler([this](ConnectionHdl hdl) {
            DisconnectReason reason = DisconnectReason::Unknown;
            std::string reasonStr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_connections.find(hdl);
                if (it != m_connections.end()) {
                    m_connections.erase(it);
                }
                auto con = m_server.get_con_from_hdl(hdl);
                auto code = con->get_remote_close_code();
                reasonStr = con->get_remote_close_reason();
                // Преобразование кода в enum (упрощённо)
                if (code == websocketpp::close::status::normal) {
                    reason = DisconnectReason::Normal;
                } else if (code == websocketpp::close::status::going_away) {
                    reason = DisconnectReason::ServerShutdown; // клиент уходит
                } else if (code == websocketpp::close::status::protocol_error) {
                    reason = DisconnectReason::ProtocolError;
                } else {
                    reason = DisconnectReason::ConnectionLost;
                }
            }
            COMPLOG_WARNING("[WS] Client disconnected:", reasonStr, "reason:", static_cast<int>(reason));
        });


        m_server.set_message_handler([this](ConnectionHdl hdl, MessagePtr msg) {
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string payload = msg->get_payload();
                COMPLOG_DEBUG("[WS] Text data got:", payload);
//                try {
//                    auto j = nlohmann::json::parse(payload);
//                } catch (nlohmann::json::exception& parseEx) {

//                }
            } else {
                COMPLOG_INFO("[WS] Binary message received, size:", msg->get_payload().size());
            }
        });


        m_server.set_fail_handler([this](ConnectionHdl hdl) {
            auto con = m_server.get_con_from_hdl(hdl);
            auto ec = con->get_ec();
            COMPLOG_ERROR("[WS] Connection failed:", ec.message());
        });

        m_server.set_http_handler([this](ConnectionHdl hdl) {
            auto con = m_server.get_con_from_hdl(hdl);
            con->set_body("Invalid protocol (connect using WebSockets)");
            con->set_status(websocketpp::http::status_code::ok);
        });
    }

    void stop() {

        // Stop server
        if (m_listening) {
            websocketpp::lib::error_code ec;
            m_server.stop_listening(ec);
            if (ec) {
                COMPLOG_ERROR("[WS] Error stopping listener:", ec.message());
            }
            m_listening = false;
        }

        // Stop connections
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& hdl : m_connections) {
                try {
                    m_server.close(hdl, websocketpp::close::status::going_away, "Server shutdown");
                } catch (...) {
                    // Игнорируем ошибки закрытия
                }
            }
            m_connections.clear();
        }

        // Stop IO
        if (m_ioThread && m_ioThread->joinable()) {
            m_ioService.stop();
            m_ioThread->join();
            m_ioThread.reset();
        }
    }
};

// ========================== Server ==========================
Server::Server() : d(std::make_unique<Impl>()) {}

Server::~Server() = default;

void Server::listen(const std::string& host, uint16_t port) {
    websocketpp::lib::asio::ip::tcp::endpoint endpoint(
        websocketpp::lib::asio::ip::make_address(host),
        port
    );

    websocketpp::lib::error_code ec;
    d->m_server.listen(endpoint, ec);
    if (ec) {
        COMPLOG_ERROR("[WS] Listen error:", ec.message());
        return;
    }

    d->m_server.start_accept(ec);
    if (ec) {
        COMPLOG_ERROR("[WS] Start accept error:", ec.message());
        return;
    }

    d->m_listening = true;
    if (!d->m_ioThread) {
        d->m_ioThread = std::make_unique<std::thread>([this]() {
            d->m_server.run();
        });
    }

    COMPLOG_OK("[WS] Server listening on", host + ":" + std::to_string(port));
}

void Server::stop() {
    d->stop();
    COMPLOG_INFO("[WS] Server stopped");
}

bool Server::isListening() const {
    return d->m_listening;
}

} // namespace WebSockets
