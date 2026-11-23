#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <list>
#include <map>
#include <unordered_map>
#include <stdint.h>

#include <functional>
#include <unordered_map>

namespace Network {

/**
 * @brief The UrlMetadata class Object to work with parsed URL
 * @note schema://login:pass@host:port/target?parameters#fragment
 */
struct Url {
    std::string source;  ///< Source URL text
    std::string_view schema;
    std::optional<std::string_view> login;
    std::optional<std::string_view> password;
    std::string_view host;
    std::optional<uint16_t> port;
    std::string_view target;
    std::string_view fragment;
    std::unordered_map<std::string_view, std::string_view> parameters;
};

/**
 * @brief The UrlProcessor class    Class to work with URLs
 */
class UrlProcessor {
public:
    // Working with available targets
    void addAvailableTarget(const std::string& tgt, bool hasArguments = false) & { m_availableTargets[tgt] = hasArguments; }
    std::map<std::string, bool> getAvailableTargets() const& { return m_availableTargets; }
    void removeAvailableTarget(const std::string& tgt) & { m_availableTargets.erase(tgt); }

    using checker_t = std::function<bool(std::string_view)>; // Checker must return true if check passed
    void addPreCheck(const std::string& checkName, checker_t&& chk) & { m_preCheckers[checkName] = std::move(chk); }
    void removePreCheck(const std::string& checkName) & { m_preCheckers.erase(checkName); }

    std::optional<Url> parse(const std::string& src, bool checkAvailability = false) const;

private:
    std::unordered_map<std::string, checker_t>  m_preCheckers; // Name - checker
    std::map<std::string, bool>                 m_availableTargets; // Url - hasArguments
};

}  // namespace Network::HTTP
