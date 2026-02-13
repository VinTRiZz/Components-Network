#include "transportpacket.h"

#include <Components/Logger/Logger.h>
#include <Components/Encryption/AES-256.h>
#include <Components/Encryption/Encoding.h>

#include <boost/algorithm/hex.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/optional.hpp>

namespace TransportLevel
{

const int MAX_PACKET_SIZE_B {8000};

std::string &TransportPacket::getEncryptionKey()
{
    static std::string TRANSPORT_PACKET_ENCRYPTION_KEY {DEFAULT_ENCRYPTION_KEY};
    return TRANSPORT_PACKET_ENCRYPTION_KEY;
}

std::string TransportPacket::generateGuid()
{
    std::string hashedStr = Encryption::generateKey(16);
    hashedStr = Encryption::encodeHex(hashedStr);
    hashedStr.erase(32, hashedStr.size());
    hashedStr.insert(8, "-");
    hashedStr.insert(13, "-");
    hashedStr.insert(18, "-");
    hashedStr.insert(23, "-");
    return hashedStr;
}

bool TransportPacket::mustBeSeparated() const
{
    return (payload.size() > MAX_PACKET_SIZE_B);
}

TransportPacket::operator std::string()
{
    return toString();
}

std::string TransportPacket::toString() const
{
    // Create a stringstream to hold the binary data
    std::ostringstream oss;
    boost::archive::text_oarchive oarchive(oss);
    oarchive << *this;  // Serialize the structure
    auto result = Encryption::encodeHex( Encryption::aes256encrypt(oss.str(), getEncryptionKey()) );
    return result;
}

std::vector<std::string> TransportPacket::toSubpackets()
{
    std::vector<std::string> subPacketVector;

    TransportPacket subPacket;
    subPacket.guid = guid;
    subPacket.type = type;

    auto serializedPacket = toString();

    subPacket.subPacketTotalCount = serializedPacket.size() / MAX_PACKET_SIZE_B + 1;
    const uint64_t packetSize = serializedPacket.size();

    subPacketVector.reserve(subPacket.subPacketTotalCount);

    auto currentItPos = serializedPacket.begin();
    uint64_t currentPacketSize = 0;

    for (uint64_t i = 0; i < subPacket.subPacketTotalCount; i++) {
        currentPacketSize = (i * MAX_PACKET_SIZE_B + MAX_PACKET_SIZE_B) < packetSize ? MAX_PACKET_SIZE_B : serializedPacket.size() % MAX_PACKET_SIZE_B;

        subPacket.subPacketNo = i;
        subPacket.payload = std::string(currentItPos, currentItPos + currentPacketSize );
        currentItPos += currentPacketSize;

        subPacketVector.push_back(subPacket);
    }

    return subPacketVector;
}

bool TransportPacket::fromString(const std::string &input)
{
    try {
        std::string inputDecoded = Encryption::decodeHex(input);
        inputDecoded = Encryption::aes256decrypt(inputDecoded, getEncryptionKey());
        std::istringstream iss(inputDecoded);
        boost::archive::text_iarchive iarchive(iss);
        iarchive >> *this;  // Deserialize the structure
        return true;
    } catch (boost::archive::archive_exception& ex) {
        COMPLOG_ERROR("Error deserialize packet:", ex.what(), "packet size:", input.size());
    } catch (boost::wrapexcept<boost::algorithm::non_hex_input>& ex) {
        COMPLOG_ERROR("Error deserialize packet payload:", ex.what(), "packet size:", input.size());
    }
    return false;
}

bool TransportPacket::fromSubpackets(std::vector<TransportPacket> &subpacketsVect)
{
    std::string resultPacketSerialized;
    std::sort(subpacketsVect.begin(),
              subpacketsVect.end(),
              [](const TransportPacket& rp1, const TransportPacket& rp2){ return (rp1.subPacketNo < rp2.subPacketNo); });

    for (auto& subPacket : subpacketsVect) {
        resultPacketSerialized += subPacket.payload;
    }
    return fromString(resultPacketSerialized);
}

}
