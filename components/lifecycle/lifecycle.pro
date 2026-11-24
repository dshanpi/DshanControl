QT = core network
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = dshan-imx6ull-services
SOURCES += main.cpp ../runtime/src/storage.cpp
HEADERS += ../native/common.h
LIBS += -lz
