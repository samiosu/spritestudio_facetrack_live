#include "spriteface/face.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
const std::string token = "0123456789abcdef0123456789abcdef";
void expect(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<typename Fn> void rejects(Fn fn) {
    try { fn(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid input was accepted");
}
void eventually(const std::function<bool()>& condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("receiver timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void parser_test() {
    spriteface::FaceFrame frame;
    frame.seq = UINT32_MAX;
    frame.tracking = true;
    frame.confidence = 0.9f;
    frame.yaw = -0.2f;
    frame.left_eye = 0.1f;
    frame.mouth_open = 0.7f;
    const auto encoded = spriteface::encode_face_packet(frame, token);
    const auto decoded = spriteface::parse_face_packet(encoded, token);
    expect(decoded.seq == UINT32_MAX && decoded.tracking, "sequence/tracking roundtrip failed");
    expect(std::abs(decoded.mouth_open - 0.7f) < 1e-6f, "face roundtrip failed");
    expect(spriteface::parse_face_packet(encoded, "0123456789ABCDEF0123456789ABCDEF").tracking, "hex token normalization failed");
    auto j = nlohmann::json::parse(encoded);
    for (const auto& bad : {"{", "[]", "null", "", "{\"v\":NaN}"}) rejects([&] { spriteface::parse_face_packet(bad, token); });
    rejects([&] { spriteface::parse_face_packet(std::string(2049, ' '), token); });
    rejects([&] { spriteface::parse_face_packet(encoded, "ffffffffffffffffffffffffffffffff"); });
    auto mutate = [&](const std::function<void(nlohmann::json&)>& fn) {
        auto bad = j; fn(bad); rejects([&] { spriteface::parse_face_packet(bad.dump(), token); });
    };
    mutate([](auto& b) { b["v"] = 2; });
    mutate([](auto& b) { b.erase("type"); });
    mutate([](auto& b) { b["seq"] = -1; });
    mutate([](auto& b) { b["seq"] = std::uint64_t(UINT32_MAX) + 1; });
    mutate([](auto& b) { b["seq"] = 1.5; });
    mutate([](auto& b) { b["tracking"] = "true"; });
    mutate([](auto& b) { b["eyes"]["left"] = 1.1; });
    mutate([](auto& b) { b["head"]["roll"] = 8.0; });
    mutate([](auto& b) { b.erase("mouth"); });
    mutate([](auto& b) { b["mouth"]["open"] = nullptr; });
    j["tracking"] = false;
    const auto lost = spriteface::parse_face_packet(j.dump(), token);
    expect(!lost.tracking && lost.yaw == 0 && lost.left_eye == 1 && lost.mouth_open == 0, "lost tracking did not become neutral");
    expect(spriteface::make_pairing_uri("192.168.1.2", 11574, token).find("host=192.168.1.2") != std::string::npos, "invalid pairing URI");
    for (const auto& host : {"0.0.0.0", "0.1.2.3", "224.1.1.1", "192.168.01.2", "256.1.1.1", "127.0.0.", "example.com", "127.0.0.1&token=x"})
        rejects([&] { spriteface::make_pairing_uri(host, 11574, token); });
    const auto generated = spriteface::make_session_token();
    expect(generated.size() == 32 && generated.find_first_not_of("0123456789abcdef") == std::string::npos, "invalid generated token");
}
void filter_test() {
    spriteface::FaceSnapshot sample;
    sample.fresh = sample.frame.tracking = true;
    sample.frame.yaw = 0.4f; sample.frame.left_eye = 0; sample.frame.mouth_open = 1;
    spriteface::FaceFilter one, many;
    const auto a = one.update(sample, 0.1f);
    spriteface::FaceFrame b;
    for (int i = 0; i < 10; ++i) b = many.update(sample, 0.01f);
    expect(std::abs(a.yaw - b.yaw) < 1e-5, "filter depends on render frame rate");
    sample.fresh = false;
    for (int i = 0; i < 60; ++i) b = many.update(sample, 1.0f / 60.0f);
    expect(!b.tracking && std::abs(b.yaw) < 1e-5 && b.left_eye > 0.9999 && b.mouth_open < 1e-5, "stale filter did not return to neutral");
}
struct Sender {
#ifdef _WIN32
    SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ~Sender() { closesocket(socket); }
#else
    int socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ~Sender() { close(socket); }
#endif
    sockaddr_in target{};
    explicit Sender(std::uint16_t port) {
        target.sin_family = AF_INET; target.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &target.sin_addr);
    }
    void send(const std::string& bytes) {
        const auto n = sendto(socket, bytes.data(), static_cast<int>(bytes.size()), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
        expect(n == static_cast<int>(bytes.size()), "UDP send failed");
    }
    std::string reply() {
        fd_set set; FD_ZERO(&set); FD_SET(socket, &set); timeval timeout{1,0};
        expect(select(static_cast<int>(socket) + 1, &set, nullptr, nullptr, &timeout) > 0, "no pairing response");
        char bytes[256]{};
        const int n = static_cast<int>(recv(socket, bytes, sizeof(bytes), 0));
        expect(n > 0, "empty pairing response"); return std::string(bytes, n);
    }
};
void receiver_test() {
    spriteface::FaceReceiver receiver;
    rejects([&] { receiver.start("not-an-ip", 0, token); });
    receiver.start("127.0.0.1", 0, "0123456789ABCDEF0123456789ABCDEF");
    expect(receiver.port() != 0 && !receiver.snapshot().fresh, "initial receiver state is wrong");
    {
        Sender sender(receiver.port()), other(receiver.port());
        sender.send("{\"v\":1,\"type\":\"hello\",\"token\":\"" + token + "\"}");
        expect(nlohmann::json::parse(sender.reply())["type"] == "ready", "pairing handshake failed");
        spriteface::FaceFrame frame; frame.tracking = true; frame.confidence = 1; frame.seq = UINT32_MAX;
        sender.send(spriteface::encode_face_packet(frame, token));
        eventually([&] { return receiver.snapshot().accepted == 1; });
        frame.seq = 0;
        sender.send(spriteface::encode_face_packet(frame, token));
        eventually([&] { return receiver.snapshot().accepted == 2; });
        sender.send(spriteface::encode_face_packet(frame, token)); // Duplicate.
        frame.seq = UINT32_MAX;
        sender.send(spriteface::encode_face_packet(frame, token)); // Out of order.
        frame.seq = 1;
        other.send(spriteface::encode_face_packet(frame, token)); // Different active peer.
        sender.send(spriteface::encode_face_packet(frame, "ffffffffffffffffffffffffffffffff"));
        eventually([&] { return receiver.snapshot().rejected == 4; });
        expect(receiver.snapshot().accepted == 2 && receiver.snapshot().fresh, "invalid frame changed state");
        other.send("{\"v\":1,\"type\":\"hello\",\"token\":\"" + token + "\"}");
        expect(nlohmann::json::parse(other.reply())["type"] == "busy", "active peer was not protected");
        eventually([&] { return !receiver.snapshot().fresh; });
        other.send(spriteface::encode_face_packet(frame, token));
        eventually([&] { return receiver.snapshot().accepted == 3; });
        expect(receiver.snapshot().frame.seq == 1 && receiver.snapshot().fresh, "stale peer handover failed");
        frame.seq = 2; frame.tracking = false;
        other.send(spriteface::encode_face_packet(frame, token));
        eventually([&] { return receiver.snapshot().accepted == 4; });
        expect(!receiver.snapshot().frame.tracking, "explicit tracking loss failed");
    }
    const auto before = std::chrono::steady_clock::now();
    receiver.stop();
    expect(std::chrono::steady_clock::now() - before < std::chrono::seconds(1), "receiver shutdown blocked");
    expect(!receiver.snapshot().fresh, "stopped receiver remained fresh");
    receiver.start("127.0.0.1", 0, token);
    expect(receiver.snapshot().accepted == 0, "receiver restart did not clear state");
}
}
int main() {
    try { parser_test(); filter_test(); receiver_test(); std::cout << "Protocol, filtering, and loopback receiver tests passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
