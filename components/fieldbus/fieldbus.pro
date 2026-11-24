QT = core network
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = dshancontrol-fieldbus-worker
SOURCES += main.cpp ../runtime/src/storage.cpp
HEADERS += ../native/common.h ../runtime/src/storage.h
LIBS += -lz
