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
