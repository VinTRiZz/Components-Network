#include "transportclient.h"

#include <QTcpSocket>

#include <boost/scoped_ptr.hpp>

#include "../TransportPacket/transportpacket.h"

#include <Components/Logger/Logger.h>

#include <thread>

namespace TransportLevel
{

struct TransportClient::Impl
{
    boost::scoped_ptr<QTcpSocket> sock {new QTcpSocket};

    std::string token;
    QString serverAddress {"127.0.0.1"};
    uint16_t serverPort {0};

    std::unordered_map<std::string, std::vector<TransportPacket> > subpacketVectors;

    ProcessCallbackT m_processorCallback;
    ErrorCallbackT m_errorCallback; // Used to show errors

    void send(const QByteArray& reqp) {
        if (sock->write(reqp) != reqp.size()) {
            LOG_ERROR("Error sending data");
        }
//        LOG_OK("Sent", reqp.size(), "bytes");
//        sock->waitForReadyRead(1000);
    }

    void processSubpacket(TransportPacket& subp) {
        subpacketVectors[subp.guid].push_back(subp);
        if (subpacketVectors[subp.guid].size() == subp.subPacketTotalCount) {
            subp.fromSubpackets(subpacketVectors[subp.guid]);
            TransportPacket responseP;
            if (m_processorCallback(subp.payload, responseP.payload)) {
                send(responseP.toString().c_str());
                return;
            }
        }
    }
};

TransportClient::TransportClient(QObject *parent) :
    QObject{parent},
    d {new Impl}
{
    connect(d->sock.get(), &QTcpSocket::connected, this, &TransportClient::connected);
    connect(d->sock.get(), &QTcpSocket::disconnected, this, &TransportClient::disconnected);

    connect(d->sock.get(), &QTcpSocket::readyRead, this, &TransportClient::gotResponse);
}

TransportClient::~TransportClient()
{

}

bool TransportClient::connectToHost()
{
    if (d->token.empty()) {
        error("Token not set");
        return false;
    }

    if (d->serverPort == 0) {
        error("Port not set");
        return false;
    }

    if (!d->m_processorCallback) {
        error("Callback for response packets not set");
        return false;
    }

    d->sock->connectToHost(d->serverAddress, d->serverPort);
    d->sock->waitForConnected(3000);

    return isConnected();
}

bool TransportClient::isConnected() const
{
    return (d->sock->state() == QTcpSocket::ConnectedState);
}

void TransportClient::disconnectFromHost()
{
    if (isConnected()) {
        d->sock->disconnectFromHost();
        d->sock->waitForDisconnected(1000);
    }
}

void TransportClient::setToken(const QString &token)
{
    d->token = token.toStdString();
}

void TransportClient::setAddress(const QString &hostname, uint16_t port)
{
    if (isConnected()) {
        error("Can not set address: in connected state");
        return;
    }
    d->serverAddress = hostname;
    d->serverPort = port;
}

void TransportClient::setProcessor(const ProcessCallbackT &pc)
{
    if (isConnected()) {
        error("Can not set processor: in connected state");
        return;
    }
    d->m_processorCallback = pc;
}

void TransportClient::setErrorCallback(const ErrorCallbackT &ect)
{
    d->m_errorCallback = ect;
}

void TransportClient::request(const std::string &requestData)
{
    if (!connectToHost()) {
        error("Cannot request: connection error");
        return;
    }

    TransportPacket reqp;
    reqp.token = d->token;
    reqp.guid = reqp.generateGuid();
    reqp.payload = requestData;
    if (reqp.mustBeSeparated()) {
        auto subpackets = reqp.toSubpackets();
        for (auto& sbp : subpackets) {
            d->send(sbp.c_str());
        }
    } else {
        d->send(reqp.toString().c_str());
    }
}

void TransportClient::error(const QString &errorMsg)
{
    if (d->m_errorCallback) {
        d->m_errorCallback(errorMsg);
    }
}

void TransportClient::gotResponse()
{
//    LOG_DEBUG("GOT RESPONSE");
    auto responseData = d->sock->readAll();
    TransportPacket respp;
    if (!respp.fromString(responseData.toStdString())) {
        error("Failed to deserialize response");
        return;
    }

    if (respp.subPacketTotalCount != 0) {
        d->processSubpacket(respp);
        return;
    }

    TransportPacket requestP;
    if (d->m_processorCallback(respp.payload, requestP.payload)) {
        d->send(requestP.payload.c_str());
    }
}

}
