#include "ssstring_uty.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
fs::path resolve(const fs::path& base, const fs::path& relative) {
    const std::string result = spritestudio6::getFullPath(base.u8string(), relative.u8string());
    require(!result.empty() && result.back() == fs::path::preferred_separator,
            "SDK directory path must end with the native separator");
    return fs::u8path(result).lexically_normal();
}
void same_path(const fs::path& actual, const fs::path& expected, const char* description) {
    // Ignore the directory helper's final slash when comparing path components.
    const auto a = actual / ".";
    const auto e = expected / ".";
    if (a.lexically_normal() != e.lexically_normal())
        throw std::runtime_error(std::string(description) + " (actual: " + actual.u8string() +
                                 ", expected: " + expected.u8string() + ")");
}
} // namespace
int main() {
    const auto initial_cwd = fs::current_path();
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp = fs::temp_directory_path() / ("spriteface-sdk-path-" + std::to_string(unique));
    try {
        fs::create_directories(temp / "existing/sub");
        const auto root = fs::canonical(temp);
        same_path(resolve({}, {}), initial_cwd, "Empty paths must resolve to cwd");
        same_path(resolve({}, "assets/demo"), initial_cwd / "assets/demo", "Empty base must support relative paths");
        same_path(resolve("assets", "demo/../models"), initial_cwd / "assets/models", "Relative base must resolve against cwd");
        same_path(resolve(root, "existing/../missing/nested"), root / "missing/nested", "Missing directories must yield a usable path");
        same_path(resolve(root, fs::absolute(initial_cwd)), initial_cwd, "Absolute relative argument must override base");
        same_path(resolve(root, {}), root, "Empty relative path must preserve an absolute base");
        // Loader callers concatenate filenames; this must not turn dir + file
        // into dirfile. Exercise the exact SDK calling convention.
        const auto directory = spritestudio6::getFullPath(root.u8string(), "existing");
        same_path(fs::u8path(directory + "atlas.png"), root / "existing/atlas.png", "SDK filename concatenation requires final separator");
        const auto unicode = fs::u8path(u8"顔とキャラクター/表情");
        same_path(resolve(root, unicode), root / unicode, "UTF-8 names must survive native path conversion");
        std::string long_relative;
        for (int i = 0; i < 220; ++i) long_relative += "component_" + std::to_string(i) + "/";
        const auto long_result = resolve(root, fs::u8path(long_relative));
        require(long_result.u8string().size() > 2048, "Regression fixture must exceed the old SDK buffer");
        same_path(long_result, root / fs::u8path(long_relative), "Long paths must not truncate or overflow");
        // Force weakly_canonical's error branch on POSIX NAME_MAX and typical
        // Windows component limits; the fallback must retain the full path.
        const fs::path invalid_component(std::string(300, 'x'));
        same_path(resolve(root, invalid_component / "asset"), root / invalid_component / "asset", "Canonicalization errors must safely preserve a lexical path");
        std::error_code symlink_error;
        fs::create_directory_symlink(root / "existing/sub", root / "alias", symlink_error);
        if (!symlink_error) {
            same_path(resolve(root, "alias/child"), root / "existing/sub/child", "Existing symlink prefixes must resolve before missing descendants");
#ifdef _WIN32
            // Windows absolute paths (_fullpath in the original SDK) normalize
            // dot segments before filesystem symlink resolution.
            same_path(resolve(root, "alias/../asset"), root / "asset", "Windows must preserve native absolute-path dot normalization");
#else
            same_path(resolve(root, "alias/../asset"), root / "existing/asset", "Existing symlinks must keep realpath resolution semantics");
#endif
        }
        require(fs::current_path() == initial_cwd, "SDK path resolution must not change process cwd");
        fs::remove_all(temp);
        std::cout << "SDK safe path regression tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored; fs::remove_all(temp, ignored);
        std::cerr << "SDK path test: " << error.what() << '\n';
        return 1;
    }
}
