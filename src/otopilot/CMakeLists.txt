cmake_minimum_required(VERSION 3.20)
project(otopilot CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=gnu++17 değil, saf -std=c++17

# Tür verilmezse optimize derle (-O2 karşılığı).
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Klasördeki bütün .cpp dosyaları. CONFIGURE_DEPENDS: yeni dosya eklenince
# make kendisi fark eder, cmake'i yeniden çalıştırmak gerekmez.
file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(main ${SOURCES})
target_compile_options(main PRIVATE -Wall -Wextra)
target_link_libraries(main PRIVATE ws2_32)   # Windows TCP (Winsock)
target_link_options(main PRIVATE -static)    # MinGW çalışma zamanı exe'nin içine

# exe build klasöründe kalmasın; bugünkü gibi src/otopilot/main.exe olarak çıksın.
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
