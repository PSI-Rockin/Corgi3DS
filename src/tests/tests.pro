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
    ../core/emulator.cpp \
    ../core/cpu/arm.cpp \
    ../core/cpu/arm_interpret.cpp \
    ../core/cpu/arm_disasm.cpp \
    ../core/cpu/cp15.cpp \
    ../core/cpu/thumb_disasm.cpp \
    ../core/cpu/thumb_interpret.cpp \
    ../core/arm9/rsa.cpp \
    ../core/timers.cpp \
    ../core/arm9/dma9.cpp \
    ../core/pxi.cpp \
    ../core/arm11/mpcore_pmr.cpp \
    ../core/arm11/gpu.cpp \
    ../core/arm9/aes.cpp \
    ../core/arm9/sha.cpp \
    ../core/common/bswp.cpp \
    ../core/common/rotr.cpp \
    ../core/arm9/aes_lib.c \
    ../core/arm9/emmc.cpp \
    ../core/arm9/interrupt9.cpp \
    ../core/i2c.cpp \
    ../core/common/exceptions.cpp \
    ../core/cpu/mmu.cpp \
    ../core/scheduler.cpp \
    ../core/cpu/vfp.cpp \
    ../core/cpu/vfp_disasm.cpp \
    ../core/cpu/vfp_interpreter.cpp

HEADERS += \
    ../core/emulator.hpp \
    ../core/cpu/arm.hpp \
    ../core/cpu/arm_disasm.hpp \
    ../core/cpu/arm_interpret.hpp \
    ../core/common/rotr.hpp \
    ../core/cpu/cp15.hpp \
    ../core/arm9/rsa.hpp \
    ../core/timers.hpp \
    ../core/arm9/dma9.hpp \
    ../core/pxi.hpp \
    ../core/arm11/mpcore_pmr.hpp \
    ../core/arm11/gpu.hpp \
    ../core/arm9/aes.hpp \
    ../core/arm9/sha.hpp \
    ../core/common/bswp.hpp \
    ../core/arm9/aes_lib.hpp \
    ../core/arm9/aes_lib.h \
    ../core/arm9/emmc.hpp \
    ../core/arm9/interrupt9.hpp \
    ../core/i2c.hpp \
    ../core/common/common.hpp \
    ../core/common/exceptions.hpp \
    ../core/cpu/mmu.hpp \
    ../core/scheduler.hpp \
    ../core/cpu/vfp.hpp

INCLUDEPATH += /usr/local/include

LIBS += -L/usr/local/lib -lgmpxx -lgmp
