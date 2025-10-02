#ifndef TRANSPORTCLIENT_H
#define TRANSPORTCLIENT_H

#include <QObject>
#include <memory>

namespace TransportLevel
{

// Requests outputPacket if return true
typedef std::function<bool(const std::string& inputPacket, std::string& outputPacket)> ProcessCallbackT;

// Calls on every error or warning
typedef std::function<void(const QString&)> ErrorCallbackT;

class TransportClient : public QObject
{
    Q_OBJECT
public:
    explicit TransportClient(QObject *parent = nullptr);
    ~TransportClient();

    bool connectToHost();
    bool isConnected() const;
    void disconnectFromHost();

    void setToken(const QString& token);
    void setAddress(const QString& hostname, uint16_t port);
    void setProcessor(const ProcessCallbackT& pc);
    void setErrorCallback(const ErrorCallbackT& ect);

    void request(const std::string& requestData);

signals:
    void connected();
    void disconnected();

private:
    struct Impl;
    std::unique_ptr<Impl> d;

    void error(const QString& errorMsg);

private slots:
    void gotResponse();
};

}

#endif // TRANSPORTCLIENT_H
