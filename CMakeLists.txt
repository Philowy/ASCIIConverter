cmake_minimum_required(VERSION 4.3)
project(ASCIIconverter)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM
)

FetchContent_MakeAvailable(SFML)

# Ścieżka do OpenCV w MSYS2 (żeby CMake na pewno ją znalazł)
# set(OpenCV_DIR "C:/msys64/mingw64/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.hpp")

add_executable(ASCIIconverter ${SOURCES} ${HEADERS})

target_include_directories(ASCIIconverter PRIVATE src)

target_link_libraries(ASCIIconverter PRIVATE
        ${OpenCV_LIBS}
        SFML::Audio
        SFML::System
)