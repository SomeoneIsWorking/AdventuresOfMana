cmake_minimum_required(VERSION 3.24)
project(mana LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLESV2 REQUIRED IMPORTED_TARGET glesv2)

include(FetchContent)
FetchContent_Declare(lucent
  GIT_REPOSITORY https://github.com/SomeoneIsWorking/lucent
  GIT_TAG main)
FetchContent_MakeAvailable(lucent)

add_library(mcf STATIC
  src/mcf/lha.cpp
  src/mcf/assets.cpp)
target_include_directories(mcf PUBLIC src)
target_link_libraries(mcf PUBLIC lucent::lucent)

add_executable(mana src/host/main.cpp)
target_link_libraries(mana PRIVATE mcf SDL3::SDL3 PkgConfig::GLESV2)
