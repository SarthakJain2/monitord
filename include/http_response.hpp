#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace http {

enum class HttpStatus {
    OK = 200,
    CREATED = 201,
    NO_CONTENT = 204,
    SWITCHING_PROTOCOLS = 101,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    SERVICE_UNAVAILABLE = 503
};

class HttpResponse {
public:
    HttpResponse();
    explicit HttpResponse(HttpStatus status);
    HttpResponse(HttpStatus status, const std::string& body);

    // Set status
    HttpResponse& SetStatus(HttpStatus status);

    // Set headers
    HttpResponse& SetHeader(const std::string& key, const std::string& value);
    HttpResponse& SetContentType(const std::string& type);
    HttpResponse& SetContentLength(size_t length);

    // Set body
    HttpResponse& SetBody(const std::string& body);
    HttpResponse& SetBody(const std::vector<char>& body);

    // JSON response
    HttpResponse& Json(const std::string& json);

    // Static file response
    static HttpResponse FromFile(const std::string& file_path);

    // Build HTTP response string
    std::string ToString() const;

    HttpStatus GetStatus() const { return status_; }
    const std::string& GetBody() const { return body_; }

private:
    std::string StatusToString(HttpStatus status) const;
    std::string GetStatusMessage(HttpStatus status) const;

    HttpStatus status_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

// Helper functions for common responses
HttpResponse Ok(const std::string& body = "");
HttpResponse Created(const std::string& body = "");
HttpResponse NotFound(const std::string& message = "Not Found");
HttpResponse BadRequest(const std::string& message = "Bad Request");
HttpResponse Forbidden(const std::string& message = "Forbidden");
HttpResponse InternalError(const std::string& message = "Internal Server Error");
HttpResponse JsonResponse(const std::string& json, HttpStatus status = HttpStatus::OK);

} // namespace http
