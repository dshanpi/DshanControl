QT = core network
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = dshancontrol-api
SOURCES += main.cpp http.cpp api.cpp dictionary.cpp store.cpp telemetry.cpp ../runtime/src/storage.cpp
HEADERS += http.h api.h store.h ../native/common.h
LIBS += -lsqlite3 -lcrypto -lz
