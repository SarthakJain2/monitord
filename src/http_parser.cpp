#include "http_parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

namespace http {

HttpRequest HttpParser::Parse(const std::string& raw_request) {
    HttpRequest request;
    
    if (raw_request.empty()) {
        return request;
    }
    
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream request_line(line);
        std::string method_str, path_with_query, version;
        
        request_line >> method_str >> path_with_query >> version;
        
        request.method = ParseMethod(method_str);
        
        // Parse path and query string
        size_t query_pos = path_with_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_with_query.substr(0, query_pos);
            std::string query_string = path_with_query.substr(query_pos + 1);
            ParseQueryParams(query_string, request);
        } else {
            request.path = path_with_query;
        }
        
        request.version = version;
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = Trim(line.substr(0, colon_pos));
            std::string value = Trim(line.substr(colon_pos + 1));
            request.headers[ToLower(key)] = value;
        }
    }
    
    // Parse body
    std::string content_length_str = request.GetHeader("content-length");
    if (!content_length_str.empty()) {
        size_t content_length = std::stoul(content_length_str);
        if (content_length > 0) {
            request.body.resize(content_length);
            stream.read(&request.body[0], content_length);
        }
    } else {
        // Try to read remaining data
        std::ostringstream body_stream;
        body_stream << stream.rdbuf();
        request.body = body_stream.str();
    }
    
    return request;
}

HttpMethod HttpParser::ParseMethod(const std::string& method_str) {
    std::string upper = method_str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    if (upper == "GET") return HttpMethod::GET;
    if (upper == "POST") return HttpMethod::POST;
    if (upper == "PUT") return HttpMethod::PUT;
    if (upper == "DELETE") return HttpMethod::DELETE;
    if (upper == "PATCH") return HttpMethod::PATCH;
    if (upper == "HEAD") return HttpMethod::HEAD;
    if (upper == "OPTIONS") return HttpMethod::OPTIONS;
    
    return HttpMethod::UNKNOWN;
}

std::string HttpParser::MethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

void HttpParser::ParseQueryParams(const std::string& query_string, HttpRequest& request) {
    std::istringstream stream(query_string);
    std::string param;
    
    while (std::getline(stream, param, '&')) {
        size_t equals_pos = param.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = UrlDecode(param.substr(0, equals_pos));
            std::string value = UrlDecode(param.substr(equals_pos + 1));
            request.query_params[key] = value;
        } else {
            request.query_params[UrlDecode(param)] = "";
        }
    }
}

std::string HttpParser::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string HttpParser::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> HttpParser::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;
    
    while (std::getline(stream, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string HttpParser::UrlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            hex_stream >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    
    return result;
}

} // namespace http
