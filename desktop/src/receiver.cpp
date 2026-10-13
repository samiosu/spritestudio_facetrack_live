#include "spriteface/face.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <array>
#include <mutex>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace spriteface {
namespace {
using Clock = std::chrono::steady_clock;
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket no_socket = INVALID_SOCKET;
void close_socket(Socket s) { closesocket(s); }
#else
using Socket = int;
constexpr Socket no_socket = -1;
void close_socket(Socket s) { close(s); }
#endif
std::string endpoint(const sockaddr_in& peer) {
    char host[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &peer.sin_addr, host, sizeof(host));
    return std::string(host) + ":" + std::to_string(ntohs(peer.sin_port));
}
}
struct FaceReceiver::Impl {
    Socket socket = no_socket;
    std::atomic<bool> running{false};
    std::thread worker;
    mutable std::mutex mutex;
    FaceSnapshot sample;
    Clock::time_point received{};
    bool have_frame = false;
    std::uint16_t bound_port = 0;
    std::string token;
#ifdef _WIN32
    bool winsock_started = false;
#endif

    void receive() {
        while (running.load()) {
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(socket, &readable);
            timeval timeout{0, 100000};
            const int selected = select(static_cast<int>(socket) + 1, &readable, nullptr, nullptr, &timeout);
            if (selected <= 0) continue;
            std::array<char, 2049> buffer{};
            sockaddr_in sender{};
#ifdef _WIN32
            int sender_length = sizeof(sender);
            const int size = recvfrom(socket, buffer.data(), static_cast<int>(buffer.size()), 0,
                                      reinterpret_cast<sockaddr*>(&sender), &sender_length);
#else
            socklen_t sender_length = sizeof(sender);
            const auto size = recvfrom(socket, buffer.data(), buffer.size(), 0,
                                      reinterpret_cast<sockaddr*>(&sender), &sender_length);
#endif
            if (size < 0) continue;
            try {
                if (size == 0 || size > 2048) throw std::invalid_argument("invalid packet size");
                const std::string_view bytes(buffer.data(), static_cast<std::size_t>(size));
                const auto j = nlohmann::json::parse(bytes.begin(), bytes.end());
                if (j.value("type", std::string{}) == "hello") {
                    if (!j.at("v").is_number_integer() || j.at("v") != 1 || !j.at("token").is_string() || normalize_session_token(j.at("token").get<std::string>()) != token)
                        throw std::invalid_argument("invalid hello");
                    const auto name = endpoint(sender);
                    bool busy;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        busy = have_frame && name != sample.peer && Clock::now() - received < std::chrono::milliseconds(800);
                    }
                    const std::string response = busy ? "{\"v\":1,\"type\":\"busy\"}" : "{\"v\":1,\"type\":\"ready\"}";
                    sendto(socket, response.data(), static_cast<int>(response.size()), 0,
                           reinterpret_cast<const sockaddr*>(&sender), sender_length);
                    continue;
                }
                const auto frame = parse_face_packet(bytes, token);
                const auto now = Clock::now();
                const auto name = endpoint(sender);
                std::lock_guard<std::mutex> lock(mutex);
                const bool expired = !have_frame || now - received >= std::chrono::milliseconds(800);
                if (!expired) {
                    if (name != sample.peer) { ++sample.rejected; continue; }
                    const std::uint32_t delta = frame.seq - sample.frame.seq;
                    if (delta == 0 || delta >= 0x80000000u) { ++sample.rejected; continue; }
                }
                sample.frame = frame;
                sample.peer = name;
                ++sample.accepted;
                received = now;
                have_frame = true;
            } catch (const std::exception&) {
                std::lock_guard<std::mutex> lock(mutex);
                ++sample.rejected;
            }
        }
    }
};
FaceReceiver::FaceReceiver() : impl_(std::make_unique<Impl>()) {}
FaceReceiver::~FaceReceiver() { stop(); }
void FaceReceiver::start(const std::string& bind_address, std::uint16_t port, const std::string& token) {
    stop();
    const auto normalized_token = normalize_session_token(token);
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
    impl_->winsock_started = true;
#endif
    impl_->socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl_->socket == no_socket) { stop(); throw std::runtime_error("cannot create UDP socket"); }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr) != 1) {
        stop(); throw std::invalid_argument("bind address must be an IPv4 literal");
    }
    if (bind(impl_->socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        stop(); throw std::runtime_error("cannot bind UDP socket (port in use or permission denied)");
    }
#ifdef _WIN32
    int length = sizeof(address);
#else
    socklen_t length = sizeof(address);
#endif
    if (getsockname(impl_->socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        stop(); throw std::runtime_error("cannot inspect UDP socket");
    }
    impl_->bound_port = ntohs(address.sin_port);
    impl_->token = normalized_token;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->sample = FaceSnapshot{};
        impl_->have_frame = false;
    }
    impl_->running.store(true);
    try { impl_->worker = std::thread([this] { impl_->receive(); }); }
    catch (...) { stop(); throw; }
}
void FaceReceiver::stop() {
    impl_->running.store(false);
    if (impl_->worker.joinable()) impl_->worker.join();
    if (impl_->socket != no_socket) { close_socket(impl_->socket); impl_->socket = no_socket; }
    impl_->bound_port = 0;
#ifdef _WIN32
    if (impl_->winsock_started) { WSACleanup(); impl_->winsock_started = false; }
#endif
}
FaceSnapshot FaceReceiver::snapshot(std::chrono::milliseconds timeout) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto result = impl_->sample;
    if (impl_->have_frame) {
        const auto age = Clock::now() - impl_->received;
        result.age_seconds = std::chrono::duration<double>(age).count();
        result.fresh = impl_->running.load() && age < timeout;
    }
    return result;
}
std::uint16_t FaceReceiver::port() const { return impl_->bound_port; }
} // namespace spriteface
