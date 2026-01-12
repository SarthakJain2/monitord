#include <gtest/gtest.h>
#include "http_parser.hpp"

using namespace http;

TEST(HttpParserTest, ParseGetRequest) {
    std::string request = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "User-Agent: test\r\n"
        "\r\n";
    
    HttpRequest req = HttpParser::Parse(request);
    
    EXPECT_EQ(req.method, HttpMethod::GET);
    EXPECT_EQ(req.path, "/api/users");
    EXPECT_EQ(req.version, "HTTP/1.1");
    EXPECT_EQ(req.GetHeader("host"), "localhost:8080");
    EXPECT_EQ(req.GetHeader("user-agent"), "test");
}

TEST(HttpParserTest, ParsePostRequestWithBody) {
    std::string request = 
        "POST /api/users HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 17\r\n"
        "\r\n"
        "{\"name\": \"Alice\"}";
    
    HttpRequest req = HttpParser::Parse(request);
    
    EXPECT_EQ(req.method, HttpMethod::POST);
    EXPECT_EQ(req.path, "/api/users");
    EXPECT_EQ(req.GetHeader("content-type"), "application/json");
    EXPECT_EQ(req.body, "{\"name\": \"Alice\"}");
}

TEST(HttpParserTest, ParseQueryParameters) {
    std::string request = 
        "GET /api/search?q=test&page=1 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "\r\n";
    
    HttpRequest req = HttpParser::Parse(request);
    
    EXPECT_EQ(req.path, "/api/search");
    EXPECT_EQ(req.query_params["q"], "test");
    EXPECT_EQ(req.query_params["page"], "1");
}

TEST(HttpParserTest, ParseUnknownMethod) {
    std::string request = 
        "CUSTOM /path HTTP/1.1\r\n"
        "\r\n";
    
    HttpRequest req = HttpParser::Parse(request);
    
    EXPECT_EQ(req.method, HttpMethod::UNKNOWN);
}

TEST(HttpParserTest, MethodToString) {
    EXPECT_EQ(HttpParser::MethodToString(HttpMethod::GET), "GET");
    EXPECT_EQ(HttpParser::MethodToString(HttpMethod::POST), "POST");
    EXPECT_EQ(HttpParser::MethodToString(HttpMethod::PUT), "PUT");
    EXPECT_EQ(HttpParser::MethodToString(HttpMethod::DELETE), "DELETE");
}
