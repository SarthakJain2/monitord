# Real-Time System Monitoring Dashboard

A complete, production-ready system monitoring solution built with a high-performance HTTP server in modern C++17. This project provides real-time monitoring of system resources (CPU, memory, disk, network) with a beautiful web dashboard, WebSocket support for live updates, and intelligent alerting.

**Purpose**: Monitor system health in real-time with automatic alerts when resources exceed thresholds. Perfect for server monitoring, performance analysis, and system administration.

## Features

### Core Monitoring

- **Real-Time System Metrics**: CPU, memory, disk, disk I/O, and network monitoring
- **Live Web Dashboard**: Beautiful, responsive dashboard with interactive charts
- **WebSocket Support**: Real-time metric updates via WebSocket connections
- **Time-Series Storage**: Historical data storage with configurable retention
- **Intelligent Alerting**: Automatic alerts when thresholds are exceeded
- **REST API**: Complete API for querying metrics and alerts

### Technical Excellence

- **Async I/O Event Loop**: Uses kqueue (macOS) for efficient event-driven I/O
- **Thread Pool**: Configurable thread pool for concurrent request handling
- **HTTP/1.1 Support**: Full HTTP request parsing and response generation
- **Systems Programming**: Direct OS-level metric collection (mach APIs, sysctl)
- **Modern C++17**: Smart pointers, move semantics, templates, lambdas
- **Comprehensive Testing**: Full test suite using GoogleTest

## Architecture

### Core Components

1. **EventLoop**: Async I/O event loop using kqueue for efficient event handling
2. **ThreadPool**: Worker thread pool for processing HTTP requests concurrently
3. **HttpParser**: Complete HTTP/1.1 request parser with header and body support
4. **HttpResponse**: HTTP response builder with status codes and headers
5. **Router**: Flexible routing system with path parameters (`/users/:id`)
6. **Server**: Main server class that orchestrates all components
7. **Logger**: Thread-safe logging system
8. **Connection**: Connection management with socket operations
9. **MetricsCollector**: System metrics collection (CPU, memory, disk, disk I/O, network) using macOS APIs
10. **MetricsStorage**: Time-series storage for historical metric data
11. **AlertManager**: Threshold-based alerting system
12. **WebSocket**: WebSocket protocol implementation for real-time updates

### Design Patterns

- **RAII**: All resources are managed through smart pointers and RAII
- **Move Semantics**: Efficient resource transfer using move constructors
- **Template Metaprogramming**: Generic thread pool with futures
- **Observer Pattern**: Event-driven architecture with callbacks
- **Strategy Pattern**: Pluggable route handlers

## Building

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15 or higher
- Make or Ninja build system

### Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
make

# Run tests
make test
# or
ctest

# Build benchmarks (optional)
cmake -DBUILD_BENCHMARKS=ON ..
make benchmarks
```

### Build Options

- `BUILD_TESTS`: Build test suite (default: ON)
- `BUILD_BENCHMARKS`: Build benchmark tools (default: ON)

## Usage

### Basic Server Example

```cpp
#include "server.hpp"
#include "http_response.hpp"

using namespace http;

int main() {
    Config config;
    config.port = 8080;
    config.thread_pool_size = 4;

    Server server(config);

    // Register routes
    server.Get("/", [](const HttpRequest& req) {
        HttpResponse response(HttpStatus::OK);
        response.SetBody("Hello, World!");
        return response;
    });

    server.Get("/api/users", [](const HttpRequest& req) {
        return JsonResponse(R"([{"id": 1, "name": "Alice"}])");
    });

    server.Start();
    return 0;
}
```

### Running the Monitoring Server

```bash
# Run with default settings (port 8080, 4 threads)
cd build && ./server

# Run with custom port and thread count
./server 8080 4

# Open dashboard in browser (macOS)
open http://localhost:8080

# Or visit in any browser
# http://localhost:8080
```

### Dashboard & API Endpoints

**Web Dashboard:**

- `GET /` - Real-time monitoring dashboard with live charts

**Metrics API:**

- `GET /api/metrics/latest` - Get latest system metrics (JSON)
- `GET /api/metrics/range?seconds=300` - Get metrics for time range (default: 5 minutes)
- `GET /api/metrics/stats?seconds=3600` - Get aggregated statistics (default: 1 hour)

**Alerts API:**

- `GET /api/alerts` - Get all active alerts (JSON array)

**WebSocket:**

- `ws://localhost:8080/ws/metrics` - Real-time metric stream (updates every second, JSON format)

**Health Check:**

- `GET /health` - Health check endpoint

### System Metrics Collected

- **CPU Usage**: Percentage of CPU time spent in user and system modes (matches Activity Monitor)
- **Memory Usage**: Memory used percentage calculated as `(Active + Wired + Compressed) / Total` (matches Activity Monitor)
- **Disk Usage**: Disk capacity percentage calculated as `Used / (Used + Available)` (matches macOS `df` command)
- **Disk I/O**: Read/write operations, data transferred, and current I/O rates (matches Activity Monitor style)
  - Cumulative reads/writes since boot
  - Cumulative data read/written (TB)
  - Current read/write rates (ops/sec)
  - Current data read/write rates (KB/s or MB/s)
- **Network**: RX/TX bytes, packets, and current rates (KB/s or MB/s)

## Configuration

### Environment Variables

- `SERVER_HOST`: Server host address (default: "0.0.0.0")
- `SERVER_PORT`: Server port (default: 8080)
- `THREAD_POOL_SIZE`: Number of worker threads (default: 4)
- `MAX_CONNECTIONS`: Maximum concurrent connections (default: 1000)
- `LOG_FILE`: Log file path (default: console only)
- `STATIC_DIRECTORY`: Directory for static file serving

### Configuration File

Create a `config.txt` file:

```
host=0.0.0.0
port=8080
thread_pool_size=8
max_connections=1000
log_file=server.log
static_directory=/var/www/html
```

## Testing

The project includes comprehensive unit tests:

```bash
# Run all tests
./tests

# Run specific test
./tests --gtest_filter=HttpParserTest.*
```

### Test Coverage

- HTTP request parsing (methods, headers, body, query params)
- HTTP response generation
- Router functionality (path matching, parameters)
- Thread pool concurrency
- Server configuration

## Benchmarks

Run performance benchmarks:

```bash
./benchmarks
```

The benchmark suite tests:

- Single-threaded performance
- Multi-threaded performance
- High-load scenarios
- Request throughput (req/s)
- Average latency

## Performance Characteristics

- **Concurrency**: Handles thousands of concurrent connections
- **Throughput**: High request/second rate with thread pool
- **Latency**: Low-latency response times with async I/O
- **Memory**: Efficient memory usage with RAII and move semantics
- **Scalability**: Configurable thread pool and connection limits

## Code Quality

### Modern C++ Features

- C++17 standard compliance
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Move semantics and perfect forwarding
- Lambda expressions and closures
- `std::future` and `std::async` for async operations
- `std::filesystem` for file operations
- Template metaprogramming

### Best Practices

- RAII for resource management
- Exception safety guarantees
- Thread-safe logging
- Const correctness
- Clear separation of concerns
- Comprehensive error handling

## Project Structure

```
.
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── server.hpp
│   ├── event_loop.hpp
│   ├── thread_pool.hpp
│   ├── http_parser.hpp
│   ├── http_request.hpp
│   ├── http_response.hpp
│   ├── router.hpp
│   ├── logger.hpp
│   ├── config.hpp
│   ├── connection.hpp
│   ├── metrics_collector.hpp
│   ├── metrics_storage.hpp
│   ├── alert_manager.hpp
│   └── websocket.hpp
├── src/                    # Implementation files
│   ├── main.cpp            # Main entry point with dashboard and API routes
│   ├── server.cpp
│   ├── event_loop.cpp
│   ├── thread_pool.cpp
│   ├── http_parser.cpp
│   ├── http_response.cpp
│   ├── router.cpp
│   ├── logger.cpp
│   ├── config.cpp
│   ├── connection.cpp
│   ├── metrics_collector.cpp
│   ├── metrics_storage.cpp
│   ├── alert_manager.cpp
│   └── websocket.cpp
├── tests/                  # Unit tests
│   ├── test_http_parser.cpp
│   ├── test_http_response.cpp
│   ├── test_router.cpp
│   ├── test_thread_pool.cpp
│   └── test_server.cpp
└── benchmarks/            # Performance benchmarks
    └── benchmark_server.cpp
```

## Example Requests

### Using curl

```bash
# Simple GET request
curl http://localhost:8080/

# JSON API
curl http://localhost:8080/api/users

# Query parameters
curl "http://localhost:8080/api/echo?message=hello"

# POST request
curl -X POST http://localhost:8080/api/users \
  -H "Content-Type: application/json" \
  -d '{"name": "Bob", "email": "bob@example.com"}'

# Path parameters
curl http://localhost:8080/api/users/123
```

## License

This project is provided as-is for demonstration purposes.

## Author

Built to demonstrate advanced C++ programming skills for systems-level development.
