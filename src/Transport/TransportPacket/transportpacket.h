#ifndef TRANSPORTPACKET_H
#define TRANSPORTPACKET_H

#include <string>
#include <functional>

namespace TransportLevel
{
const std::string DEFAULT_ENCRYPTION_KEY { "H1#32%2$67h*0H@E3453H&2hk$hK7^6&" };

struct TransportPacket
{
    static std::string& getEncryptionKey();

    enum class TPacketType : uint8_t
    {
        Invalid,
        ToClient,
        ToServer,
        Error
    };

    std::string token;
    std::string guid;
    TPacketType type {TPacketType::Invalid};
    uint64_t subPacketNo {0};
    uint64_t subPacketTotalCount {0};
    std::string payload;

    // Serialization for boost
    template<class Archive>
    void serialize(Archive & ar, const unsigned int) {
        ar & guid;
        ar & type;
        ar & subPacketNo;
        ar & subPacketTotalCount;
        ar & payload;
    }

    static std::string generateGuid();

    bool mustBeSeparated() const;

    operator std::string();
    std::string toString() const;
    std::vector<std::string> toSubpackets();

    bool fromString(const std::string &input);
    bool fromSubpackets(std::vector<TransportPacket> &subpacketsVect);

    bool operator ==(const TransportPacket& otp) {
        return ( (otp.type == type) &&
                 (otp.guid == guid) &&
                 (otp.subPacketNo == subPacketNo) &&
                 (otp.subPacketTotalCount == subPacketTotalCount) &&
                 (otp.payload == payload)
        );
    }
};

}

#endif // TRANSPORTPACKET_H
