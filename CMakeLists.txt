cmake_minimum_required(VERSION 3.16)
project(BinCalc VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

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

# --- Headless unit tests (opt-in) --------------------------------------------
# Built via CMake so AUTOMOC + the Qt6 imported targets handle moc and linking
# correctly. Building these by hand with `g++ ... -lQt6Core` trips a binutils
# copy-relocation error against QByteArray's protected `_empty`.
option(BINCALC_BUILD_TESTS "Build the headless unit tests" OFF)
if(BINCALC_BUILD_TESTS)
    enable_testing()

    # test_model: pure Model coverage (no GUI), Core only.
    add_executable(test_model tests/test_model.cpp src/calculatormodel.cpp)
    target_include_directories(test_model PRIVATE src)
    target_link_libraries(test_model PRIVATE Qt6::Core)
    add_test(NAME model COMMAND test_model)

    # test_clipboard: View-layer coverage of Ctrl+C / Ctrl+V wiring, driven
    # headlessly under QT_QPA_PLATFORM=offscreen. Needs Gui (clipboard) +
    # Widgets (CalculatorView) + Test (QTest/qExec). calculatorview.cpp is
    # decoupled from the Model, so it compiles standalone.
    find_package(Qt6 COMPONENTS Test QUIET)
    if(Qt6Test_FOUND)
        add_executable(test_clipboard tests/test_clipboard.cpp src/calculatorview.cpp)
        target_include_directories(test_clipboard PRIVATE src)
        target_link_libraries(test_clipboard PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
        add_test(NAME clipboard COMMAND test_clipboard)

        # test_keys: View keyboard dispatch (digit/op/command signals, out-of-base
        # interception, bit-grid focus navigation) — headless, offscreen.
        add_executable(test_keys tests/test_keys.cpp src/calculatorview.cpp)
        target_include_directories(test_keys PRIVATE src)
        target_link_libraries(test_keys PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
        add_test(NAME keys COMMAND test_keys)

        # test_controller: end-to-end MVC wiring — real keystrokes through the
        # View reach the Model via the Controller and refresh the View back.
        add_executable(test_controller tests/test_controller.cpp
            src/calculatorcontroller.cpp src/calculatorview.cpp src/calculatormodel.cpp)
        target_include_directories(test_controller PRIVATE src)
        target_link_libraries(test_controller PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
        add_test(NAME controller COMMAND test_controller)
    endif()
endif()
