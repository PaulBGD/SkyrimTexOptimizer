cmake_minimum_required(VERSION 3.16)
project(STO)

cmake_policy(SET CMP0042 NEW)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(libs/DirectXTex)
add_subdirectory(libs/NIF)
add_subdirectory(libs/libbsarch)
#add_subdirectory(libs/zlib)

include_directories(libs/libbsarch/src)
#include_directories(libs/bsatk)
#include_directories(${CMAKE_BINARY_DIR}/libs/zlib)

include_directories(libs/libnop/include)

add_executable(STO main.cpp textures.cpp textures.hpp sha2_512_256.h processor.cpp processor.h resizer.cpp resizer.h main.h MeshBSA.cpp MeshBSA.h)
target_link_libraries(STO PRIVATE directxtex nif libbsarch)
