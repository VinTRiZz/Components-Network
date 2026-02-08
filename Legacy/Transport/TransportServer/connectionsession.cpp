#include "connectionsession.h"

#include <Components/Logger/Logger.h>

#include <boost/scope_exit.hpp>

#include <thread>

TransportLevel::ConnectionSession::ConnectionSession(tcp::socket socket, const ProcessCallbackT &packetProcessCallback, const std::vector<std::string> &tokenList) :
    socket_(std::move(socket)),
    m_packetProcessCallback{packetProcessCallback},
    m_tokenList {tokenList}
{

}

TransportLevel::ConnectionSession::~ConnectionSession() {
    LOG_INFO("Client disconnected");
}


void TransportLevel::ConnectionSession::readSocket(boost::asio::io_context &processingContext) {
    socket_.async_read_some(
        boost::asio::buffer(data_),
        [this, self = shared_from_this(), &processingContext](boost::system::error_code ec, std::size_t) {
        if (ec.value() == 2) {
            return;
        } else if (ec) {
            LOG_INFO("Unknown exchange error");
        }

        processingContext.post([this, self, tmpBuffer = data_](){ // Buffer is only to avoid errors caused by threading
            TransportPacket requestPacket;
            if (!requestPacket.fromString(tmpBuffer)) {
                LOG_ERROR("Error deserialize request packet");
                return;
            }

            auto tokenIt = std::find(m_tokenList.begin(), m_tokenList.end(), requestPacket.token);
            if (tokenIt == m_tokenList.end()) {
                TransportPacket responsePacket = requestPacket;
                responsePacket.token = "";
                writeSocket(responsePacket);
                LOG_ERROR("Invalid token received");
                return;
            }

            if (requestPacket.subPacketTotalCount > 0) {
                if (!processSubpacket(requestPacket)) { // Get next subpacket
                    return;
                }
                m_subPacketsMap.erase(requestPacket.guid);
            }

            TransportPacket responsePacket;
            if (!m_packetProcessCallback(requestPacket.payload, responsePacket.payload)) {
                return;
            }

            if (responsePacket.mustBeSeparated()) {
                auto subpackets = responsePacket.toSubpackets();
                for (auto& sbp : subpackets) {
                    writeSocket(sbp);
                }
            } else {
                writeSocket(responsePacket.toString());
            }
        });
        self->readSocket(processingContext);
    });
}

void TransportLevel::ConnectionSession::writeSocket(const std::string &dataStr)
{
    socket_.write_some(boost::asio::buffer(dataStr));
//    LOG_OK("Sent", dataStr.size(), "bytes");

    // Need this delay to send data correctly
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Local machine will work with packets so fast
}

void TransportLevel::ConnectionSession::writeSocketAsync(const std::string &dataStr)
{
    socket_.async_write_some(
        boost::asio::buffer(dataStr),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t length) {
        if (ec) {
            LOG_ERROR("Error senging response:", ec.message());
        }
//        LOG_OK("Sent", length, "bytes");
    });
}

bool TransportLevel::ConnectionSession::processSubpacket(TransportPacket &p)
{
    auto subpacketQueueIt = m_subPacketsMap.find(p.guid);
    if (subpacketQueueIt == m_subPacketsMap.end()) {
        std::vector<TransportPacket> subpackets;

        m_subPacketsMap[p.guid] = [subpackets](TransportPacket &sp) mutable -> bool {
            subpackets.push_back(sp);
            if (subpackets.size() < sp.subPacketTotalCount) {
                return false;
            }

            sp.fromSubpackets(subpackets);
            return true;
        };
    }
    return m_subPacketsMap[p.guid](p);
}
