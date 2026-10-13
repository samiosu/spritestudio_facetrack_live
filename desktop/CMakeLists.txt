# Build SDK core only. Viewer/JUCE and the SDK's old image decoder are excluded.
set(SF_SDK_DIR "${CMAKE_CURRENT_BINARY_DIR}/sdk/Common")
file(COPY "${spritestudio_SOURCE_DIR}/Common" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/sdk")

# Upstream's part state evaluation precedes hierarchy matrices and mesh solving.
# A narrow virtual callback lets face values participate in the existing pipeline.
# Work on a build-directory copy; never modify downloaded dependencies in place.
set(SF_DECODER_H "${SF_SDK_DIR}/Animator/ssplayer_animedecode.h")
set(SF_DECODER_CPP "${SF_SDK_DIR}/Animator/ssplayer_animedecode.cpp")
file(READ "${SF_DECODER_H}" SF_HEADER)
set(SF_HOOK_ANCHOR "\t\tvirtual void\tupdate( float frameDelta = 1.0f );")
string(FIND "${SF_HEADER}" "${SF_HOOK_ANCHOR}" SF_HOOK_POS)
if(SF_HOOK_POS EQUAL -1)
    message(FATAL_ERROR "Pinned SpriteStudio SDK header changed; review the face-state hook patch.")
endif()
string(REPLACE "${SF_HOOK_ANCHOR}"
    "${SF_HOOK_ANCHOR}\n\t\t// SpriteFace extension: after attributes, before matrices and mesh solve.\n\t\tvirtual void onPartStateEvaluated(SsPart*, SsPartState*) {}"
    SF_HEADER "${SF_HEADER}")
file(WRITE "${SF_DECODER_H}" "${SF_HEADER}")
file(READ "${SF_DECODER_CPP}" SF_SOURCE)
set(SF_STATE_ANCHOR "\t\tupdateState( time , part , anime , &partStateRaw[cnt] );")
string(FIND "${SF_SOURCE}" "${SF_STATE_ANCHOR}" SF_STATE_POS)
if(SF_STATE_POS EQUAL -1)
    message(FATAL_ERROR "Pinned SpriteStudio SDK source changed; review the face-state hook patch.")
endif()
string(REPLACE "${SF_STATE_ANCHOR}" "${SF_STATE_ANCHOR}\n\t\tonPartStateEvaluated(part, &partStateRaw[cnt]);" SF_SOURCE "${SF_SOURCE}")
file(WRITE "${SF_DECODER_CPP}" "${SF_SOURCE}")
# Upstream's USE_GLEW branch omits extension declarations on Linux.
file(WRITE "${SF_SDK_DIR}/Drawer/ssOpenGLSetting.h" "#pragma once\n#include <GL/glew.h>\n")

# Upstream's getFullPath uses 2048-byte getwd/realpath buffers and temporarily
# changes the process working directory. FORTIFY_SOURCE correctly aborts on
# getwd's buffer size. Replace only that function, retaining the directory's
# trailing separator and the no-fullpath mode. All path strings remain UTF-8.
set(SF_PATH_CPP "${SF_SDK_DIR}/Loader/ssstring_uty.cpp")
file(READ "${SF_PATH_CPP}" SF_PATH_SOURCE)
string(REPLACE "#include <string>" "#include <filesystem>\n#include <string>" SF_PATH_SOURCE "${SF_PATH_SOURCE}")
set(SF_PATH_START_ANCHOR "std::string getFullPath( const std::string& basePath , const std::string &relPath )")
set(SF_PATH_END_ANCHOR "\nstd::string Replace(")
string(FIND "${SF_PATH_SOURCE}" "${SF_PATH_START_ANCHOR}" SF_PATH_START)
string(FIND "${SF_PATH_SOURCE}" "${SF_PATH_END_ANCHOR}" SF_PATH_END)
if(SF_PATH_START EQUAL -1 OR SF_PATH_END EQUAL -1 OR SF_PATH_END LESS SF_PATH_START)
    message(FATAL_ERROR "Pinned SpriteStudio SDK path helper changed; review the safe-path patch.")
endif()
string(SUBSTRING "${SF_PATH_SOURCE}" 0 ${SF_PATH_START} SF_PATH_PREFIX)
string(SUBSTRING "${SF_PATH_SOURCE}" ${SF_PATH_END} -1 SF_PATH_SUFFIX)
set(SF_SAFE_PATH [=[
// SpriteFace modification: no fixed-size buffers or process cwd changes.
std::string getFullPath(const std::string& basePath, const std::string& relPath)
{
#ifdef _NOTUSE_TEXTURE_FULLPATH
    return relPath;
#else
    namespace fs = std::filesystem;
    const fs::path base = basePath.empty() ? fs::current_path() : fs::u8path(basePath);
    fs::path target = fs::absolute(base / fs::u8path(relPath));
    std::error_code error;
    const fs::path canonical = fs::weakly_canonical(target, error);
    // Missing or inaccessible assets must reach the loader as ordinary paths;
    // they must not cause reads from uninitialized realpath output buffers.
    target = error ? target.lexically_normal() : canonical;
    std::string result = target.u8string();
    const char separator = static_cast<char>(fs::path::preferred_separator);
    if (result.empty() || result.back() != separator) result += separator;
    return result;
#endif
}
]=])
file(WRITE "${SF_PATH_CPP}" "${SF_PATH_PREFIX}${SF_SAFE_PATH}${SF_PATH_SUFFIX}")

set(SF_SDK_SOURCES
    Loader/tinyxml2/tinyxml2.cpp
    Loader/ssloader_sspj.cpp Loader/ssloader_ssae.cpp Loader/ssloader_ssce.cpp
    Loader/ssloader_ssee.cpp Loader/ssloader_ssqe.cpp
    Loader/SsEffectBehavior.cpp Loader/SsEffectElement.cpp
    Loader/ssstring_uty.cpp Loader/ssarchiver.cpp Loader/sstypes.cpp
    Loader/ssvalue.cpp Loader/ssInterpolation.cpp Loader/ssattribute.cpp
    Loader/sscharconverter.cpp Loader/ssFilesystem.cpp
    Animator/ssplayer_cellmap.cpp Animator/ssplayer_PartState.cpp
    Animator/ssplayer_render.cpp Animator/ssplayer_matrix.cpp
    Animator/ssplayer_effect.cpp Animator/ssplayer_effectfunction.cpp
    Animator/ssplayer_animedecode.cpp Animator/ssplayer_effect2.cpp Animator/ssplayer_mesh.cpp
    Helper/IsshTexture.cpp Helper/DebugPrint.cpp Helper/OpenGL/SSTextureGL.cpp
    Drawer/ssplayer_render_gl.cpp Drawer/ssplayer_shader_gl.cpp)
list(TRANSFORM SF_SDK_SOURCES PREPEND "${SF_SDK_DIR}/")
add_library(spriteface_sdk STATIC ${SF_SDK_SOURCES})
target_include_directories(spriteface_sdk PUBLIC
    "${SF_SDK_DIR}/Loader" "${SF_SDK_DIR}/Animator" "${SF_SDK_DIR}/Helper" "${SF_SDK_DIR}/Drawer")
target_compile_definitions(spriteface_sdk PUBLIC USE_GLEW=1 GLEW_STATIC PRIVATE _NOTUSE_STBI=1)
target_compile_features(spriteface_sdk PUBLIC cxx_std_17)
target_link_libraries(spriteface_sdk PUBLIC glew_s OpenGL::GL)
if(WIN32)
    target_compile_definitions(spriteface_sdk PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(SPRITEFACE_BUILD_TESTS)
    add_executable(spriteface-sdk-path-tests tests/sdk_path_tests.cpp)
    target_link_libraries(spriteface-sdk-path-tests PRIVATE spriteface_sdk)
    target_compile_features(spriteface-sdk-path-tests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(spriteface-sdk-path-tests PRIVATE /utf-8)
    endif()
    add_test(NAME spriteface-sdk-path COMMAND spriteface-sdk-path-tests)
endif()
if(MSVC)
    target_compile_options(spriteface_sdk PRIVATE /utf-8)
endif()

add_library(spriteface_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp" "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp")
target_include_directories(spriteface_imgui PUBLIC "${imgui_SOURCE_DIR}")
target_link_libraries(spriteface_imgui PUBLIC glfw OpenGL::GL)

add_library(spriteface_qr STATIC "${qrcodegen_SOURCE_DIR}/cpp/qrcodegen.cpp")
target_include_directories(spriteface_qr PUBLIC "${qrcodegen_SOURCE_DIR}/cpp")
target_compile_features(spriteface_qr PUBLIC cxx_std_17)

add_executable(spriteface src/gui_main.cpp src/sprite_model.cpp)
target_include_directories(spriteface PRIVATE include "${stb_SOURCE_DIR}")
target_link_libraries(spriteface PRIVATE spriteface_core spriteface_sdk spriteface_imgui spriteface_qr nlohmann_json::nlohmann_json)
target_compile_features(spriteface PRIVATE cxx_std_17)
target_compile_definitions(spriteface PRIVATE SPRITEFACE_ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")
if(MSVC)
    target_compile_options(spriteface PRIVATE /utf-8 /W4)
else()
    target_compile_options(spriteface PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_custom_command(TARGET spriteface POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:spriteface>/assets")
install(TARGETS spriteface RUNTIME DESTINATION .)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION .)
