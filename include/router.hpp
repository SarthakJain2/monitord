#pragma once

#include "http_request.hpp"
#include "http_response.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <regex>

namespace http {

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

struct Route {
    HttpMethod method;
    std::string pattern;
    std::regex regex_pattern;
    RouteHandler handler;
    std::vector<std::string> param_names;
};

class Router {
public:
    Router();

    // Register routes
    void Register(HttpMethod method, const std::string& path, RouteHandler handler);

    // Find and execute route handler
    HttpResponse HandleRequest(const HttpRequest& request) const;

    // Check if route exists
    bool HasRoute(HttpMethod method, const std::string& path) const;

private:
    std::string PathToRegex(const std::string& path, std::vector<std::string>& param_names) const;
    std::unordered_map<std::string, std::string> ExtractParams(
        const Route& route,
        const std::string& path
    ) const;

    std::vector<Route> routes_;
};

} // namespace http
