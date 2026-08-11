cmake_minimum_required(VERSION 3.24)
project(mana LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)
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
  src/mcf/assets.cpp)
target_include_directories(mcf PUBLIC src)
target_link_libraries(mcf PUBLIC lucent::lucent)

add_library(engine STATIC src/engine/script.cpp src/engine/world.cpp src/engine/audio.cpp)
target_include_directories(engine PUBLIC src)
target_link_libraries(engine PUBLIC lua lucent::lucent SDL3::SDL3 PkgConfig::VORBISFILE)

add_executable(mana src/host/main.cpp src/host/render.cpp)
target_link_libraries(mana PRIVATE mcf engine SDL3::SDL3 PkgConfig::GLESV2)
