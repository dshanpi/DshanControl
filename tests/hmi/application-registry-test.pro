QT += core gui testlib
CONFIG += c++11 testcase console
TARGET = application-registry-test
TEMPLATE = app
SOURCES += application_registry_test.cpp
HEADERS += ../../components/hmi/src/desktop/applicationregistry.h
RESOURCES += ../../components/hmi/src/desktop/desktop.qrc
