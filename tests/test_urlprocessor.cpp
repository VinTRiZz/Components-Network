#include <gtest/gtest.h>

#include "Common/urlprocessor.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using Network::Url;
using Network::UrlProcessor;

using Port = std::optional<std::uint16_t>;
using Parameters = std::map<std::string, std::string>;

struct UrlCase
{
    std::string               source;
    std::string               schema;
    std::string               host;
    Port                      port;
    std::string               target;
    Parameters                parameters;
    std::optional<std::string> login;
    std::optional<std::string> password;
};

std::string asString(std::string_view value)
{
    return std::string(value);
}

Parameters asParameters(
    const std::unordered_map<std::string_view, std::string_view>& values)
{
    Parameters result;
    for (const auto& [key, value] : values) {
        result.emplace(std::string(key), std::string(value));
    }
    return result;
}

void expectOptionalString(const std::optional<std::string_view>& actual,
                          const std::optional<std::string>& expected,
                          const char* fieldName)
{
    SCOPED_TRACE(::testing::Message() << fieldName);

    if (expected.has_value()) {
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(asString(*actual), *expected);
    } else {
        EXPECT_FALSE(actual.has_value());
    }
}

void expectUrl(const UrlProcessor& processor, const UrlCase& expected)
{
    SCOPED_TRACE(::testing::Message() << "URL: " << expected.source);

    const std::optional<Url> parsed = processor.parse(expected.source);
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->source, expected.source);
    EXPECT_EQ(asString(parsed->schema), expected.schema);
    expectOptionalString(parsed->login, expected.login, "login");
    expectOptionalString(parsed->password, expected.password, "password");
    EXPECT_EQ(asString(parsed->host), expected.host);

    if (expected.port.has_value()) {
        ASSERT_TRUE(parsed->port.has_value());
        EXPECT_EQ(*parsed->port, *expected.port);
    } else {
        EXPECT_FALSE(parsed->port.has_value());
    }

    EXPECT_EQ(asString(parsed->target), expected.target);
    EXPECT_EQ(asParameters(parsed->parameters), expected.parameters);
}

void expectInvalidUrl(const UrlProcessor& processor, const std::string& source)
{
    SCOPED_TRACE(::testing::Message() << "URL: " << source);
    EXPECT_FALSE(processor.parse(source).has_value());
}

}  // namespace

TEST(Components_Network, UrlProcessorSimpleUrls)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://example.com/", "http", "example.com", Port{}, "/"},
        {"https://example.com/", "https", "example.com", Port{}, "/"},
        {"ws://example.com/socket", "ws", "example.com", Port{}, "/socket"},
        {"wss://example.com/socket", "wss", "example.com", Port{}, "/socket"},
        {"http://localhost/", "http", "localhost", Port{}, "/"},
        {"http://localhost:3000/", "http", "localhost", Port{3000}, "/"},
        {"http://127.0.0.1/", "http", "127.0.0.1", Port{}, "/"},
        {"http://127.0.0.1:8080/status", "http", "127.0.0.1", Port{8080}, "/status"},
        {"https://example.com:443/", "https", "example.com", Port{443}, "/"},
        {"http://example.com", "http", "example.com", Port{}, ""},
        {"http://example.com//", "http", "example.com", Port{}, "//"},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorComplexUrls)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"https://user:password@example.com:8443/api/v1/resource",
         "https", "example.com", Port{8443}, "/api/v1/resource", {},
         "user", "password"},
        {"https://user@example.com/private",
         "https", "example.com", Port{}, "/private", {},
         "user", std::nullopt},
        {"ftp://anonymous@files.example.com/pub/file.txt",
         "ftp", "files.example.com", Port{}, "/pub/file.txt", {},
         "anonymous", std::nullopt},
        {"http://user:p:ass@example.com:8080/private",
         "http", "example.com", Port{8080}, "/private", {},
         "user", "p:ass"},
        {"https://user:p%40ss@example.com/secure",
         "https", "example.com", Port{}, "/secure", {},
         "user", "p%40ss"},
        {"http://user:pass@10.0.0.1:9090/metrics",
         "http", "10.0.0.1", Port{9090}, "/metrics", {},
         "user", "pass"},
        {"https://my-service.api.example.com:443/v2/objects/42",
         "https", "my-service.api.example.com", Port{443}, "/v2/objects/42"},
        {"https://example.com/assets/app.min.js",
         "https", "example.com", Port{}, "/assets/app.min.js"},
        {"https://example.com/search/hello%20world",
         "https", "example.com", Port{}, "/search/hello%20world"},
        {"http://example.com/a/b/c/d/e/f",
         "http", "example.com", Port{}, "/a/b/c/d/e/f"},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorCredentials)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://user@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "user", std::nullopt},
        {"http://user:password@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "user", "password"},
        {"http://user:@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "user", ""},
        {"http://:password@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "", "password"},
        {"http://user:p:ass@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "user", "p:ass"},
        {"http://user:pa%40ss@example.com/path",
         "http", "example.com", Port{}, "/path", {},
         "user", "pa%40ss"},
        {"http://user:pass@example.com/users/@me",
         "http", "example.com", Port{}, "/users/@me", {},
         "user", "pass"},
        {"http://example.com/path", "http", "example.com", Port{}, "/path"},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorTargets)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://example.com/", "http", "example.com", Port{}, "/"},
        {"http://example.com/api", "http", "example.com", Port{}, "/api"},
        {"http://example.com/api/", "http", "example.com", Port{}, "/api/"},
        {"http://example.com/a/b/c.d/e_f-g",
         "http", "example.com", Port{}, "/a/b/c.d/e_f-g"},
        {"http://example.com/a/../b", "http", "example.com", Port{}, "/a/../b"},
        {"http://example.com//double//slash",
         "http", "example.com", Port{}, "//double//slash"},
        {"http://example.com/users/@me",
         "http", "example.com", Port{}, "/users/@me"},
        {"http://example.com/time/12:30",
         "http", "example.com", Port{}, "/time/12:30"},
        {"http://example.com/redirect/http://other.example/path",
         "http", "example.com", Port{}, "/redirect/http://other.example/path"},
        {"http://example.com/%D0%BF%D1%83%D1%82%D1%8C",
         "http", "example.com", Port{}, "/%D0%BF%D1%83%D1%82%D1%8C"},
        {"http://example.com/search%3Fq=test",
         "http", "example.com", Port{}, "/search%3Fq=test"},
        {"http://example.com/page%23section",
         "http", "example.com", Port{}, "/page%23section"},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorPorts)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://example.com:0/", "http", "example.com", Port{0}, "/"},
        {"http://example.com:1/", "http", "example.com", Port{1}, "/"},
        {"http://example.com:80/", "http", "example.com", Port{80}, "/"},
        {"http://example.com:443/", "http", "example.com", Port{443}, "/"},
        {"http://example.com:8000/", "http", "example.com", Port{8000}, "/"},
        {"http://example.com:8080/", "http", "example.com", Port{8080}, "/"},
        {"http://example.com:65535/", "http", "example.com", Port{65535}, "/"},
        {"http://example.com:00080/", "http", "example.com", Port{80}, "/"},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorArguments)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://example.com/search?q=test",
         "http", "example.com", Port{}, "/search", {{"q", "test"}}},
        {"http://example.com/search?q=test&limit=10&active=true",
         "http", "example.com", Port{}, "/search",
         {{"q", "test"}, {"limit", "10"}, {"active", "true"}}},
        {"http://example.com/search?q=hello%20world&lang=en",
         "http", "example.com", Port{}, "/search",
         {{"q", "hello%20world"}, {"lang", "en"}}},
        {"http://example.com/items?ids=1,2,3&sort=desc",
         "http", "example.com", Port{}, "/items",
         {{"ids", "1,2,3"}, {"sort", "desc"}}},
        {"http://example.com/path?token=abc==&empty=&flag",
         "http", "example.com", Port{}, "/path",
         {{"token", "abc=="}, {"empty", ""}, {"flag", ""}}},
        {"http://example.com/path?a&b=2&c=3",
         "http", "example.com", Port{}, "/path",
         {{"a", ""}, {"b", "2"}, {"c", "3"}}},
        {"http://example.com/path?redirect=http://other.example/home",
         "http", "example.com", Port{}, "/path",
         {{"redirect", "http://other.example/home"}}},
        {"http://example.com/path?",
         "http", "example.com", Port{}, "/path", {}},
        {"http://example.com?x=1",
         "http", "example.com", Port{}, "", {{"x", "1"}}},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorArgumentsDuplicateKeys)
{
    const UrlProcessor processor;
    const std::string source = "http://example.com/path?tag=a&tag=b";

    const std::optional<Url> parsed = processor.parse(source);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_EQ(parsed->parameters.size(), 1u);

    const auto parameter = parsed->parameters.find("tag");
    ASSERT_NE(parameter, parsed->parameters.end());
    EXPECT_TRUE(parameter->second == std::string_view{"a"}
                || parameter->second == std::string_view{"b"});
}

TEST(Components_Network, UrlProcessorFragments)
{
    const UrlProcessor processor;

    const std::vector<UrlCase> cases {
        {"http://example.com/page#section",
         "http", "example.com", Port{}, "/page", {}},
        {"http://example.com/page?x=1#section",
         "http", "example.com", Port{}, "/page", {{"x", "1"}}},
        {"http://example.com/page#section?ignored=1",
         "http", "example.com", Port{}, "/page", {}},
        {"http://example.com/page?x=1#section?ignored=2",
         "http", "example.com", Port{}, "/page", {{"x", "1"}}},
        {"http://example.com/page#",
         "http", "example.com", Port{}, "/page", {}},
        {"http://example.com/page#a&b=c",
         "http", "example.com", Port{}, "/page", {}},
        {"http://example.com#fragment",
         "http", "example.com", Port{}, "", {}},
        {"http://example.com?x=1#frag",
         "http", "example.com", Port{}, "", {{"x", "1"}}},
        {"http://example.com/page#first#second",
         "http", "example.com", Port{}, "/page", {}},
        {"http://example.com/page?next=/home#frag",
         "http", "example.com", Port{}, "/page", {{"next", "/home"}}},
    };

    for (const UrlCase& expected : cases) {
        expectUrl(processor, expected);
    }
}

TEST(Components_Network, UrlProcessorInvalidUrls)
{
    const UrlProcessor processor;

    const std::vector<std::string> cases {
        "",
        "http://",
        "http:///path",
        "http://:80/",
        "http://user:pass@/path",
        "http://user@/path",
        "http://user:pass@:80/path",
        "http://example.com:/",
        "http://example.com:65536/",
        "http://example.com:99999/",
        "http://example.com:99999999999999999999/",
        "http://example.com:abc/",
        "http://example.com:-1/",
        "http://example.com:+80/",
        "http://example.com: 80/",
        "http://example.com:80abc/",
        "http://example.com:80:90/",
        "example.com/path",
        "/path",
        "path",
        "?x=1",
        "#fragment",
        "mailto:user@example.com",
        "http:/example.com/path",
        "http//example.com/path",
        "://example.com/path",
        "1http://example.com/",
        "ht~tp://example.com/",
    };

    for (const std::string& source : cases) {
        expectInvalidUrl(processor, source);
    }
}

TEST(Components_Network, UrlProcessorPreChecks)
{
    UrlProcessor processor;

    const std::string httpsUrl = "https://example.com/resource";
    const std::string httpUrl = "http://example.com/resource";
    const std::string localhostUrl = "https://localhost/resource";

    ASSERT_TRUE(processor.parse(httpsUrl).has_value());

    std::string observed;
    processor.addPreCheck("capture", [&observed](std::string_view url) {
        observed = std::string(url);
        return true;
    });

    ASSERT_TRUE(processor.parse(httpsUrl).has_value());
    EXPECT_EQ(observed, httpsUrl);

    processor.addPreCheck("https-only", [](std::string_view url) {
        return url.rfind("https://", 0) == 0;
    });

    EXPECT_FALSE(processor.parse(httpUrl).has_value());
    EXPECT_TRUE(processor.parse(httpsUrl).has_value());

    processor.addPreCheck("deny-localhost", [](std::string_view url) {
        return url.find("localhost") == std::string_view::npos;
    });

    EXPECT_FALSE(processor.parse(localhostUrl).has_value());
    EXPECT_TRUE(processor.parse(httpsUrl).has_value());

    processor.removePreCheck("deny-localhost");
    EXPECT_TRUE(processor.parse(localhostUrl).has_value());

    processor.addPreCheck("https-only", [](std::string_view) { return true; });
    EXPECT_TRUE(processor.parse(httpUrl).has_value());

    processor.removePreCheck("https-only");
    EXPECT_TRUE(processor.parse(httpUrl).has_value());

    processor.removePreCheck("capture");
    EXPECT_TRUE(processor.parse(httpUrl).has_value());
}

TEST(Components_Network, UrlProcessorAvailableTargets)
{
    UrlProcessor processor;

    EXPECT_TRUE(processor.getAvailableTargets().empty());

    processor.addAvailableTarget("/api");
    processor.addAvailableTarget("/search", true);
    processor.addAvailableTarget("/health", false);

    const auto targets = processor.getAvailableTargets();
    ASSERT_EQ(targets.size(), 3u);
    EXPECT_FALSE(targets.at("/api"));
    EXPECT_TRUE(targets.at("/search"));
    EXPECT_FALSE(targets.at("/health"));

    const std::string apiUrl = "http://example.com/api";
    const std::string apiWithArguments = "http://example.com/api?x=1";
    const std::string searchUrl = "http://example.com/search?q=test";
    const std::string searchWithoutArguments = "http://example.com/search";
    const std::string unknownUrl = "http://example.com/unknown";
    const std::string nestedUrl = "http://example.com/api/v1";

    EXPECT_TRUE(processor.parse(apiUrl, true).has_value());
    EXPECT_FALSE(processor.parse(apiWithArguments, true).has_value());
    EXPECT_TRUE(processor.parse(searchUrl, true).has_value());
    EXPECT_TRUE(processor.parse(searchWithoutArguments, true).has_value());
    EXPECT_FALSE(processor.parse(unknownUrl, true).has_value());
    EXPECT_FALSE(processor.parse(nestedUrl, true).has_value());

    EXPECT_TRUE(processor.parse(unknownUrl).has_value());
    EXPECT_TRUE(processor.parse(nestedUrl, false).has_value());

    processor.addAvailableTarget("/api", true);
    const auto updatedTargets = processor.getAvailableTargets();
    EXPECT_TRUE(updatedTargets.at("/api"));
    EXPECT_TRUE(processor.parse(apiWithArguments, true).has_value());

    processor.removeAvailableTarget("/api");
    EXPECT_EQ(processor.getAvailableTargets().count("/api"), 0u);
    EXPECT_FALSE(processor.parse(apiUrl, true).has_value());
    EXPECT_NO_THROW(processor.removeAvailableTarget("/missing"));
}
