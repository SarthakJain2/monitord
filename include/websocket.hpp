#pragma once

#include <string>
#include <functional>
#include <vector>
#include <cstdint>

namespace http {

class WebSocket {
public:
    enum class Opcode {
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA
    };
    
    // Check if request is a WebSocket upgrade request
    static bool IsWebSocketRequest(const std::string& request);
    
    // Generate WebSocket handshake response
    static std::string GenerateHandshakeResponse(const std::string& request);
    
    // Extract WebSocket key from request
    static std::string ExtractKey(const std::string& request);
    
    // Generate accept key from client key
    static std::string GenerateAcceptKey(const std::string& client_key);
    
    // Encode message to WebSocket frame
    static std::vector<uint8_t> EncodeFrame(const std::string& message, Opcode opcode = Opcode::Text);
    
    // Decode WebSocket frame to message
    static std::string DecodeFrame(const std::vector<uint8_t>& frame);
    
    // Simple base64 encoding (for handshake)
    static std::string Base64Encode(const std::string& input);
    
    // SHA-1 hash (for handshake)
    static std::string SHA1(const std::string& input);
};

} // namespace http
