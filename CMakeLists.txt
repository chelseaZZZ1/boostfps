cmake_minimum_required(VERSION 3.18)
project(ChxlLauncher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(imgui CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(ChxlLauncher main.cpp)

target_link_libraries(ChxlLauncher PRIVATE
    imgui::imgui
    CURL::libcurl
    nlohmann_json::nlohmann_json
    d3d11
    dwmapi
)
