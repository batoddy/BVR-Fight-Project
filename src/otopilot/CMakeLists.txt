cmake_minimum_required(VERSION 3.20)
project(otopilot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Wall -Wextra)
add_link_options(-static)            # MinGW çalışma zamanı exe'nin içine

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    control/*.cpp model/*.cpp calculations/*.cpp io/*.cpp common/*.cpp)

add_library(core STATIC ${SOURCES})
target_include_directories(core PUBLIC .)
target_link_libraries(core PUBLIC ws2_32)   # Windows TCP

add_executable(main main.cpp)
target_link_libraries(main core)
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})   # src/otopilot/main.exe

add_executable(units_test tests/units_test.cpp)
target_link_libraries(units_test core)

add_executable(maneuver_test tests/maneuver_test.cpp)
target_link_libraries(maneuver_test core)
