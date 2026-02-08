#ifndef UL_EXCHANGEPACKET_H
#define UL_EXCHANGEPACKET_H

#ifdef QT_CORE_LIB
#include <QString>
#include <QDebug>
#endif // QT_CORE_LIB

#include <sstream>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <Components/Encryption/Encoding.h>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

namespace Exchange
{

enum PACKET_TYPES
{
    ACTION_PACKET_TYPE,
    INFO_PACKET_TYPE,
    ERROR_PACKET_TYPE
};

struct Packet
{
    int type {0};
    std::string command;
    std::string data;
    bool isValid {false};

    // Serialization for boost
    template<class Archive>
    void serialize(Archive & ar, const unsigned int) {
        ar & type;
        ar & command;
        ar & data;
    }
};

class PacketConverter
{
public:
    static QByteArray convert(const Packet& data_p)
    {
        try {
            if (!data_p.isValid) {
                return {};
            }
            std::ostringstream oss;
            boost::archive::text_oarchive oarchive(oss);
            oarchive << data_p;  // Serialize the structure
            return QByteArray::fromStdString(Encryption::encodeHex(oss.str()));
        }
        catch (std::exception& ex) {
            return QByteArray();
        }
    }

    static Packet convert(const QByteArray& data_ba)
    {
        try {
            std::string inputDecoded = Encryption::decodeHex(data_ba.toStdString());
            std::istringstream iss(inputDecoded);
            boost::archive::text_iarchive iarchive(iss);

            Packet result;
            iarchive >> result;  // Deserialize the structure
            result.isValid = true; // Shows that conversion complete with success
            return result;
        }
        catch (std::exception& ex) {
            return Packet();
        }
    }
};

}


#endif // UL_EXCHANGEPACKET_H
