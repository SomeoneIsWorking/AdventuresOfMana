cmake_minimum_required(VERSION 3.24)
project(mana LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)
pkg_check_modules(VORBISFILE REQUIRED IMPORTED_TARGET vorbisfile)

include(FetchContent)
# Lua 5.3: the engine imports lua_rotate and luaopen_utf8, which are 5.3+, and
# the shipping scripts are written against that version.
FetchContent_Declare(lua53
  URL https://www.lua.org/ftp/lua-5.3.6.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lua53)
file(GLOB LUA_SRC ${lua53_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM LUA_SRC ${lua53_SOURCE_DIR}/src/lua.c ${lua53_SOURCE_DIR}/src/luac.c)
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC ${lua53_SOURCE_DIR}/src)
target_compile_definitions(lua PRIVATE LUA_USE_LINUX)

FetchContent_Declare(lucent
  GIT_REPOSITORY https://github.com/SomeoneIsWorking/lucent
  GIT_TAG main)
FetchContent_MakeAvailable(lucent)

add_library(mcf STATIC
  src/mcf/lha.cpp
  src/mcf/assets.cpp
  src/mcf/png.cpp)
target_include_directories(mcf PUBLIC src)
target_link_libraries(mcf PUBLIC lucent::lucent)

add_library(engine STATIC
  src/engine/audio.cpp
  src/engine/event_box.cpp
  src/engine/mode.cpp
  src/engine/script.cpp
  src/engine/world.cpp)
target_include_directories(engine PUBLIC src)
target_link_libraries(engine PUBLIC lua lucent::lucent SDL3::SDL3 PkgConfig::VORBISFILE)

add_library(render_assets STATIC
  src/host/render_asset.cpp
  src/host/render_camera.cpp
  src/host/render_pose.cpp
  src/host/render_snapshot.cpp)
target_include_directories(render_assets PUBLIC src)
target_link_libraries(render_assets PUBLIC mcf lucent::lucent)

add_library(host_image STATIC
  src/host/image_compare.cpp
  src/host/image_write.cpp)
target_include_directories(host_image PUBLIC src)

set(MANA_SHADER_ARTIFACTS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.vert.spv
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.vert.dxil
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.vert.msl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.frag.spv
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.frag.dxil
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/solid.frag.msl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.vert.spv
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.vert.dxil
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.vert.msl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.frag.spv
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.frag.dxil
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/textured.frag.msl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/skinned.vert.spv
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/skinned.vert.dxil
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/skinned.vert.msl)
set(MANA_SHADER_PACK ${CMAKE_CURRENT_BINARY_DIR}/generated/shader_pack.inc)
add_custom_command(
  OUTPUT ${MANA_SHADER_PACK}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_shader_pack.py
          --input-dir ${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated
          --output ${MANA_SHADER_PACK}
  DEPENDS tools/embed_shader_pack.py ${MANA_SHADER_ARTIFACTS}
  VERBATIM)

add_library(host_gpu STATIC
  src/host/gpu_asset.cpp
  src/host/gpu_asset_pipeline.cpp
  src/host/gpu_device.cpp
  src/host/gpu_pipeline.cpp
  src/host/gpu_scene.cpp
  src/host/gpu_snapshot_renderer.cpp
  src/host/gpu_shader.cpp
  ${MANA_SHADER_PACK})
target_include_directories(host_gpu PUBLIC src)
target_include_directories(host_gpu PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(host_gpu PUBLIC render_assets SDL3::SDL3 lucent::lucent)

add_executable(mana_gpu_selftest
  src/tools/gpu_selftest.cpp)
target_link_libraries(mana_gpu_selftest PRIVATE host_gpu)

add_executable(mana_gpu_asset_selftest
  src/tools/gpu_asset_selftest.cpp
  src/tools/gpu_asset_selftest_main.cpp)
target_link_libraries(mana_gpu_asset_selftest PRIVATE host_gpu host_image)

add_executable(mana
  src/host/interaction.cpp
  src/host/main.cpp
  src/host/navigation.cpp
  src/host/render.cpp
  src/host/scene_pair_capture.cpp
  src/host/story_driver.cpp)
target_link_libraries(mana PRIVATE mcf engine render_assets host_gpu host_image SDL3::SDL3 PkgConfig::GLESV2)
