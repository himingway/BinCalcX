#pragma once

// App version string. CMake injects BINCALC_VERSION_STRING from project(VERSION)
// (see CMakeLists.txt); the fallback here keeps the qmake build working.
#ifndef BINCALC_VERSION_STRING
#define BINCALC_VERSION_STRING "1.7.1"
#endif
