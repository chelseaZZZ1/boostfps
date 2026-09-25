cmake_minimum_required(VERSION 3.18)
project(ChxlLauncher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# บังคับใช้ Static C Runtime (/MT)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(imgui CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(ChxlLauncher WIN32 main.cpp)

target_link_libraries(ChxlLauncher PRIVATE
    imgui::imgui
    CURL::libcurl
    nlohmann_json::nlohmann_json
    d3d11
    dwmapi
    ws2_32
    wldap32
    crypt32
    normaliz
)

if(MSVC)
    set_target_properties(ChxlLauncher PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
endif()
