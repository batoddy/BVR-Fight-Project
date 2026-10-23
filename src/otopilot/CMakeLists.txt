cmake_minimum_required(VERSION 3.20)
project(otopilot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=gnu++17 değil, saf -std=c++17

# Tür verilmezse optimize derle (-O2 karşılığı).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Kaynak klasörlerindeki .cpp dosyaları; build ve third_party taranmaz.
# CONFIGURE_DEPENDS: bu klasörlere yeni dosya eklenince CMake listeyi günceller.
file(GLOB SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/control/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/model/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/calculations/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/io/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
)

add_executable(main ${SOURCES})
target_include_directories(main PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(main PRIVATE -Wall -Wextra)
target_link_libraries(main PRIVATE ws2_32)   # Windows TCP (Winsock)
target_link_options(main PRIVATE -static)    # MinGW çalışma zamanı exe'nin içine

# exe build klasöründe kalmasın; bugünkü gibi src/otopilot/main.exe olarak çıksın.
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Birim dönüşümleri ve IO sözleşmesi: cmake --build ... ardından ctest --test-dir ...
include(CTest)
if(BUILD_TESTING)
    add_executable(units_test
        tests/units_test.cpp
        model/aircraft.cpp
        calculations/geometry.cpp
        calculations/engagement.cpp
        calculations/missile_geometry.cpp
        io/state_reader.cpp
        io/command_writer.cpp
        io/calculation_writer.cpp
    )
    target_include_directories(units_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_options(units_test PRIVATE -Wall -Wextra)
    target_link_options(units_test PRIVATE -static)
    add_test(NAME units COMMAND units_test)
endif()
