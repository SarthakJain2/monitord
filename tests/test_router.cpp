#include <gtest/gtest.h>
#include "router.hpp"
#include "http_request.hpp"

using namespace http;

TEST(RouterTest, RegisterAndHandleRoute) {
    Router router;
    
    bool handler_called = false;
    router.Register(HttpMethod::GET, "/test", [&handler_called](const HttpRequest& req) {
        handler_called = true;
        return Ok("test response");
    });
    
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = "/test";
    
    HttpResponse response = router.HandleRequest(request);
    
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(response.GetStatus(), HttpStatus::OK);
}

TEST(RouterTest, RouteNotFound) {
    Router router;
    
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = "/nonexistent";
    
    HttpResponse response = router.HandleRequest(request);
    
    EXPECT_EQ(response.GetStatus(), HttpStatus::NOT_FOUND);
}

TEST(RouterTest, MethodMismatch) {
    Router router;
    
    router.Register(HttpMethod::GET, "/test", [](const HttpRequest& req) {
        return Ok("get");
    });
    
    HttpRequest request;
    request.method = HttpMethod::POST;
    request.path = "/test";
    
    HttpResponse response = router.HandleRequest(request);
    
    EXPECT_EQ(response.GetStatus(), HttpStatus::NOT_FOUND);
}

TEST(RouterTest, PathParameters) {
    Router router;
    
    std::string captured_id;
    router.Register(HttpMethod::GET, "/users/:id", [&captured_id](const HttpRequest& req) {
        captured_id = req.query_params.at("id");
        return Ok("user " + captured_id);
    });
    
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = "/users/123";
    
    HttpResponse response = router.HandleRequest(request);
    
    EXPECT_EQ(captured_id, "123");
    EXPECT_EQ(response.GetStatus(), HttpStatus::OK);
}

TEST(RouterTest, HasRoute) {
    Router router;
    
    router.Register(HttpMethod::GET, "/test", [](const HttpRequest& req) {
        return Ok("");
    });
    
    EXPECT_TRUE(router.HasRoute(HttpMethod::GET, "/test"));
    EXPECT_FALSE(router.HasRoute(HttpMethod::POST, "/test"));
    EXPECT_FALSE(router.HasRoute(HttpMethod::GET, "/other"));
}
