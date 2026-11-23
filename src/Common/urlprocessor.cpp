#include "urlprocessor.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef COMPONENTS_SW_network_qi
#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix.hpp>
#include <boost/phoenix/fusion/at.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#endif // Qi

namespace Network {

#ifdef COMPONENTS_SW_network_qi

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;

/**
 * @brief The UrlGrammar class  URI parser
 * @note Parses URI for this format: scheme ":" ["//" [userinfo "@"] host [":" port] ] path ["?" query] ["#" fragment]
 * @note Schema is expected
 */
template<typename Iterator>
struct UrlGrammar : qi::grammar<Iterator, Url()> {

    // Extra
    struct ToStringViewImpl {
        template <typename Range>
        std::string_view operator()(const Range& rng) const {
            if (rng.empty()) return {};
            return std::string_view(&*rng.begin(), rng.size());
        }
    };
    constexpr static phx::function<ToStringViewImpl> to_string_view{};

    struct InsertMapImpl {
        template <typename Map, typename Pair>
        void operator()(Map& map, Pair&& p) const {
            map.insert(typename Map::value_type(p.first, p.second));
        }
    };
    constexpr static phx::function<InsertMapImpl> insert_map{};

    using HostPair = std::pair<std::string_view, uint16_t>;
    using parameterMap_t = decltype(Url::parameters); // std::unordered_map<std::string_view, std::string_view>
    using parameterPair_t = std::pair<parameterMap_t::key_type, parameterMap_t::mapped_type>;

    UrlGrammar() : UrlGrammar::base_type(m_start) {
        m_schemaRule = qi::raw[qi::lexeme[+(qi::char_ - (qi::lit("://") | qi::lit(":")))]]
                              [qi::_val = to_string_view(qi::_1) ] >> (qi::lit("://") | qi::lit(":"));

        m_authLoginRule = qi::raw[qi::lexeme[+(qi::char_ - (qi::lit(':') | qi::lit('@')))]]
                                 [qi::_val = to_string_view(qi::_1) ] >> (qi::lit(':') | qi::lit('@'));
        m_authPasswordRule = qi::raw[qi::lexeme[+(qi::char_ - '@')]]
                                    [qi::_val = to_string_view(qi::_1) ] >> '@';

        m_hostRule = qi::raw[qi::lexeme[+(qi::char_ -(qi::lit(':') | qi::lit('/')) )]][phx::bind(&HostPair::first, qi::_val) = to_string_view(qi::_1)]
                         >> (qi::lit(':') >> -qi::uint_parser<uint16_t>()[phx::bind(&HostPair::second, qi::_val) = qi::_1]) | qi::lit('/');

        m_targetRule = qi::raw[qi::lexeme[+(qi::char_ - qi::lit('?'))]]
                              [qi::_val = to_string_view(qi::_1) ] >> -qi::lit('?');

        m_parameterPairRule = qi::raw[qi::lexeme[+(qi::char_ - qi::lit('='))]][phx::bind(&parameterPair_t::first, qi::_val) = to_string_view(qi::_1)]
                              >> qi::lit('=')
                              >> qi::raw[qi::lexeme[+(qi::char_ - (qi::lit('&') | qi::lit(';')))]][phx::bind(&parameterPair_t::second, qi::_val) = to_string_view(qi::_1)];
        m_parametersRule = m_parameterPairRule[insert_map(qi::_val, qi::_1)] % (qi::lit('&') | qi::lit(';'));

        m_start = m_schemaRule[phx::bind(&Url::schema, qi::_val) = qi::_1]
                  >> -m_authLoginRule[phx::bind(&Url::login, qi::_val) = qi::_1]
                  >> -m_authPasswordRule[phx::bind(&Url::password, qi::_val) = qi::_1]
                  >> -m_hostRule[phx::bind(&Url::host, qi::_val) = phx::at_c<0>(qi::_1), phx::bind(&Url::port, qi::_val) = phx::at_c<1>(qi::_1)]
                  >> -m_targetRule[phx::bind(&Url::target, qi::_val) = qi::_1]
                  >> -(m_parametersRule >> -qi::lit('#'))[phx::bind(&Url::parameters, qi::_val) = qi::_1]
                  >> -(-qi::lit('&') >> qi::lit('#') >> qi::raw[qi::lexeme[+(qi::char_)]])[phx::bind(&Url::fragment, qi::_val) = to_string_view(qi::_1)]
            ;

                // Debug needs
        m_schemaRule.name("Schema");
        m_authLoginRule.name("Auth_Login");
        m_authPasswordRule.name("Auth_Password");
        m_parameterPairRule.name("Query_Value");
        m_parametersRule.name("Query_Parameters");
        m_targetRule.name("Target");
        m_start.name("Url Parser");
    }

    qi::rule<Iterator, Url()> m_start;

    using stringRule_t = qi::rule<Iterator, std::string_view()>;
    stringRule_t m_schemaRule;

    stringRule_t m_authLoginRule;
    stringRule_t m_authPasswordRule;

    qi::rule<Iterator, HostPair> m_hostRule;

    stringRule_t m_targetRule;

    qi::rule<Iterator, parameterPair_t > m_parameterPairRule;
    qi::rule<Iterator, parameterMap_t()> m_parametersRule;
};


std::optional<Url> UrlProcessor::parse(const std::string& src,
                                       bool checkAvailability) const {
    if (src.empty()) { return {}; }

    if (checkAvailability) {
        for (auto& [_, chk] : m_preCheckers) {
            if (!chk(src)) {
                return {};
            }
        }
    }

    Url res {.source = src};
    const char* dataBeg = res.source.data();
    const char* dataEnd = res.source.data() + res.source.size();

    UrlGrammar<const char*> grammar;
    if (!qi::parse(dataBeg, dataEnd, grammar, res) || (dataBeg != dataEnd)) {
        return {};
    }

    if (checkAvailability) {
        auto availableTarget = m_availableTargets.find(std::string(res.target));
        if (availableTarget == m_availableTargets.end() ||
            availableTarget->second != res.parameters.empty()) {
            return {};
        }
    }
    return res;
}

#else

std::optional<Url> UrlProcessor::parse(const std::string& src,
                                       bool checkAvailability) const {
    if (src.empty()) { return {}; }

    // URL format:
    // schema://login:pass@host:port/target?parameters#fragment
    Url res {.source = src};
    auto currentDelim = src.find_first_of('/'); // This symbol must appear at least once

    // Not found target in url
    if (std::string::npos == currentDelim) {
        auto portStart = src.find_first_of(':');
        if (std::string::npos == portStart) {
            res.target = res.source;
        } else {
            res.target = {res.source.data(), portStart};
            try {
                res.port = std::stod(std::string{res.source.data() + portStart, res.source.size() - portStart});
            } catch (const std::invalid_argument& ex) {
                res.port = {};
            }
        }
        return res;
    }

    // There must be schema or target
    auto schemaEnd = src.find_first_of(':');
    if (schemaEnd < currentDelim) {
        res.schema = {res.source.data(), schemaEnd};
    }

    return res;
}

#endif // Boost::Qi enable

}  // namespace Network::HTTP
