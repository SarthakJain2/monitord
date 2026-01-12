#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace http {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS,
    UNKNOWN
};

struct HttpRequest {
    HttpMethod method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> query_params;

    std::string GetHeader(const std::string& key) const {
        auto it = headers.find(key);
        return (it != headers.end()) ? it->second : "";
    }
};

class HttpParser {
public:
    static HttpRequest Parse(const std::string& raw_request);
    static HttpMethod ParseMethod(const std::string& method_str);
    static std::string MethodToString(HttpMethod method);
    static void ParseQueryParams(const std::string& query_string, HttpRequest& request);

private:
    static std::string Trim(const std::string& str);
    static std::string ToLower(const std::string& str);
    static std::vector<std::string> Split(const std::string& str, char delimiter);
    static std::string UrlDecode(const std::string& str);
};

} // namespace http
