#include "spriteface/face.hpp"
#include "spriteface/secure_packet.hpp"
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
void encryption_test() {
    const std::string plaintext = "{\"type\":\"hello\"}";
    const auto encrypted = spriteface::seal_packet(plaintext, token);
    expect(encrypted.substr(0, 3) == "SF2" && encrypted.find(plaintext) == std::string::npos,
           "packet was not encrypted");
    expect(spriteface::open_packet(encrypted, token) == plaintext, "AES-GCM roundtrip failed");
    rejects([&] { spriteface::open_packet(plaintext, token); });
    rejects([&] { spriteface::open_packet(encrypted, "ffffffffffffffffffffffffffffffff"); });
    auto tampered = encrypted;
    tampered.back() ^= 1;
    rejects([&] { spriteface::open_packet(tampered, token); });
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
        const std::string client = "11111111111111111111111111111111";
        const std::string other_client = "22222222222222222222222222222222";
        const auto hello = [&](const std::string& id) {
            return nlohmann::json{{"v", 1}, {"type", "hello"}, {"token", token}, {"client", id}}.dump();
        };
        sender.send(hello(client)); // Plaintext is never accepted.
        eventually([&] { return receiver.snapshot().rejected == 1; });
        sender.send(spriteface::seal_packet(hello(client), token));
        const auto ready = nlohmann::json::parse(spriteface::open_packet(sender.reply(), token));
        expect(ready["type"] == "ready" && ready["client"] == client, "pairing handshake failed");
        const std::string server = ready["server"];
        auto frame_packet = [&](const spriteface::FaceFrame& frame, const std::string& key,
                                const std::string& client_id, const std::string& server_id) {
            auto payload = nlohmann::json::parse(spriteface::encode_face_packet(frame, key));
            payload["client"] = client_id;
            payload["server"] = server_id;
            return spriteface::seal_packet(payload.dump(), key);
        };
        spriteface::FaceFrame frame; frame.tracking = true; frame.confidence = 1; frame.seq = UINT32_MAX;
        sender.send(frame_packet(frame, token, client, server));
        eventually([&] { return receiver.snapshot().accepted == 1; });
        frame.seq = 0;
        const auto replay = frame_packet(frame, token, client, server);
        sender.send(replay);
        eventually([&] { return receiver.snapshot().accepted == 2; });
        sender.send(replay); // Duplicate ciphertext, including nonce.
        frame.seq = UINT32_MAX;
        sender.send(frame_packet(frame, token, client, server)); // Out of order.
        frame.seq = 1;
        other.send(frame_packet(frame, token, client, server)); // Different active peer.
        sender.send(frame_packet(frame, "ffffffffffffffffffffffffffffffff", client, server));
        eventually([&] { return receiver.snapshot().rejected == 5; });
        expect(receiver.snapshot().accepted == 2 && receiver.snapshot().fresh, "invalid frame changed state");
        other.send(spriteface::seal_packet(hello(other_client), token));
        expect(nlohmann::json::parse(spriteface::open_packet(other.reply(), token))["type"] == "busy",
               "active peer was not protected");
        eventually([&] { return !receiver.snapshot().fresh; });
        other.send(replay); // An old session cannot take over after the timeout.
        eventually([&] { return receiver.snapshot().rejected == 6; });
        other.send(spriteface::seal_packet(hello(other_client), token));
        const auto next = nlohmann::json::parse(spriteface::open_packet(other.reply(), token));
        expect(next["type"] == "ready" && next["server"] != server, "server challenge was reused");
        const std::string next_server = next["server"];
        sender.send(replay); // Replay from the previous connection after handover.
        eventually([&] { return receiver.snapshot().rejected == 7; });
        other.send(frame_packet(frame, token, other_client, next_server));
        eventually([&] { return receiver.snapshot().accepted == 3; });
        expect(receiver.snapshot().frame.seq == 1 && receiver.snapshot().fresh, "stale peer handover failed");
        frame.seq = 2; frame.tracking = false;
        other.send(frame_packet(frame, token, other_client, next_server));
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
    try { parser_test(); filter_test(); encryption_test(); receiver_test(); std::cout << "Protocol, filtering, and loopback receiver tests passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
