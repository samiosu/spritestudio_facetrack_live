#pragma once
#include "spriteface/face.hpp"
#include <memory>
#include <string>
#include <vector>

namespace spriteface {
struct FaceBinding {
    std::string part;
    std::string signal = "roll";
    std::string property = "rotation_z";
    std::string operation = "add";
    float input_min = -0.6f, input_max = 0.6f;
    float output_min = -30.0f, output_max = 30.0f;
};
struct AnimationChoice { std::string label; int pack = 0, animation = 0; };

// Owns SDK resources; construct/destroy while its OpenGL context is current.
class SpriteModel {
public:
    SpriteModel();
    ~SpriteModel();
    SpriteModel(const SpriteModel&) = delete;
    SpriteModel& operator=(const SpriteModel&) = delete;
    void load(const std::string& project_path);
    void select_animation(int choice);
    void load_bindings(const std::string& path);
    void save_bindings(const std::string& path) const;
    void update(const FaceFrame& face, float dt_seconds, bool playing);
    void render(int width, int height, float zoom);
    bool loaded() const;
    float frame() const;
    void seek(float frame);
    int start_frame() const;
    int end_frame() const;
    int selected_animation() const;
    const std::vector<AnimationChoice>& animations() const;
    const std::vector<std::string>& parts() const;
    std::vector<FaceBinding>& bindings();
    std::vector<std::string> unmatched_bindings() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace spriteface
