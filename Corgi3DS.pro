QT += core gui multimedia
greaterThan(QT_MAJOR_VERSION, 4) : QT += widgets

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle

QMAKE_CFLAGS_RELEASE -= -O
QMAKE_CFLAGS_RELEASE -= -O1
QMAKE_CFLAGS_RELEASE *= -O2
QMAKE_CFLAGS_RELEASE -= -O3

SOURCES += main.cpp \
    emulator.cpp \
    arm.cpp \
    arm_interpret.cpp \
    arm_disasm.cpp \
    cp15.cpp \
    thumb_disasm.cpp \
    thumb_interpret.cpp \
    rsa.cpp \
    timers.cpp \
    dma9.cpp \
    pxi.cpp \
    mpcore_pmr.cpp \
    gpu.cpp \
    aes.cpp \
    sha.cpp \
    bswp.cpp \
    rotr.cpp \
    aes_lib.c \
    emmc.cpp \
    interrupt9.cpp \
    emuwindow.cpp

HEADERS += \
    emulator.hpp \
    arm.hpp \
    arm_disasm.hpp \
    arm_interpret.hpp \
    rotr.hpp \
    cp15.hpp \
    rsa.hpp \
    timers.hpp \
    dma9.hpp \
    pxi.hpp \
    mpcore_pmr.hpp \
    gpu.hpp \
    aes.hpp \
    sha.hpp \
    bswp.hpp \
    aes_lib.hpp \
    aes_lib.h \
    emmc.hpp \
    interrupt9.hpp \
    emuwindow.hpp

INCLUDEPATH += /usr/local/include

LIBS += -L/usr/local/lib -lgmpxx -lgmp
