cmake_minimum_required(VERSION 3.16)
project(BinCalc VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# qt_add_executable (not plain add_executable) so that under a STATIC Qt build
# the qwindows platform plugin is auto-linked in — otherwise a static .exe fails
# at startup with "could not find the Qt platform plugin windows". For normal
# shared builds it behaves identically to add_executable.
qt_add_executable(BinCalc WIN32
    src/main.cpp
    src/calculatormodel.cpp
    src/calculatorview.cpp
    src/calculatorcontroller.cpp
    assets/app.qrc
)

target_include_directories(BinCalc PRIVATE src)
target_link_libraries(BinCalc PRIVATE Qt6::Core Qt6::Widgets)

# Size-optimise the binary (especially on Windows).
if(MSVC)
    target_compile_options(BinCalc PRIVATE /O1 /GL)          # optimise for size + link-time code gen
    target_link_options(BinCalc PRIVATE /LTCG /OPT:REF /OPT:ICF)  # strip unreferenced, fold identical COMDATs
endif()

# Windows .exe icon (explorer / taskbar / start-menu). The .rc is generated from
# assets/app.rc.in with an absolute .ico path so rc.exe resolves it from any
# build directory. The runtime window icon (title bar / alt-tab) comes from the
# Qt resource set in main.cpp and works on every platform.
if(WIN32)
    set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.ico")
    configure_file(assets/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    target_sources(BinCalc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

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
