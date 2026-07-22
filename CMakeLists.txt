cmake_minimum_required(VERSION 3.17)
project(audioglobber)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavfilter
    libavformat
    libavcodec
    libswresample
    libswscale
    libavutil
)

set(MW_TRY_VULKAN OFF)
add_subdirectory(submodules/milsko)

file(GLOB_RECURSE sources src/*.cpp)
add_executable(${PROJECT_NAME} ${sources})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    PkgConfig::LIBAV
    Mw
    MNFM
)

add_subdirectory(submodules/MilskoNativeFileManager)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/submodules/milsko/include/)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/submodules/MilskoNativeFileManager/include/)
