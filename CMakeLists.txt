cmake_minimum_required(VERSION 3.14)
project(AlphabetSoup LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(alphabet_soup
    src/main.cpp
    src/game.cpp
    src/typing.cpp
    src/render.cpp
)

target_link_libraries(alphabet_soup PRIVATE raylib)
target_include_directories(alphabet_soup PRIVATE src)
