#include <gtest/gtest.h>
#include "http_response.hpp"

using namespace http;

TEST(HttpResponseTest, BasicResponse) {
    HttpResponse response(HttpStatus::OK, "Hello World");
    
    std::string str = response.ToString();
    
    EXPECT_NE(str.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(str.find("Hello World"), std::string::npos);
    EXPECT_NE(str.find("Content-Length: 11"), std::string::npos);
}

TEST(HttpResponseTest, SetHeaders) {
    HttpResponse response;
    response.SetHeader("X-Custom-Header", "test-value");
    response.SetContentType("application/json");
    
    std::string str = response.ToString();
    
    EXPECT_NE(str.find("X-Custom-Header: test-value"), std::string::npos);
    EXPECT_NE(str.find("Content-Type: application/json"), std::string::npos);
}

TEST(HttpResponseTest, JsonResponse) {
    HttpResponse response;
    response.Json(R"({"key": "value"})");
    
    std::string str = response.ToString();
    
    EXPECT_NE(str.find("Content-Type: application/json"), std::string::npos);
    EXPECT_NE(str.find(R"({"key": "value"})"), std::string::npos);
}

TEST(HttpResponseTest, NotFoundHelper) {
    HttpResponse response = NotFound("Resource not found");
    
    EXPECT_EQ(response.GetStatus(), HttpStatus::NOT_FOUND);
    EXPECT_NE(response.GetBody().find("Resource not found"), std::string::npos);
}

TEST(HttpResponseTest, BadRequestHelper) {
    HttpResponse response = BadRequest("Invalid input");
    
    EXPECT_EQ(response.GetStatus(), HttpStatus::BAD_REQUEST);
    EXPECT_NE(response.GetBody().find("Invalid input"), std::string::npos);
}

TEST(HttpResponseTest, JsonResponseHelper) {
    HttpResponse response = JsonResponse(R"({"status": "ok"})");
    
    EXPECT_EQ(response.GetStatus(), HttpStatus::OK);
    std::string str = response.ToString();
    EXPECT_NE(str.find("Content-Type: application/json"), std::string::npos);
}
