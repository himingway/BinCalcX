cmake_minimum_required(VERSION 3.16)
project(BinCalc VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_executable(BinCalc WIN32
    src/main.cpp
    src/calculatormodel.cpp
    src/calculatorview.cpp
    src/calculatorcontroller.cpp
)

target_include_directories(BinCalc PRIVATE src)
target_link_libraries(BinCalc PRIVATE Qt6::Core Qt6::Widgets)

# A friendlier app id on macOS; harmless elsewhere.
set_target_properties(BinCalc PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# --- Headless model unit tests (opt-in) --------------------------------------
# Built via CMake so AUTOMOC + the Qt6::Core imported target handle moc and
# linking correctly. Building it by hand with `g++ ... -lQt6Core` trips a
# binutils copy-relocation error against QByteArray's protected `_empty`.
option(BINCALC_BUILD_TESTS "Build the headless model unit tests" OFF)
if(BINCALC_BUILD_TESTS)
    enable_testing()
    add_executable(test_model tests/test_model.cpp src/calculatormodel.cpp)
    target_include_directories(test_model PRIVATE src)
    target_link_libraries(test_model PRIVATE Qt6::Core)
    add_test(NAME model COMMAND test_model)
endif()
