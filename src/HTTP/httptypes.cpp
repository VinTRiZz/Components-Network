#include "httptypes.hpp"

#include <boost/beast.hpp>

namespace HTTP
{

std::string Packet::toString(BodyType btype) {
    switch (btype)
    {
    case BodyType::Undefined: return "text/plain";
    case BodyType::Json: return "application/json";
    case BodyType::Html: return "text/html";
    case BodyType::Bytes: return "application/octet-stream";
    }
    return "text/plain";
}

Packet::BodyType Packet::fromString(const std::string &btype)
{
    if (btype == "application/json") {
        return Packet::Json;
    } else if (btype == "text/html") {
        return Packet::Html;
    } else if (btype == "application/octet-stream") {
        return Packet::Bytes;
    }

    return Packet::Undefined;
}

std::string toString(MethodType meth) {
    switch (meth)
    {
    case Get: return "GET";
    case Put: return "PUT";
    case Post: return "POST";
    case Delete: return "DELETE";
    }
    return "";
}

Packet createErrorPacket(unsigned status)
{
    Packet res;
    res.bodyType = Packet::Undefined;
    res.body = to_string(boost::beast::http::obsolete_reason(boost::beast::http::status(status)));
    res.statusCode = static_cast<unsigned int>(status);
    return res;
}

}
