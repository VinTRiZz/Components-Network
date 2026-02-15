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
Packet createErrorPacket(unsigned status);

using RequestProcessor = std::function<void(Packet&&)>;
using TargetProcessor = std::function<void(Packet&&, const RequestProcessor&)>;



// Из-за суперстранной истории с методом to_string() в boost::beast

template<typename T, typename = void>
struct has_to_string_method : std::false_type {};

template<typename T>
struct has_to_string_method<T, std::void_t<decltype(std::declval<T>().to_string())>>
    : std::true_type {};

template <typename T>
auto to_string(T&& v) {
    if constexpr (has_to_string_method<std::decay_t<T> >::value) {
        return v.to_string();
    } else {
        return v;
    }
}
}
