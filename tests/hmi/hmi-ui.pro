QT += core gui widgets network testlib
CONFIG += c++11 testcase console
TARGET = hmi-ui-test
TEMPLATE = app
SOURCES += hmi_ui_test.cpp ../../components/hmi/src/mainwindow.cpp ../../components/hmi/src/updatepanel.cpp
HEADERS += ../../components/hmi/src/mainwindow.h ../../components/hmi/src/updatepanel.h

RESOURCES += ../../components/hmi/src/icons.qrc

SOURCES += ../../components/hmi/src/visuals.cpp
HEADERS += ../../components/hmi/src/visuals.h
