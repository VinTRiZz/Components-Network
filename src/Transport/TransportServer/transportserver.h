#ifndef TRANSPORTSERVERINSTANCE_H
#define TRANSPORTSERVERINSTANCE_H

#include <memory>
#include <functional>

namespace TransportLevel
{

typedef std::function<bool(const std::string& inputPacket, std::string& outputPacket)> ProcessCallbackT;

class TransportServerInstance
{
public:
    TransportServerInstance();
    ~TransportServerInstance();

    void setPort(uint16_t portNo);
    void setThreadCount(uint64_t threadCount);
    void loadTokens(const std::string& filePath);

    int init(const ProcessCallbackT& procCallback);

    int start();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

}

#endif // TRANSPORTSERVERINSTANCE_H
