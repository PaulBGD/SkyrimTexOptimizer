cmake_minimum_required(VERSION 3.16)
project(STO)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory("libs/DirectXTex")
add_subdirectory(libs/NIF)

add_executable(STO main.cpp textures.cpp textures.hpp sha2_512_256.h processor.cpp processor.h)
target_link_libraries(STO PRIVATE directxtex nif)
