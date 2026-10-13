#include <GL/glew.h>
#include "spriteface/sprite_model.hpp"
#include "ssplayer_animedecode.h"
#include "ssplayer_render_gl.h"
#include "ssplayer_shader_gl.h"
#include "OpenGL/SSTextureGL.h"
#include <nlohmann/json.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace spriteface {
namespace {
namespace ss = spritestudio6;
using json = nlohmann::json;
ss::SSTextureLoader::DataHandle load_image(const char* filename, int* w, int* h, int* bpp) {
    int channels = 0;
    auto* result = stbi_load(filename, w, h, &channels, 4);
    if (bpp) *bpp = 4;
    return result;
}
void free_image(ss::SSTextureLoader::DataHandle image) { stbi_image_free(image); }
const char* image_error() { return stbi_failure_reason(); }
float signal_value(const FaceFrame& face, const std::string& name) {
    if (name == "yaw") return face.yaw;
    if (name == "pitch") return face.pitch;
    if (name == "roll") return face.roll;
    if (name == "left_eye") return face.left_eye;
    if (name == "right_eye") return face.right_eye;
    if (name == "mouth_open") return face.mouth_open;
    if (name == "mouth_smile") return face.mouth_smile;
    return 0.0f;
}
float* property_value(ss::SsPartState& part, const std::string& name) {
    if (name == "rotation_z") return &part.rotation.z;
    if (name == "position_x") return &part.position.x;
    if (name == "position_y") return &part.position.y;
    if (name == "scale_x") return &part.scale.x;
    if (name == "scale_y") return &part.scale.y;
    if (name == "local_scale_x") return &part.localscale.x;
    if (name == "local_scale_y") return &part.localscale.y;
    if (name == "alpha") return &part.alpha;
    return nullptr;
}
void validate_binding(const FaceBinding& binding) {
    static const std::unordered_set<std::string> signals = {
        "yaw", "pitch", "roll", "left_eye", "right_eye", "mouth_open", "mouth_smile"};
    static const std::unordered_set<std::string> properties = {
        "rotation_z", "position_x", "position_y", "scale_x", "scale_y", "local_scale_x", "local_scale_y", "alpha"};
    if (binding.part.empty() || !signals.count(binding.signal) || !properties.count(binding.property) ||
        (binding.operation != "add" && binding.operation != "multiply" && binding.operation != "replace") ||
        !std::isfinite(binding.input_min) || !std::isfinite(binding.input_max) ||
        !std::isfinite(binding.output_min) || !std::isfinite(binding.output_max) ||
        binding.input_max <= binding.input_min)
        throw std::runtime_error("Invalid face binding: " + binding.part);
}
class FaceDecoder final : public ss::SsAnimeDecoder {
public:
    FaceFrame face;
    const std::vector<FaceBinding>* bindings = nullptr;
protected:
    void onPartStateEvaluated(ss::SsPart* part, ss::SsPartState* state) override {
        if (!bindings) return;
        for (const auto& binding : *bindings) {
            if (binding.part != part->name || binding.input_max <= binding.input_min) continue;
            auto* property = property_value(*state, binding.property);
            if (!property) continue;
            const float t = std::clamp((signal_value(face, binding.signal) - binding.input_min) /
                                       (binding.input_max - binding.input_min), 0.0f, 1.0f);
            const float value = binding.output_min + t * (binding.output_max - binding.output_min);
            if (!std::isfinite(value)) continue;
            if (binding.operation == "multiply") *property *= value;
            else if (binding.operation == "replace") *property = value;
            else *property += value;
        }
    }
};
void validate_project(ss::SsProject& project) {
    for (const auto& pack : project.getAnimePackList()) {
        if (!pack || pack->animeList.empty() || pack->Model.partList.empty())
            throw std::runtime_error("An animation pack has no model or animation.");
        for (std::size_t i = 0; i < pack->Model.partList.size(); ++i) {
            const auto* part = pack->Model.partList[i];
            if (!part || part->arrayIndex != static_cast<int>(i) || part->parentIndex < -1 ||
                part->parentIndex >= static_cast<int>(i))
                throw std::runtime_error("Invalid parent/part ordering in " + pack->name);
            if (part->type == ss::SsPartType::instance) {
                auto pack_name = part->refAnimePack;
                auto anime_name = part->refAnime;
                if (!project.findAnimation(pack_name, anime_name))
                    throw std::runtime_error("Missing instance animation for " + part->name);
            }
        }
        for (auto name : pack->cellmapNames)
            if (!project.findCellMap(name)) throw std::runtime_error("Missing cell map: " + name);
    }
    if (project.getAnimePackList().empty()) throw std::runtime_error("Project contains no animation packs.");
}
} // namespace

struct SpriteModel::Impl {
    // Singleton SDK factories are intentionally confined to one model per app.
    ss::SSTextureFactory textures{new ss::SSTextureGL()};
    ss::SsCurrentRenderer renderer{new ss::SsRenderGL()};
    std::unique_ptr<ss::SsProject> project;
    std::unique_ptr<FaceDecoder> decoder;
    std::vector<AnimationChoice> choices;
    std::vector<std::string> parts;
    std::vector<FaceBinding> bindings;
    int selected = 0;
    float play_frame = 0.0f;
    float canvas_width = 512.0f, canvas_height = 512.0f;
    float pivot_x = 0.0f, pivot_y = 0.0f;
    Impl() {
        ss::SSTextureLoader::FunctionLoadImageFromFile = load_image;
        ss::SSTextureLoader::FunctionDecodeEndImageFile = free_image;
        ss::SSTextureLoader::FunctionMessageGetFailureLoadFromFile = image_error;
    }
    ~Impl() {
        decoder.reset();
        project.reset();
        ss::SSTextureFactory::releaseAllTexture();
        ss::SsRenderGL::clearShaderCache();
        ss::SSOpenGLShaderMan::Destory();
    }
};
SpriteModel::SpriteModel() : impl_(std::make_unique<Impl>()) {}
SpriteModel::~SpriteModel() = default;
void SpriteModel::load(const std::string& path) {
    const auto absolute = std::filesystem::absolute(std::filesystem::u8path(path)).u8string();
    std::unique_ptr<ss::SsProject> project(ss::ssloader_sspj::Load(absolute));
    if (!project) throw std::runtime_error("Cannot load .sspj and its referenced .ssae/.ssce files: " + path);
    validate_project(*project);
    impl_->decoder.reset();
    impl_->project.reset();
    ss::SSTextureFactory::releaseAllTexture();
    impl_->project = std::move(project);
    impl_->choices.clear();
    int pack_index = 0;
    for (const auto& pack : impl_->project->getAnimePackList()) {
        for (std::size_t i = 0; i < pack->animeList.size(); ++i)
            if (!pack->animeList[i]->isSetup)
                impl_->choices.push_back({pack->name + " / " + pack->animeList[i]->name, pack_index, static_cast<int>(i)});
        ++pack_index;
    }
    if (impl_->choices.empty()) throw std::runtime_error("Project contains no playable animations.");
    select_animation(0);
}
void SpriteModel::select_animation(int choice) {
    if (!impl_->project || choice < 0 || choice >= static_cast<int>(impl_->choices.size()))
        throw std::runtime_error("Invalid animation selection.");
    const auto& selected = impl_->choices[choice];
    auto* pack = impl_->project->getAnimePackList()[selected.pack].get();
    auto* animation = pack->animeList[selected.animation];
    auto cells = std::make_unique<ss::SsCellMapList>();
    cells->set(impl_->project.get(), pack);
    auto decoder = std::make_unique<FaceDecoder>();
    decoder->bindings = &impl_->bindings;
    decoder->setAnimation(&pack->Model, animation, cells.release(), impl_->project.get());
    impl_->decoder = std::move(decoder);
    impl_->selected = choice;
    impl_->play_frame = static_cast<float>(start_frame());
    const auto& settings = animation->overrideSettings ? animation->settings : pack->settings;
    impl_->canvas_width = std::max(1.0f, settings.canvasSize.x);
    impl_->canvas_height = std::max(1.0f, settings.canvasSize.y);
    impl_->pivot_x = settings.pivot.x;
    impl_->pivot_y = settings.pivot.y;
    impl_->parts.clear();
    for (const auto* part : pack->Model.partList) impl_->parts.push_back(part->name);
    update(FaceFrame{}, 0.0f, false);
    for (const auto& state : impl_->decoder->getPartState())
        if (state.cellValue.cell && !state.cellValue.texture)
            throw std::runtime_error("A cell texture failed to load. Check imageBaseDirectory and image paths.");
}
void SpriteModel::load_bindings(const std::string& path) {
    std::ifstream input(std::filesystem::u8path(path));
    if (!input) throw std::runtime_error("Cannot open mapping: " + path);
    json root; input >> root;
    if (root.at("schema").get<int>() != 1 || !root.at("bindings").is_array())
        throw std::runtime_error("Mapping must use schema 1.");
    std::vector<FaceBinding> bindings;
    for (const auto& item : root.at("bindings")) {
        FaceBinding b;
        b.part = item.at("part").get<std::string>();
        b.signal = item.at("signal").get<std::string>();
        b.property = item.at("property").get<std::string>();
        b.operation = item.value("operation", "add");
        b.input_min = item.at("input_min").get<float>(); b.input_max = item.at("input_max").get<float>();
        b.output_min = item.at("output_min").get<float>(); b.output_max = item.at("output_max").get<float>();
        validate_binding(b);
        bindings.push_back(std::move(b));
    }
    impl_->bindings = std::move(bindings);
}
void SpriteModel::save_bindings(const std::string& path) const {
    json root = {{"schema", 1}, {"bindings", json::array()}};
    for (const auto& b : impl_->bindings) {
        validate_binding(b);
        root["bindings"].push_back({{"part", b.part}, {"signal", b.signal}, {"property", b.property},
            {"operation", b.operation}, {"input_min", b.input_min}, {"input_max", b.input_max},
            {"output_min", b.output_min}, {"output_max", b.output_max}});
    }
    std::ofstream output(std::filesystem::u8path(path));
    if (!output) throw std::runtime_error("Cannot save mapping: " + path);
    output << root.dump(2) << '\n';
    if (!output) throw std::runtime_error("Failed to write mapping: " + path);
}
void SpriteModel::update(const FaceFrame& face, float dt, bool playing) {
    if (!impl_->decoder) return;
    if (playing) {
        const float span = static_cast<float>(end_frame() - start_frame() + 1);
        const float elapsed = std::max(0.0f, dt) * impl_->decoder->getAnimeFPS();
        impl_->play_frame = start_frame() + std::fmod(impl_->play_frame - start_frame() + elapsed, std::max(1.0f, span));
    }
    impl_->decoder->face = face;
    impl_->decoder->setPlayFrame(impl_->play_frame);
    impl_->decoder->update(dt * impl_->decoder->getAnimeFPS());
}
void SpriteModel::render(int width, int height, float zoom) {
    if (!impl_->decoder || width <= 0 || height <= 0) return;
    const float scale = std::min(width / impl_->canvas_width, height / impl_->canvas_height) * zoom;
    const float half_w = width / (2.0f * scale), half_h = height / (2.0f * scale);
    const float center_x = -impl_->pivot_x * impl_->canvas_width;
    const float center_y = -impl_->pivot_y * impl_->canvas_height;
    glViewport(0, 0, width, height);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glMatrixMode(GL_TEXTURE); glLoadIdentity();
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(center_x - half_w, center_x + half_w, center_y - half_h, center_y + half_h, -10000, 10000);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    impl_->decoder->draw();
    glUseProgram(0);
}
bool SpriteModel::loaded() const { return impl_->decoder != nullptr; }
float SpriteModel::frame() const { return impl_->play_frame; }
void SpriteModel::seek(float frame) { impl_->play_frame = std::clamp(frame, static_cast<float>(start_frame()), static_cast<float>(end_frame())); }
int SpriteModel::start_frame() const { return impl_->decoder ? impl_->decoder->getAnimeStartFrame() : 0; }
int SpriteModel::end_frame() const { return impl_->decoder ? impl_->decoder->getAnimeEndFrame() : 0; }
int SpriteModel::selected_animation() const { return impl_->selected; }
const std::vector<AnimationChoice>& SpriteModel::animations() const { return impl_->choices; }
const std::vector<std::string>& SpriteModel::parts() const { return impl_->parts; }
std::vector<FaceBinding>& SpriteModel::bindings() { return impl_->bindings; }
std::vector<std::string> SpriteModel::unmatched_bindings() const {
    std::vector<std::string> result;
    for (const auto& binding : impl_->bindings)
        if (std::find(impl_->parts.begin(), impl_->parts.end(), binding.part) == impl_->parts.end())
            result.push_back(binding.part);
    return result;
}
} // namespace spriteface
