#include "websocket.hpp"
#include <sstream>
#include <regex>
#include <cstring>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

// Use macOS CommonCrypto instead of OpenSSL

namespace http {

bool WebSocket::IsWebSocketRequest(const std::string& request) {
    return request.find("Upgrade: websocket") != std::string::npos ||
           request.find("upgrade: websocket") != std::string::npos;
}

std::string WebSocket::ExtractKey(const std::string& request) {
    std::regex key_regex(R"(Sec-WebSocket-Key:\s*([^\r\n]+))", std::regex::icase);
    std::smatch match;
    
    if (std::regex_search(request, match, key_regex)) {
        std::string key = match[1].str();
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        return key;
    }
    
    return "";
}

std::string WebSocket::GenerateAcceptKey(const std::string& client_key) {
    const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = client_key + magic_string;
    
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), 
            static_cast<CC_LONG>(combined.length()), hash);
    
    return Base64Encode(std::string(reinterpret_cast<const char*>(hash), CC_SHA1_DIGEST_LENGTH));
}

std::string WebSocket::GenerateHandshakeResponse(const std::string& request) {
    std::string key = ExtractKey(request);
    if (key.empty()) {
        return "";
    }
    
    std::string accept_key = GenerateAcceptKey(key);
    
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
             << "\r\n";
    
    return response.str();
}

std::vector<uint8_t> WebSocket::EncodeFrame(const std::string& message, Opcode opcode) {
    std::vector<uint8_t> frame;
    
    // First byte: FIN (1), RSV (000), Opcode (4 bits)
    uint8_t first_byte = 0x80 | static_cast<uint8_t>(opcode);
    frame.push_back(first_byte);
    
    // Second byte: MASK (0 for server), Payload length
    size_t payload_len = message.length();
    
    if (payload_len < 126) {
        frame.push_back(static_cast<uint8_t>(payload_len));
    } else if (payload_len < 65536) {
        frame.push_back(126);
        frame.push_back((payload_len >> 8) & 0xFF);
        frame.push_back(payload_len & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((payload_len >> (i * 8)) & 0xFF);
        }
    }
    
    // Payload
    frame.insert(frame.end(), message.begin(), message.end());
    
    return frame;
}

std::string WebSocket::DecodeFrame(const std::vector<uint8_t>& frame) {
    if (frame.size() < 2) {
        return "";
    }
    
    size_t payload_offset = 2;
    size_t payload_len = frame[1] & 0x7F;
    
    if (payload_len == 126) {
        if (frame.size() < 4) return "";
        payload_len = (frame[2] << 8) | frame[3];
        payload_offset = 4;
    } else if (payload_len == 127) {
        if (frame.size() < 10) return "";
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | frame[2 + i];
        }
        payload_offset = 10;
    }
    
    if (frame.size() < payload_offset + payload_len) {
        return "";
    }
    
    return std::string(frame.begin() + payload_offset, frame.begin() + payload_offset + payload_len);
}

std::string WebSocket::Base64Encode(const std::string& input) {
    // Simple base64 encoding
    const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    int val = 0, valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6) {
        result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    
    while (result.length() % 4) {
        result.push_back('=');
    }
    
    return result;
}

std::string WebSocket::SHA1(const std::string& input) {
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), 
            static_cast<CC_LONG>(input.length()), hash);
    return std::string(reinterpret_cast<const char*>(hash), CC_SHA1_DIGEST_LENGTH);
}

} // namespace http
