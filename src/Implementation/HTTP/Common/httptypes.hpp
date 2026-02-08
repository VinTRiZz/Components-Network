#ifndef HTTPTYPES_H
#define HTTPTYPES_H

#include <string>
#include <functional>

namespace HTTP
{

struct HttpPacket
{
    enum MethodType : int {
        Get,
        Put,
        Post,
        Delete
    };
    static std::string toString(MethodType meth) {
        switch (meth)
        {
        case Get: return "GET";
        case Put: return "PUT";
        case Post: return "POST";
        case Delete: return "DELETE";
        }
        return "";
    }

    std::string target;
    bool        isFile {false};
    std::string fileSavepath {"download.bin"};

    enum BodyType : int {
        Undefined,
        Json,
        Html,
        Bytes,
    };
    BodyType    bodyType {BodyType::Undefined};
    BodyType    acceptableType {BodyType::Undefined};

    static std::string toString(BodyType btype) {
        switch (btype)
        {
        case BodyType::Undefined: return "text/plain";
        case BodyType::Json: return "application/json";
        case BodyType::Html: return "text/html";
        case BodyType::Bytes: return "application/octet-stream";
        }
        return "text/plain";
    }
    static BodyType fromString(const std::string& btype)
    {
        if (btype == "application/json") {
            return HttpPacket::Json;
        } else if (btype == "text/html") {
            return HttpPacket::Html;
        } else if (btype == "application/octet-stream") {
            return HttpPacket::Bytes;
        }

        return HttpPacket::Undefined;
    }
    std::string data;

    unsigned int    statusCode {0};
};

using RequestProcessor = std::function<void(const HttpPacket&)>;

using ProcessingCallback =
    std::function<
        void(
            const HttpPacket&,
            std::function<void(const HttpPacket&)>
        )>;

}

#endif // HTTPTYPES_H
