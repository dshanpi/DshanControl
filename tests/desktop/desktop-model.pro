QT += core testlib
QT -= gui
CONFIG += console c++11 testcase
TARGET = desktop-model-test
TEMPLATE = app
SOURCES += model_test.cpp ../../components/desktop/src/model.cpp ../../components/runtime/src/storage.cpp
HEADERS += ../../components/desktop/src/model.h
LIBS += -lcrypto -lz
