QT += core
QT -= gui
CONFIG += console c++11 release
CONFIG -= app_bundle
TARGET = storage-test-driver
TEMPLATE = app
SOURCES += storage_test_driver.cpp ../../components/runtime/src/systemstorage.cpp ../../components/runtime/src/storage.cpp
LIBS += -lz
