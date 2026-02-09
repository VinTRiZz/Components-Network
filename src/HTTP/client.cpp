#include "client.hpp"

namespace HTTP {

Client::Client(asio::io_context &ioc)
    : m_resolver(asio::make_strand(ioc))
    , m_sendStream(asio::make_strand(ioc))
    , m_sendTimer(asio::make_strand(ioc))
{}

Client::~Client() {
    close();
}

bool Client::setHost(const std::string &host, uint16_t port) {
    m_serverInfo.host = host;
    m_serverInfo.port = port;
    m_serverInfo.scheme = "http";
    return true;
}

void Client::setTimeout(int seconds) {
    m_serverInfo.timeout_seconds = seconds;
}

void Client::setMethod(http::verb method) {
    m_currentRequest.method = method;
}

void Client::setTarget(const std::string &target) {
    m_currentRequest.target = target;
}

void Client::setBody(const std::string &body) {
    m_currentRequest.body = body;
}

void Client::setHeader(const std::string &key, const std::string &value) {
    m_currentRequest.setHeader(key, value);
}

void Client::setContentType(const std::string &type) {
    setHeader("Content-Type", type);
}

void Client::setUserAgent(const std::string &agent) {
    setHeader("User-Agent", agent);
}

void Client::setAuthorization(const std::string &token) {
    setHeader("Authorization", token);
}

bool Client::sendData(HTTPPacket &&packet) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentRequest = std::move(packet);

        // Разрешение хоста
        auto const results = m_resolver.resolve(
                    m_serverInfo.host,
                    std::to_string(m_serverInfo.port)
                    );

        // Установка таймаута
        m_sendTimer.expires_after(std::chrono::seconds(m_serverInfo.timeout_seconds));
        m_sendTimer.async_wait([this](const boost::system::error_code& ec) {
            if (ec != boost::asio::error::operation_aborted) {
                m_sendStream.close();
            }
        });

        // Подключение
        beast::get_lowest_layer(m_sendStream).connect(results);

        // Создание HTTP запроса
        http::request<http::string_body> req{m_currentRequest.method,
                    m_currentRequest.target,
                    m_currentRequest.version};

        req.body() = m_currentRequest.body;
        req.prepare_payload();

        // Установка заголовков
        for (const auto& [key, value] : m_currentRequest.headers) {
            req.set(key, value);
        }

        // Установка хоста
        req.set(http::field::host, m_serverInfo.host);

        // Отправка запроса
        http::write(m_sendStream, req);

        // Получение ответа
        beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(m_sendStream, buffer, res);

        // Отмена таймера
        m_sendTimer.cancel();

        // Сохранение ответа
        m_lastResponse.version = res.version();
        m_lastResponse.body = beast::buffers_to_string(res.body().data());
        m_lastResponse.headers.clear();

        for (const auto& field : res) {
            m_lastResponse.setHeader(std::string(field.name_string()),
                                     std::string(field.value()));
        }

        return true;

    } catch (const std::exception& e) {
        if (m_errorCallback) {
            m_errorCallback(ErrorType::UNKNOWN, e.what());
        }
        return false;
    }
}

const HTTPPacket &Client::getLastResponse() const {
    return m_lastResponse;
}

void Client::setErrorCallback(ErrorCallback callback) {
    m_errorCallback = std::move(callback);
}

void Client::close() {
    beast::error_code ec;
    m_sendStream.socket().shutdown(tcp::socket::shutdown_both, ec);
    m_sendStream.close();
}

} // namespace HTTP
