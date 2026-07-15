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

add_subdirectory(milsko)

file(GLOB_RECURSE sources src/*.cpp)
add_executable(${PROJECT_NAME} ${sources})

target_link_libraries(${PROJECT_NAME}
    PkgConfig::LIBAV
    Mw
)

if(UNIX)
  find_package(DBus1 REQUIRED)
  target_include_directories(${PROJECT_NAME} PRIVATE ${DBus1_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${DBus1_LIBRARIES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/milsko/include/)
