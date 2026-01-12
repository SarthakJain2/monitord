#include <gtest/gtest.h>
#include "server.hpp"
#include "config.hpp"
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace http;

TEST(ServerTest, CreateServer) {
    Config config;
    config.port = 0; // Use any available port
    
    Server server(config);
    
    EXPECT_FALSE(server.IsRunning());
}

TEST(ServerTest, RegisterRoutes) {
    Config config;
    Server server(config);
    
    bool get_handler_called = false;
    bool post_handler_called = false;
    
    server.Get("/test", [&get_handler_called](const HttpRequest& req) {
        get_handler_called = true;
        return Ok("get");
    });
    
    server.Post("/test", [&post_handler_called](const HttpRequest& req) {
        post_handler_called = true;
        return Created("post");
    });
    
    // Routes are registered, but we can't easily test without starting server
    // This test mainly ensures the API works
    EXPECT_FALSE(server.IsRunning());
}

TEST(ServerTest, ServerConfiguration) {
    Config config;
    config.port = 9999;
    config.thread_pool_size = 8;
    config.host = "127.0.0.1";
    
    Server server(config);
    
    EXPECT_FALSE(server.IsRunning());
}
