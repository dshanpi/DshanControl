QT += core
QT -= gui
CONFIG += console c++11 release
TEMPLATE = app
TARGET = dshancontrol-media-worker
SOURCES += src/main.cpp ../runtime/src/storage.cpp
LIBS += -lcrypto -lz
contains(CONFIG, cedar) {
    DEFINES += DSHAN_CEDAR_PLAYER
    INCLUDEPATH += $$[QT_SYSROOT]/usr/include/libcedarx $$[QT_SYSROOT]/usr/include/libcedarc
    LIBS += -ltplayer -lcdx_base -lrt
}

contains(CONFIG, gstsoftware) {
    QT += network
    CONFIG += link_pkgconfig
    PKGCONFIG += gstreamer-app-1.0 gstreamer-video-1.0
    DEFINES += DSHAN_GSTREAMER_PLAYER
    HEADERS += src/gstworker.h
}
