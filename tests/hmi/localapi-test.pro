QT += core testlib
QT -= gui
CONFIG += console c++11 testcase
TEMPLATE = app
TARGET = localapi-test
SOURCES += localapi-test.cpp
HEADERS += ../../components/hmi/src/localapi.h
