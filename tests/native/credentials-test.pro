QT += core
QT -= gui
CONFIG += console c++11 release
CONFIG -= app_bundle
TARGET = credentials-test-driver
TEMPLATE = app
SOURCES += credentials_test_driver.cpp ../../components/runtime/src/applicationcredentials.cpp ../../components/runtime/src/storage.cpp
LIBS += -lz
