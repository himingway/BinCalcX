QT += widgets
CONFIG += c++17 release

TARGET = BinCalc
TEMPLATE = app

# Cross-platform: no console window on Windows.
win32: CONFIG += embed_manifest_exe

SOURCES += \
    src/main.cpp \
    src/calculatormodel.cpp \
    src/calculatorview.cpp \
    src/calculatorcontroller.cpp

HEADERS += \
    src/calculatormodel.h \
    src/calculatorview.h \
    src/calculatorcontroller.h

# App icon — runtime window icon (Qt resource) on every platform, plus the
# Windows .exe icon via assets/app.rc (resolved relative to this in-tree build).
RESOURCES += assets/app.qrc
win32: RC_FILE = assets/app.rc
