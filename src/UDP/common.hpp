#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <vector>
#include <stdint.h>
#include <memory>

namespace UDP
{

/**
 * @brief The ErrorType enum Тип ошибки при работе с UDP
 */
enum ErrorType : short {
    NoError = 0,
    ConnectionError,
    SendError,
    ReceiveError,
    BindingError,
    ResolutionError
};

using ErrorCallback = std::function<void(ErrorType, const std::string&)>;
using RequestProcessor = std::function<void(std::vector<uint8_t>&&)>;

}
