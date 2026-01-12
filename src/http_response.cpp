#include "http_response.hpp"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <iterator>

namespace http {

HttpResponse::HttpResponse() : status_(HttpStatus::OK) {
    SetHeader("Server", "HighPerformanceServer/1.0");
    SetHeader("Connection", "close");
}

HttpResponse::HttpResponse(HttpStatus status) : status_(status) {
    SetHeader("Server", "HighPerformanceServer/1.0");
    SetHeader("Connection", "close");
}

HttpResponse::HttpResponse(HttpStatus status, const std::string& body)
    : status_(status), body_(body) {
    SetHeader("Server", "HighPerformanceServer/1.0");
    SetHeader("Connection", "close");
    SetContentLength(body.length());
}

HttpResponse& HttpResponse::SetStatus(HttpStatus status) {
    status_ = status;
    return *this;
}

HttpResponse& HttpResponse::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

HttpResponse& HttpResponse::SetContentType(const std::string& type) {
    SetHeader("Content-Type", type);
    return *this;
}

HttpResponse& HttpResponse::SetContentLength(size_t length) {
    SetHeader("Content-Length", std::to_string(length));
    return *this;
}

HttpResponse& HttpResponse::SetBody(const std::string& body) {
    body_ = body;
    SetContentLength(body.length());
    return *this;
}

HttpResponse& HttpResponse::SetBody(const std::vector<char>& body) {
    body_.assign(body.begin(), body.end());
    SetContentLength(body_.length());
    return *this;
}

HttpResponse& HttpResponse::Json(const std::string& json) {
    SetContentType("application/json");
    SetBody(json);
    return *this;
}

HttpResponse HttpResponse::FromFile(const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return NotFound("File not found");
    }
    
    if (!std::filesystem::is_regular_file(file_path)) {
        return BadRequest("Path is not a file");
    }
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return InternalError("Failed to open file");
    }
    
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    
    HttpResponse response(HttpStatus::OK);
    response.SetBody(buffer);
    
    // Set content type based on extension
    std::string ext = std::filesystem::path(file_path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == ".html" || ext == ".htm") {
        response.SetContentType("text/html");
    } else if (ext == ".css") {
        response.SetContentType("text/css");
    } else if (ext == ".js") {
        response.SetContentType("application/javascript");
    } else if (ext == ".json") {
        response.SetContentType("application/json");
    } else if (ext == ".png") {
        response.SetContentType("image/png");
    } else if (ext == ".jpg" || ext == ".jpeg") {
        response.SetContentType("image/jpeg");
    } else if (ext == ".gif") {
        response.SetContentType("image/gif");
    } else if (ext == ".svg") {
        response.SetContentType("image/svg+xml");
    } else {
        response.SetContentType("application/octet-stream");
    }
    
    return response;
}

std::string HttpResponse::ToString() const {
    std::ostringstream response;
    
    // Status line
    response << "HTTP/1.1 " << static_cast<int>(status_) << " "
             << GetStatusMessage(status_) << "\r\n";
    
    // Headers
    for (const auto& [key, value] : headers_) {
        response << key << ": " << value << "\r\n";
    }
    
    // Empty line before body
    response << "\r\n";
    
    // Body
    if (!body_.empty()) {
        response << body_;
    }
    
    return response.str();
}

std::string HttpResponse::StatusToString(HttpStatus status) const {
    return std::to_string(static_cast<int>(status));
}

std::string HttpResponse::GetStatusMessage(HttpStatus status) const {
    switch (status) {
        case HttpStatus::OK: return "OK";
        case HttpStatus::CREATED: return "Created";
        case HttpStatus::NO_CONTENT: return "No Content";
        case HttpStatus::SWITCHING_PROTOCOLS: return "Switching Protocols";
        case HttpStatus::BAD_REQUEST: return "Bad Request";
        case HttpStatus::UNAUTHORIZED: return "Unauthorized";
        case HttpStatus::FORBIDDEN: return "Forbidden";
        case HttpStatus::NOT_FOUND: return "Not Found";
        case HttpStatus::METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HttpStatus::INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HttpStatus::NOT_IMPLEMENTED: return "Not Implemented";
        case HttpStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

// Helper functions
HttpResponse Ok(const std::string& body) {
    return HttpResponse(HttpStatus::OK, body);
}

HttpResponse Created(const std::string& body) {
    return HttpResponse(HttpStatus::CREATED, body);
}

HttpResponse NotFound(const std::string& message) {
    HttpResponse response(HttpStatus::NOT_FOUND);
    response.SetContentType("text/plain");
    response.SetBody(message);
    return response;
}

HttpResponse BadRequest(const std::string& message) {
    HttpResponse response(HttpStatus::BAD_REQUEST);
    response.SetContentType("text/plain");
    response.SetBody(message);
    return response;
}

HttpResponse InternalError(const std::string& message) {
    HttpResponse response(HttpStatus::INTERNAL_SERVER_ERROR);
    response.SetContentType("text/plain");
    response.SetBody(message);
    return response;
}

HttpResponse JsonResponse(const std::string& json, HttpStatus status) {
    HttpResponse response(status);
    response.Json(json);
    return response;
}

HttpResponse Forbidden(const std::string& message) {
    HttpResponse response(HttpStatus::FORBIDDEN);
    response.SetContentType("text/plain");
    response.SetBody(message);
    return response;
}

} // namespace http
