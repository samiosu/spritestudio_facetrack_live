#include "spriteface/face.hpp"
#include <atomic>
#include <csignal>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace { volatile std::sig_atomic_t keep_running = 1; void interrupt(int) { keep_running = 0; } }
int main(int argc, char** argv) {
    try {
        std::string bind = "127.0.0.1", advertise = "127.0.0.1", token;
        std::uint16_t port = 11574;
        double duration = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "SpriteFace UDP receiver\n--bind IPv4 (default127.0.0.1) --advertise IPv4 --port 11574\n--token 32-hex (defaultnewrandomsession) --duration seconds (defaultuntilCtrl+C)\n";
                return 0;
            }
            if (i + 1 >= argc) throw std::invalid_argument("missing value for " + option);
            const std::string value = argv[++i];
            if (option == "--bind") bind = value;
            else if (option == "--advertise") advertise = value;
            else if (option == "--token") token = value;
            else if (option == "--port") {
                const auto n = std::stoul(value);
                if (n == 0 || n > 65535) throw std::invalid_argument("port must be 1..65535");
                port = static_cast<std::uint16_t>(n);
            } else if (option == "--duration") {
                duration = std::stod(value);
                if (duration <= 0 || !std::isfinite(duration)) throw std::invalid_argument("duration must be positive");
            } else throw std::invalid_argument("unknown option " + option);
        }
        if (token.empty()) token = spriteface::make_session_token();
        const auto uri = spriteface::make_pairing_uri(advertise, port, token);
        spriteface::FaceReceiver receiver;
        receiver.start(bind, port, token);
        std::signal(SIGINT, interrupt);
        std::signal(SIGTERM, interrupt);
        std::cout << "Listening on " << bind << ":" << port << "\nPairing: " << uri << "\n" << std::flush;
        const auto started = std::chrono::steady_clock::now();
        std::uint64_t last_accepted = 0;
        bool was_fresh = false;
        while (keep_running) {
            const auto sample = receiver.snapshot();
            if (sample.accepted != last_accepted || sample.fresh != was_fresh) {
                std::cout << "{\"fresh\":" << (sample.fresh ? "true" : "false")
                          << ",\"tracking\":" << (sample.frame.tracking && sample.fresh ? "true" : "false")
                          << ",\"seq\":" << sample.frame.seq << ",\"accepted\":" << sample.accepted
                          << ",\"rejected\":" << sample.rejected << ",\"mouth\":" << sample.frame.mouth_open << "}\n" << std::flush;
                last_accepted = sample.accepted;
                was_fresh = sample.fresh;
            }
            if (duration > 0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() >= duration) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        receiver.stop();
    } catch (const std::exception& error) { std::cerr << "SpriteFace: " << error.what() << '\n'; return 1; }
}
