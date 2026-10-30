cmake_minimum_required(VERSION 3.20)
project(otopilot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=gnu++17 değil, saf -std=c++17

# Tür verilmezse optimize derle (-O2 karşılığı).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# main.cpp dışındaki bütün kaynaklar tek kütüphanede; main ve testler buna bağlanır.
# CONFIGURE_DEPENDS: bu klasörlere yeni dosya eklenince CMake listeyi günceller.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/control/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/model/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/calculations/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/io/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
)

add_library(otopilot_core STATIC ${CORE_SOURCES})
target_include_directories(otopilot_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(otopilot_core PRIVATE -Wall -Wextra)
target_link_libraries(otopilot_core PUBLIC ws2_32)   # Windows TCP (Winsock)

add_executable(main main.cpp)
target_compile_options(main PRIVATE -Wall -Wextra)
target_link_libraries(main PRIVATE otopilot_core)
target_link_options(main PRIVATE -static)    # MinGW çalışma zamanı exe'nin içine

# exe build klasöründe kalmasın; bugünkü gibi src/otopilot/main.exe olarak çıksın.
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# Birim, hesap ve IO sözleşmesi testleri: cmake --build ... ardından ctest --test-dir ...
include(CTest)
if(BUILD_TESTING)
    add_executable(units_test tests/units_test.cpp)
    target_compile_options(units_test PRIVATE -Wall -Wextra)
    target_link_libraries(units_test PRIVATE otopilot_core)
    target_link_options(units_test PRIVATE -static)
    add_test(NAME units COMMAND units_test)
endif()
