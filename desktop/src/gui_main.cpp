#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <qrcodegen.hpp>
#include "spriteface/face.hpp"
#include "spriteface/sprite_model.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifndef SPRITEFACE_ASSET_DIR
#define SPRITEFACE_ASSET_DIR "assets"
#endif

namespace {
using namespace spriteface;
struct Options {
    std::string project, mapping, host, screenshot, token;
    std::uint16_t port = 11574;
    bool phone = false, smoke = false, network_smoke = false;
};
Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        auto value = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("Missing value for " + flag);
            return argv[i];
        };
        if (flag == "--project") options.project = value();
        else if (flag == "--mapping") options.mapping = value();
        else if (flag == "--host") options.host = value();
        else if (flag == "--token") {
            options.token = value();
            if (options.token.size() != 32 || options.token.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
                throw std::runtime_error("Token must be 32 hexadecimal characters.");
            std::transform(options.token.begin(), options.token.end(), options.token.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }
        else if (flag == "--port") {
            const auto number = std::stoi(value());
            if (number < 1 || number > 65535) throw std::runtime_error("Port must be 1..65535.");
            options.port = static_cast<std::uint16_t>(number);
        } else if (flag == "--phone") options.phone = true;
        else if (flag == "--smoke-test") options.smoke = true;
        else if (flag == "--network-smoke-test") options.network_smoke = true;
        else if (flag == "--screenshot") options.screenshot = value();
        else if (flag == "--help") {
            std::cout << "SpriteFace [--project character.sspj] [--mapping mapping.json]\n"
                         "           [--phone] [--host PC_LAN_IP] [--port 11574] [--token 32_HEX]\n"
                         "           [--smoke-test | --network-smoke-test] [--screenshot capture.ppm]\n";
            std::exit(0);
        } else throw std::runtime_error("Unknown option: " + flag);
    }
    if (options.smoke && options.network_smoke)
        throw std::runtime_error("Choose either --smoke-test or --network-smoke-test.");
    return options;
}
template <std::size_t N> void copy_text(char (&buffer)[N], const std::string& value) {
    std::snprintf(buffer, N, "%s", value.c_str());
}
std::string default_host() {
#ifdef _WIN32
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        addrinfo hints{}, *addresses = nullptr; hints.ai_family = AF_INET;
        if (getaddrinfo(hostname, nullptr, &hints, &addresses) == 0) {
            std::string result;
            for (auto* it = addresses; it; it = it->ai_next) {
                char text[INET_ADDRSTRLEN]{};
                auto* addr = reinterpret_cast<sockaddr_in*>(it->ai_addr);
                inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text));
                if (std::strncmp(text, "127.", 4) != 0) { result = text; break; }
            }
            freeaddrinfo(addresses);
            if (!result.empty()) return result;
        }
    }
#else
    ifaddrs* addresses = nullptr;
    if (getifaddrs(&addresses) == 0) {
        std::string result;
        for (auto* it = addresses; it; it = it->ifa_next) {
            if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET) continue;
            char text[INET_ADDRSTRLEN]{};
            auto* addr = reinterpret_cast<sockaddr_in*>(it->ifa_addr);
            inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text));
            if (std::strncmp(text, "127.", 4) != 0) { result = text; break; }
        }
        freeifaddrs(addresses);
        if (!result.empty()) return result;
    }
#endif
    return "127.0.0.1";
}
std::filesystem::path asset_root(const char* executable) {
    std::error_code ec;
    const auto beside = std::filesystem::absolute(std::filesystem::u8path(executable), ec).parent_path() / "assets";
    if (!ec && std::filesystem::exists(beside / "demo" / "spriteface.sspj")) return beside;
    return std::filesystem::u8path(SPRITEFACE_ASSET_DIR);
}
void qr_code(const std::string& text) {
    if (text.empty()) return;
    const auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4;
    const float module = std::floor(std::min(245.0f, ImGui::GetContentRegionAvail().x) / (qr.getSize() + border * 2));
    if (module < 1.0f) return;
    const auto pos = ImGui::GetCursorScreenPos();
    const float side = module * (qr.getSize() + border * 2);
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), IM_COL32_WHITE);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y)) {
                const auto a = ImVec2(pos.x + (x + border) * module, pos.y + (y + border) * module);
                draw->AddRectFilled(a, ImVec2(a.x + module, a.y + module), IM_COL32_BLACK);
            }
    ImGui::Dummy(ImVec2(side, side));
}
void choice(const char* label, std::string& value, const std::vector<std::string>& options) {
    if (ImGui::BeginCombo(label, value.c_str())) {
        for (const auto& candidate : options)
            if (ImGui::Selectable(candidate.c_str(), candidate == value)) value = candidate;
        ImGui::EndCombo();
    }
}
void binding_editor(SpriteModel& model) {
    static const std::vector<std::string> signals = {"yaw", "pitch", "roll", "left_eye", "right_eye", "mouth_open", "mouth_smile"};
    static const std::vector<std::string> properties = {"rotation_z", "position_x", "position_y", "scale_x", "scale_y", "local_scale_x", "local_scale_y", "alpha"};
    static const std::vector<std::string> operations = {"add", "multiply", "replace"};
    auto& bindings = model.bindings();
    for (std::size_t i = 0; i < bindings.size();) {
        ImGui::PushID(static_cast<int>(i));
        auto& binding = bindings[i];
        const std::string label = binding.part + " <- " + binding.signal;
        bool remove = false;
        if (ImGui::TreeNode(label.c_str())) {
            choice("Part", binding.part, model.parts());
            choice("Signal", binding.signal, signals);
            choice("Property", binding.property, properties);
            choice("Operation", binding.operation, operations);
            ImGui::InputFloat("Input min", &binding.input_min);
            ImGui::InputFloat("Input max", &binding.input_max);
            ImGui::InputFloat("Output min", &binding.output_min);
            ImGui::InputFloat("Output max", &binding.output_max);
            if (binding.input_max <= binding.input_min) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "Input max must exceed min.");
            remove = ImGui::Button("Remove binding");
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (remove) bindings.erase(bindings.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
    if (ImGui::Button("Add binding") && !model.parts().empty()) {
        FaceBinding binding; binding.part = model.parts().front(); bindings.push_back(binding);
    }
    for (const auto& part : model.unmatched_bindings())
        ImGui::TextColored(ImVec4(1, 0.7f, 0.35f, 1), "Unmatched part: %s", part.c_str());
    ImGui::TextWrapped("Head angles use radians for input and degrees for rotation_z output. Values are clamped to the input range. Use multiply to resize eyes or mouth relative to their animated pose.");
}
struct Capture {
    int width = 0, height = 0;
    std::vector<unsigned char> pixels;
};
Capture capture_frame(int width, int height) {
    Capture result{width, height, std::vector<unsigned char>(static_cast<std::size_t>(width) * height * 3)};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, result.pixels.data());
    return result;
}
void write_capture(const Capture& capture, const std::string& path) {
    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open screenshot: " + path);
    file << "P6\n" << capture.width << ' ' << capture.height << "\n255\n";
    for (int y = capture.height - 1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(capture.pixels.data() + static_cast<std::size_t>(y) * capture.width * 3), capture.width * 3);
    if (!file) throw std::runtime_error("Cannot write screenshot: " + path);
}
void apply_theme() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 8; style.FrameRounding = 4;
    style.WindowPadding = ImVec2(18, 16); style.ItemSpacing = ImVec2(10, 9);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.09f, 0.11f, 1);
    style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.25f, 0.27f, 1);
    style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.34f, 0.35f, 1);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.14f, 0.46f, 0.46f, 1);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.32f, 0.79f, 0.68f, 1);
}
int run(const Options& options, const char* executable) {
    glfwSetErrorCallback([](int error, const char* message) { std::cerr << "GLFW " << error << ": " << message << '\n'; });
    if (!glfwInit()) throw std::runtime_error("GLFW initialization failed. A desktop display is required.");
    // SDK's GLSL 1.20 renderer uses compatibility OpenGL, including on macOS.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    GLFWwindow* controls = glfwCreateWindow(570, 860, "SpriteFace - Controls", nullptr, nullptr);
    if (!controls) { glfwTerminate(); throw std::runtime_error("Could not create control window."); }
    GLFWwindow* preview = glfwCreateWindow(640, 640, "SpriteFace - OBS Capture", nullptr, controls);
    if (!preview) { glfwDestroyWindow(controls); glfwTerminate(); throw std::runtime_error("Could not create preview window."); }
    glfwSetWindowPos(controls, 30, 40); glfwSetWindowPos(preview, 620, 60);
    glfwMakeContextCurrent(controls);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) throw std::runtime_error("GLEW initialization failed.");
    while (glGetError() != GL_NO_ERROR) {}
    glfwSwapInterval(1);
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); apply_theme();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(controls, true); ImGui_ImplOpenGL2_Init();
    glfwMakeContextCurrent(preview); glfwSwapInterval(0);
    int status = 0;
    {
        SpriteModel model;
        FaceReceiver receiver;
        FaceFilter filter;
        const auto assets = asset_root(executable);
        char project_path[2048]{}, mapping_path[2048]{}, host[256]{};
        copy_text(project_path, options.project.empty() ? (assets / "demo" / "spriteface.sspj").u8string() : options.project);
        copy_text(mapping_path, options.mapping.empty() ? (assets / "demo" / "mapping.json").u8string() : options.mapping);
        std::string message, dropped_project;
        std::string token = options.token.empty() ? make_session_token() : options.token;
        int port = options.port;
        bool phone = options.phone, listening = false, playing = true, manual = false;
        float zoom = 1.0f, background[3] = {0.0f, 1.0f, 0.0f};
        FaceFrame manual_face;
        auto attempt = [&](auto action) {
            try { action(); message.clear(); return true; }
            catch (const std::exception& error) { message = error.what(); return false; }
        };
        auto start_receiver = [&]() {
            receiver.stop(); listening = false;
            if (port < 1 || port > 65535) throw std::runtime_error("Port must be 1..65535.");
            receiver.start(phone ? "0.0.0.0" : "127.0.0.1", static_cast<std::uint16_t>(port), token);
            filter.reset(); listening = true;
        };
        attempt([&]() { model.load(project_path); model.load_bindings(mapping_path); });
        if (!model.loaded() && (options.smoke || options.network_smoke)) throw std::runtime_error(message);
        attempt(start_receiver);
        if (!listening && options.network_smoke) throw std::runtime_error(message);
        copy_text(host, options.host.empty() ? default_host() : options.host);
        glfwSetWindowUserPointer(controls, &dropped_project); glfwSetWindowUserPointer(preview, &dropped_project);
        const auto on_drop = [](GLFWwindow* window, int count, const char** paths) {
            if (count > 0) *static_cast<std::string*>(glfwGetWindowUserPointer(window)) = paths[0];
        };
        glfwSetDropCallback(controls, on_drop); glfwSetDropCallback(preview, on_drop);
        auto last = std::chrono::steady_clock::now();
        const auto network_test_started = last;
        int iterations = 0;
        Capture neutral, current;
        while (!glfwWindowShouldClose(controls) && !glfwWindowShouldClose(preview)) {
            glfwPollEvents();
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::min(0.1f, std::chrono::duration<float>(now - last).count()); last = now;
            glfwMakeContextCurrent(preview);
            if (!dropped_project.empty()) {
                copy_text(project_path, dropped_project); dropped_project.clear();
                attempt([&]() { model.load(project_path); });
            }
            const auto snapshot = receiver.snapshot();
            FaceFrame face = filter.update(snapshot, dt);
            if (manual && !options.network_smoke) face = manual_face;
            if (options.smoke) {
                face = FaceFrame{};
                if (iterations >= 3) { face.roll = 0.40f; face.left_eye = 0.05f; face.mouth_open = 0.9f; }
            }
            // The baseline must be neutral even if a sender starts before the GUI.
            if (options.network_smoke && iterations == 0) face = FaceFrame{};
            model.update(face, dt, playing && !options.smoke && !options.network_smoke);
            int width = 0, height = 0; glfwGetFramebufferSize(preview, &width, &height);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDisable(GL_SCISSOR_TEST);
            glClearColor(background[0], background[1], background[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            model.render(width, height, zoom);
            if (options.smoke || options.network_smoke || (!options.screenshot.empty() && iterations == 0)) {
                current = capture_frame(width, height);
                if (iterations == 0) neutral = current;
                if (!options.network_smoke && !options.screenshot.empty() && iterations == (options.smoke ? 5 : 0)) write_capture(current, options.screenshot);
            }
            if (options.smoke || options.network_smoke) {
                const auto error = glGetError();
                if (error != GL_NO_ERROR) throw std::runtime_error("OpenGL error during smoke test: " + std::to_string(error));
            }
            glfwSwapBuffers(preview);
            glfwMakeContextCurrent(controls);
            ImGui_ImplOpenGL2_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
            const auto display = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(display);
            ImGui::Begin("SpriteFace", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            ImGui::TextColored(ImVec4(0.45f, 0.89f, 0.76f, 1), "SPRITEFACE");
            ImGui::TextWrapped("Native SpriteStudio face tracking for your stream.");
            ImGui::Separator();
            if (!message.empty()) {
                ImGui::TextColored(ImVec4(1, 0.6f, 0.4f, 1), "Action failed"); ImGui::TextWrapped("%s", message.c_str());
            }
            if (ImGui::CollapsingHeader("Character", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::InputText("Project .sspj", project_path, sizeof(project_path));
                if (ImGui::Button("Load project")) {
                    glfwMakeContextCurrent(preview); attempt([&]() { model.load(project_path); }); glfwMakeContextCurrent(controls);
                }
                ImGui::SameLine(); ImGui::TextDisabled("or drop a .sspj onto either window");
                if (model.loaded()) {
                    const auto& choices = model.animations();
                    if (ImGui::BeginCombo("Animation", choices[model.selected_animation()].label.c_str())) {
                        for (std::size_t i = 0; i < choices.size(); ++i)
                            if (ImGui::Selectable(choices[i].label.c_str(), static_cast<int>(i) == model.selected_animation())) {
                                glfwMakeContextCurrent(preview); attempt([&]() { model.select_animation(static_cast<int>(i)); }); glfwMakeContextCurrent(controls);
                            }
                        ImGui::EndCombo();
                    }
                    ImGui::Checkbox("Play animation", &playing);
                    float frame = model.frame();
                    if (ImGui::SliderFloat("Frame", &frame, static_cast<float>(model.start_frame()), static_cast<float>(model.end_frame()), "%.1f")) {
                        model.seek(frame); playing = false;
                    }
                }
            }
            if (ImGui::CollapsingHeader("Tracking & phone pairing", ImGuiTreeNodeFlags_DefaultOpen)) {
                const bool phone_changed = ImGui::Checkbox("Allow phone on same LAN", &phone);
                ImGui::InputInt("UDP port", &port);
                if (phone_changed) attempt(start_receiver);
                if (ImGui::Button(listening ? "Restart listener" : "Start listener")) attempt(start_receiver);
                ImGui::SameLine(); if (ImGui::Button("Stop")) { receiver.stop(); listening = false; }
                ImGui::Text("Receiver: %s / %s", listening ? "running" : "stopped", phone ? "LAN" : "loopback");
                if (snapshot.accepted) {
                    ImGui::Text("Tracking: %s | age %.0f ms | confidence %.2f", snapshot.fresh && snapshot.frame.tracking ? "active" : "lost / neutral", snapshot.age_seconds * 1000, snapshot.frame.confidence);
                    ImGui::Text("Accepted %llu | rejected %llu", static_cast<unsigned long long>(snapshot.accepted), static_cast<unsigned long long>(snapshot.rejected));
                    ImGui::Text("Sender: %s", snapshot.peer.c_str());
                } else ImGui::TextDisabled("Waiting for face data...");
                ImGui::InputText("PC LAN IPv4", host, sizeof(host));
                if (ImGui::Button("New pairing token")) { token = make_session_token(); attempt(start_receiver); }
                ImGui::SameLine(); if (ImGui::Button("Copy token")) ImGui::SetClipboardText(token.c_str());
                ImGui::TextWrapped("Token: %s", token.c_str());
                if (phone && listening) {
                    try {
                        const auto uri = make_pairing_uri(host, receiver.port(), token);
                        if (ImGui::Button("Copy pairing URI")) ImGui::SetClipboardText(uri.c_str());
                        qr_code(uri);
                    } catch (const std::exception& error) { ImGui::TextWrapped("%s", error.what()); }
                    ImGui::TextWrapped("Scan this QR in the SpriteFace phone app. Choose your PC's LAN address and allow UDP through its firewall.");
                }
                if (ImGui::TreeNode("Manual test input")) {
                    ImGui::Checkbox("Use manual values", &manual);
                    ImGui::SliderFloat("Yaw", &manual_face.yaw, -0.7f, 0.7f);
                    ImGui::SliderFloat("Pitch", &manual_face.pitch, -0.7f, 0.7f);
                    ImGui::SliderFloat("Roll", &manual_face.roll, -0.7f, 0.7f);
                    ImGui::SliderFloat("Left eye", &manual_face.left_eye, 0, 1);
                    ImGui::SliderFloat("Right eye", &manual_face.right_eye, 0, 1);
                    ImGui::SliderFloat("Mouth open", &manual_face.mouth_open, 0, 1);
                    ImGui::SliderFloat("Smile", &manual_face.mouth_smile, 0, 1);
                    ImGui::TreePop();
                }
            }
            if (ImGui::CollapsingHeader("Face mapping")) {
                ImGui::InputText("Mapping JSON", mapping_path, sizeof(mapping_path));
                if (ImGui::Button("Load mapping")) attempt([&]() { model.load_bindings(mapping_path); });
                ImGui::SameLine(); if (ImGui::Button("Save mapping")) attempt([&]() { model.save_bindings(mapping_path); });
                binding_editor(model);
            }
            if (ImGui::CollapsingHeader("OBS output", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Capture the separate \"SpriteFace - OBS Capture\" window in OBS. For a transparent scene, add a Chroma Key filter for the green background.");
                ImGui::SliderFloat("Zoom", &zoom, 0.1f, 3.0f);
                ImGui::ColorEdit3("Background", background);
                ImGui::TextDisabled("%.1f FPS | %.2f ms/frame", ImGui::GetIO().Framerate, 1000.0f / std::max(1.0f, ImGui::GetIO().Framerate));
            }
            ImGui::End(); ImGui::Render();
            glfwGetFramebufferSize(controls, &width, &height); glViewport(0, 0, width, height);
            glClearColor(0.06f, 0.09f, 0.11f, 1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(controls);
            if (options.network_smoke && iterations > 0) {
                if (snapshot.accepted >= 8 && snapshot.fresh && snapshot.frame.tracking && current.pixels != neutral.pixels) {
                    if (!options.screenshot.empty()) write_capture(current, options.screenshot);
                    std::cout << "UDP receiver / face filter / SpriteStudio rendering test passed ("
                              << snapshot.accepted << " accepted packets, " << snapshot.rejected << " rejected).\n";
                    break;
                }
                if (std::chrono::duration<float>(now - network_test_started).count() >= 5.0f)
                    throw std::runtime_error("Network smoke test timed out without tracked UDP data changing the rendered pose (accepted " +
                        std::to_string(snapshot.accepted) + ", rejected " + std::to_string(snapshot.rejected) + ").");
            }
            if (options.smoke && ++iterations >= 6) {
                if (current.pixels.empty()) throw std::runtime_error("Smoke test produced no image.");
                const bool drawn = [&]() {
                    for (std::size_t i = 0; i < current.pixels.size(); i += 3)
                        if (current.pixels[i] > 5 || current.pixels[i + 1] < 245 || current.pixels[i + 2] > 5) return true;
                    return false;
                }();
                if (!drawn) throw std::runtime_error("Smoke test: model did not draw any pixels.");
                if (options.project.empty() && current.pixels == neutral.pixels)
                    throw std::runtime_error("Smoke test: face bindings did not change the demo render.");
                std::cout << "SpriteStudio load/render/face-override smoke test passed.\n";
                break;
            }
            if (!options.smoke) ++iterations;
        }
        receiver.stop(); glfwMakeContextCurrent(preview);
    }
    glfwMakeContextCurrent(controls);
    ImGui_ImplOpenGL2_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    glfwDestroyWindow(preview); glfwDestroyWindow(controls); glfwTerminate();
    return status;
}
} // namespace
int main(int argc, char** argv) {
    try { return run(parse_options(argc, argv), argv[0]); }
    catch (const std::exception& error) { std::cerr << "SpriteFace: " << error.what() << '\n'; return 1; }
}
