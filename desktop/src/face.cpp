#include "spriteface/face.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace spriteface {
namespace {
using Json = nlohmann::json;
float number(const Json& value, const char* key, float lo, float hi) {
    const auto& item = value.at(key);
    if (!item.is_number()) throw std::invalid_argument(std::string(key) + " must be a number");
    const double result = item.get<double>();
    if (!std::isfinite(result) || result < lo || result > hi)
        throw std::invalid_argument(std::string(key) + " is out of range");
    return static_cast<float>(result);
}
void validate(const FaceFrame& f) {
    const float values[] = {f.confidence, f.left_eye, f.right_eye, f.mouth_open, f.mouth_smile};
    for (float v : values) if (!std::isfinite(v) || v < 0 || v > 1) throw std::invalid_argument("face weight is out of range");
    for (float v : {f.yaw, f.pitch, f.roll}) if (!std::isfinite(v) || v < -3.141593f || v > 3.141593f) throw std::invalid_argument("head angle is out of range");
}
}
FaceFrame parse_face_packet(std::string_view packet, std::string_view expected_token) {
    try {
        if (packet.empty() || packet.size() > 2048) throw std::invalid_argument("invalid packet size");
        const auto j = Json::parse(packet.begin(), packet.end());
        if (!j.is_object() || !j.at("v").is_number_integer() || j.at("v") != 1) throw std::invalid_argument("unsupported protocol version");
        if (!j.at("type").is_string() || j.at("type") != "frame") throw std::invalid_argument("not a frame");
        if (!j.at("token").is_string() || normalize_session_token(j.at("token").get<std::string>()) != normalize_session_token(expected_token)) throw std::invalid_argument("invalid session token");
        if (!j.at("seq").is_number_unsigned()) {
            if (!j.at("seq").is_number_integer() || j.at("seq").get<std::int64_t>() < 0) throw std::invalid_argument("invalid sequence");
        }
        const auto seq = j.at("seq").get<std::uint64_t>();
        if (seq > UINT32_MAX) throw std::invalid_argument("sequence overflow");
        if (!j.at("tracking").is_boolean()) throw std::invalid_argument("tracking must be boolean");
        FaceFrame f;
        f.seq = static_cast<std::uint32_t>(seq);
        f.tracking = j.at("tracking").get<bool>();
        f.confidence = number(j, "confidence", 0, 1);
        f.yaw = number(j.at("head"), "yaw", -3.141593f, 3.141593f);
        f.pitch = number(j.at("head"), "pitch", -3.141593f, 3.141593f);
        f.roll = number(j.at("head"), "roll", -3.141593f, 3.141593f);
        f.left_eye = number(j.at("eyes"), "left", 0, 1);
        f.right_eye = number(j.at("eyes"), "right", 0, 1);
        f.mouth_open = number(j.at("mouth"), "open", 0, 1);
        f.mouth_smile = number(j.at("mouth"), "smile", 0, 1);
        if (!f.tracking) { const auto saved = f.seq; f = FaceFrame{}; f.seq = saved; }
        return f;
    } catch (const Json::exception& error) {
        throw std::invalid_argument(std::string("invalid face packet: ") + error.what());
    }
}
std::string encode_face_packet(const FaceFrame& f, std::string_view token) {
    validate(f);
    return Json{{"v",1},{"type","frame"},{"token",normalize_session_token(token)},{"seq",f.seq},
        {"tracking",f.tracking},{"confidence",f.confidence},
        {"head",{{"yaw",f.yaw},{"pitch",f.pitch},{"roll",f.roll}}},
        {"eyes",{{"left",f.left_eye},{"right",f.right_eye}}},
        {"mouth",{{"open",f.mouth_open},{"smile",f.mouth_smile}}}}.dump();
}
std::string make_session_token() {
    std::random_device random;
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) result << std::setw(2) << (random() & 255u);
    return result.str();
}
std::string normalize_session_token(std::string_view token) {
    if (token.size() != 32 || token.find_first_not_of("0123456789abcdefABCDEF") != std::string_view::npos)
        throw std::invalid_argument("token must contain 32 hex digits");
    std::string result(token);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
std::string make_pairing_uri(std::string_view host, std::uint16_t port, std::string_view token) {
    if (host.empty() || host.find_first_not_of("0123456789.") != std::string_view::npos || port == 0)
        throw std::invalid_argument("pairing requires an IPv4 host and nonzero port");
    std::istringstream parts{std::string(host)};
    std::string part;
    int count = 0;
    int first = -1;
    while (std::getline(parts, part, '.')) {
        if (part.empty() || part.size() > 3 || (part.size() > 1 && part.front() == '0') || std::stoi(part) > 255) throw std::invalid_argument("invalid IPv4 host");
        if (count == 0) first = std::stoi(part);
        ++count;
    }
    if (count != 4 || host.back() == '.' || first == 0 || first >= 224) throw std::invalid_argument("invalid unicast pairing host");
    return "spriteface://pair?v=1&host=" + std::string(host) + "&port=" + std::to_string(port) + "&token=" + normalize_session_token(token);
}
FaceFrame FaceFilter::update(const FaceSnapshot& sample, float dt_seconds) {
    const FaceFrame target = sample.fresh && sample.frame.tracking ? sample.frame : FaceFrame{};
    const float dt = std::isfinite(dt_seconds) ? std::clamp(dt_seconds, 0.0f, 0.25f) : 0.0f;
    const float blend = -std::expm1(-dt / 0.06f);
    auto smooth = [blend](float& a, float b) { a += blend * (b - a); };
    smooth(state_.yaw, target.yaw); smooth(state_.pitch, target.pitch); smooth(state_.roll, target.roll);
    smooth(state_.left_eye, target.left_eye); smooth(state_.right_eye, target.right_eye);
    smooth(state_.mouth_open, target.mouth_open); smooth(state_.mouth_smile, target.mouth_smile);
    state_.confidence = target.confidence;
    state_.tracking = sample.fresh && sample.frame.tracking;
    state_.seq = sample.frame.seq;
    return state_;
}
void FaceFilter::reset() { state_ = FaceFrame{}; }
} // namespace spriteface
