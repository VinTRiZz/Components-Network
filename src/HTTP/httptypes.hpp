#pragma once

#include <string>
#include <functional>

namespace HTTP
{

enum MethodType : int {
    Get,
    Put,
    Post,
    Delete
};

std::string toString(MethodType meth);

struct Packet
{

    std::string target {"/"};
    bool        isFile {false};
    std::string fileSavepath {""};

    enum BodyType : int {
        Undefined,
        Json,
        Html,
        Bytes,
    };
    BodyType    bodyType {BodyType::Undefined};
    BodyType    acceptableType {BodyType::Undefined};

    static std::string toString(BodyType btype);
    static BodyType fromString(const std::string& btype);

    std::string     body;
    unsigned int    statusCode {0};
};

using RequestProcessor = std::function<void(Packet&&)>;
using TargetProcessor = std::function<void(Packet&&, const RequestProcessor&)>;

}
