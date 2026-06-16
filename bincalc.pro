QT += widgets
CONFIG += c++17 release

TARGET = BinCalc
TEMPLATE = app

# Cross-platform: no console window on Windows.
win32: CONFIG += console embed_manifest_exe

SOURCES += \
    src/main.cpp \
    src/calculatormodel.cpp \
    src/calculatorview.cpp \
    src/calculatorcontroller.cpp

HEADERS += \
    src/calculatormodel.h \
    src/calculatorview.h \
    src/calculatorcontroller.h
