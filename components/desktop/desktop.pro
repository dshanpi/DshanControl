QT += core network
QT -= gui
CONFIG += console c++11 release
TEMPLATE = app
TARGET = dshancontrol-desktop-service
SOURCES += src/main.cpp src/model.cpp src/mqtt.cpp src/providers.cpp src/server.cpp \
           ../runtime/src/storage.cpp
HEADERS += src/model.h src/mqtt.h src/providers.h src/server.h
LIBS += -lcrypto -lz
