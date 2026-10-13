#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace spriteface {
struct FaceFrame {
    std::uint32_t seq = 0;
    bool tracking = false;
    float confidence = 0.0f;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f; // radians
    float left_eye = 1.0f, right_eye = 1.0f;     // 0 closed, 1 open
    float mouth_open = 0.0f, mouth_smile = 0.0f;
};
struct FaceSnapshot {
    FaceFrame frame;
    bool fresh = false;
    double age_seconds = 0.0;
    std::uint64_t accepted = 0, rejected = 0;
    std::string peer;
};
// Validated SF1 JSON datagrams. Throws std::invalid_argument on malformed input.
FaceFrame parse_face_packet(std::string_view packet, std::string_view expected_token);
std::string encode_face_packet(const FaceFrame& frame, std::string_view token);
std::string make_session_token();
std::string normalize_session_token(std::string_view token);
std::string make_pairing_uri(std::string_view host, std::uint16_t port, std::string_view token);

class FaceReceiver {
public:
    FaceReceiver();
    ~FaceReceiver();
    FaceReceiver(const FaceReceiver&) = delete;
    FaceReceiver& operator=(const FaceReceiver&) = delete;
    // Binds an IPv4 address; port=0 is useful for tests. Throws on bind failure.
    void start(const std::string& bind_address, std::uint16_t port, const std::string& token);
    void stop();
    FaceSnapshot snapshot(std::chrono::milliseconds timeout = std::chrono::milliseconds(800)) const;
    std::uint16_t port() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Frame-rate-independent smoothing; lost tracking decays toward neutral.
class FaceFilter {
public:
    FaceFrame update(const FaceSnapshot& sample, float dt_seconds);
    void reset();
private:
    FaceFrame state_;
};
} // namespace spriteface
