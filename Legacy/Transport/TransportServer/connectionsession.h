#ifndef CONNECTIONSESSION_H
#define CONNECTIONSESSION_H

#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>

#include "../TransportPacket/transportpacket.h"

using boost::asio::ip::tcp;

namespace TransportLevel
{

typedef std::function<bool(const std::string& inputPacket, std::string& outputPacket)> ProcessCallbackT;

class ConnectionSession : public std::enable_shared_from_this<ConnectionSession> {
public:
    ConnectionSession(tcp::socket socket, const ProcessCallbackT& packetProcessCallback, const std::vector<std::string>& tokenList);

    ~ConnectionSession();

    void readSocket(boost::asio::io_context& processingContext);

private:
    void writeSocket(const std::string& dataStr);
    void writeSocketAsync(const std::string& dataStr);
    bool processSubpacket(TransportPacket& p); // Return true if all subpackets loaded

    tcp::socket socket_;
    static const uint64_t dataBufferSize_ {65535}; // Must not be so huge request
    char data_[dataBufferSize_];

    int64_t m_currentPacketNo {0};
    std::unordered_map<std::string, std::function<bool(TransportPacket&)> > m_subPacketsMap; // Used to handle subpackets
    ProcessCallbackT m_packetProcessCallback;
    const std::vector<std::string>& m_tokenList;
};

}

#endif // CONNECTIONSESSION_H
