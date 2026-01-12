#include "router.hpp"
#include "http_response.hpp"
#include <sstream>
#include <regex>

namespace http {

Router::Router() = default;

void Router::Register(HttpMethod method, const std::string& path, RouteHandler handler) {
    Route route;
    route.method = method;
    route.pattern = path;
    route.handler = std::move(handler);
    
    route.regex_pattern = std::regex(PathToRegex(path, route.param_names));
    
    routes_.push_back(std::move(route));
}

HttpResponse Router::HandleRequest(const HttpRequest& request) const {
    for (const auto& route : routes_) {
        if (route.method != request.method) {
            continue;
        }
        
        std::smatch match;
        if (std::regex_match(request.path, match, route.regex_pattern)) {
            // Extract path parameters
            HttpRequest modified_request = request;
            for (size_t i = 0; i < route.param_names.size() && i + 1 < match.size(); ++i) {
                modified_request.query_params[route.param_names[i]] = match[i + 1].str();
            }
            
            return route.handler(modified_request);
        }
    }
    
    // No route found
    return NotFound("Route not found");
}

bool Router::HasRoute(HttpMethod method, const std::string& path) const {
    for (const auto& route : routes_) {
        if (route.method == method) {
            std::smatch match;
            if (std::regex_match(path, match, route.regex_pattern)) {
                return true;
            }
        }
    }
    return false;
}

std::string Router::PathToRegex(const std::string& path, std::vector<std::string>& param_names) const {
    // Handle root path
    if (path == "/") {
        return "^/$";
    }
    
    std::ostringstream regex;
    regex << "^";
    
    std::istringstream stream(path);
    std::string segment;
    
    while (std::getline(stream, segment, '/')) {
        if (segment.empty()) continue;
        
        if (segment[0] == ':') {
            // Parameter
            std::string param_name = segment.substr(1);
            param_names.push_back(param_name);
            regex << "/([^/]+)";
        } else {
            // Literal
            regex << "/" << segment;
        }
    }
    
    regex << "$";
    return regex.str();
}

std::unordered_map<std::string, std::string> Router::ExtractParams(
    const Route& route,
    const std::string& path
) const {
    std::unordered_map<std::string, std::string> params;
    std::smatch match;
    
    if (std::regex_match(path, match, route.regex_pattern)) {
        for (size_t i = 0; i < route.param_names.size() && i + 1 < match.size(); ++i) {
            params[route.param_names[i]] = match[i + 1].str();
        }
    }
    
    return params;
}

} // namespace http
