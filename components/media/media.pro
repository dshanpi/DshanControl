QT += core
QT -= gui
CONFIG += console c++11 release
TEMPLATE = app
TARGET = dshancontrol-media-worker
SOURCES += src/main.cpp src/surfaceguardian.cpp ../runtime/src/storage.cpp
HEADERS += src/surfaceguardian.h
LIBS += -lcrypto -lz
contains(CONFIG, cedar) {
    DEFINES += DSHAN_CEDAR_PLAYER
    SOURCES += src/cedarsurface.cpp
    INCLUDEPATH += $$[QT_SYSROOT]/usr/include/libcedarx $$[QT_SYSROOT]/usr/include/libcedarc
    LIBS += -ltplayer -lcdx_base -lrt
}
