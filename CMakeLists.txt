cmake_minimum_required(VERSION 3.10)
project(empire LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(empire
    attack.cpp
    cpu.cpp
    economy.cpp
    empire.cpp
    grain.cpp
    investments.cpp
    population.cpp
    ui.cpp
)

# Find curses library (ncurses on Unix, PDCurses on Windows).
find_package(Curses REQUIRED)
target_include_directories(empire PRIVATE ${CURSES_INCLUDE_DIRS})
target_link_libraries(empire PRIVATE ${CURSES_LIBRARIES} m)

if(WIN32)
    target_compile_definitions(empire PRIVATE _WIN32)
endif()
